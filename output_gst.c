/* gst.c Atari GST output driver for vasm */
/* (c) in 2023 by Frank Wille */

#include "vasm.h"
#include "output_gst.h"
#if defined(OUTGST)
static char *copyright="vasm GST output module 0.1 (c) 2023 Frank Wille";

static rlist **sorted_rlist;
static size_t bss_bytes;


static void gst_string(FILE *f,const char *str)
{
  size_t len = strlen(str);

  if (len > 0xff)
    len = 0xff;
  fw8(f,len);
  fwdata(f,str,len);
}


static void gst_source(FILE *f,const char *str)
{
  GSTOUT(f,GST_SOURCE);
  gst_string(f,str);
}


static void gst_define(FILE *f,int id,const char *str)
{
  GSTOUT(f,GST_DEFINE);
  fw16(f,id,1);
  gst_string(f,str);
}


static void gst_section(FILE *f,section *sec)
{
  if (sec->flags & ABSOLUTE) {
    GSTOUT(f,GST_ORG);
    fw32(f,sec->org,1);
  }
  else {
    GSTOUT(f,GST_SECTION);
    fw16(f,-(int)sec->idx,1);
  }
}


static void gst_common(FILE *f,int refid)
{
  GSTOUT(f,GST_COMMON);
  fw16(f,refid,1);
}


static void gst_offset(FILE *f,size_t offs)
{
  GSTOUT(f,GST_OFFSET);
  fw32(f,offs,1);
}


static void gst_xref(FILE *f,int32_t add,unsigned rule,int refid)
{
  GSTOUT(f,GST_XREF);
  fw32(f,add,1);
  fw8(f,rule);
  fw8(f,'+');
  fw16(f,refid,1);
  fw8(f,GSTESC);
}


static void gst_xdef(FILE *f,const char *name,int32_t val,int refid)
{
  GSTOUT(f,GST_XDEF);
  gst_string(f,name);
  fw32(f,val,1);
  fw16(f,refid,1);
}


static void gst_dobss(FILE *f)
{
  if (bss_bytes) {
    gst_offset(f,bss_bytes);
    bss_bytes = 0;
  }
}


static void gst_data(FILE *f,void *data,size_t len)
{
  uint8_t *p = data;

  gst_dobss(f);

  while (len--) {
    if (*p == GSTESC)
      fw8(f,GSTESC);  /* GST escape code has to be doubled */
    fw8(f,*p++);
  }
}

static int define_section_names(FILE *f,section *sec)
{
  int id = 1;

  for (; sec; sec=sec->next) {
    if (!(sec->flags & ABSOLUTE)) {
      gst_define(f,-id,sec->name);  /* sections have negative ids */
      sec->idx = id++;
    }
  }
  return id;
}


static void define_import_names(FILE *f,symbol *sym,int secid)
{
  int id = 1;

  for (; sym; sym=sym->next) {
    if (sym->type == IMPORT) {
      if (sym->flags & COMMON) {
        gst_define(f,-secid,sym->name);  /* define common symbol as section */
        sym->idx = secid++;
      }
      else {
        if (sym->flags & WEAK)
          output_error(10,sym->name);  /* treat weak symbol as global */
        gst_define(f,id,sym->name);
        sym->idx = id++;
      }
    }
    else
      sym->idx = 0;
  }
}


static int offscmp(const void *left,const void *right)
{
  rlist *rl1 = *(rlist **)left;
  rlist *rl2 = *(rlist **)right;

  return (int)((nreloc *)rl1->reloc)->byteoffset
         - (int)((nreloc *)rl2->reloc)->byteoffset;
}


static int get_sorted_rlist(atom *a)
{
  static int max_relocs_per_atom = 0;
  rlist *rl = get_relocs(a);
  int nrel = 0;

  while (rl) {
    if (nrel >= max_relocs_per_atom) {
      max_relocs_per_atom++;
      sorted_rlist = myrealloc(sorted_rlist,
                               max_relocs_per_atom * sizeof(rlist **));
    }
    sorted_rlist[nrel++] = rl;
    rl = rl->next;
  }

  if (nrel > 1)
    qsort(sorted_rlist,nrel,sizeof(rlist *),offscmp);
  return nrel;
}


static void stream_data(FILE *f,section *sec,atom *a,size_t offs,size_t len)
{
  if (a->type == DATA) {
    dblock *db = a->content.db;

    if (offs + len > db->size)
      ierror(0);
    gst_data(f,db->data+offs,len);
  }

  else if (a->type == SPACE) {
    sblock *sb = a->content.sb;

    if (get_sec_type(sec)==S_BSS || (sb->flags & SPC_UNINITIALIZED)) {
      /* uninitialized space - remember number of consecutive bytes */
      bss_bytes += len;
    }
    else {
      /* initialized space with a repeating pattern */
      int i;

      if ((offs % sb->size) || (len % sb->size)
          || offs+len > sb->size*sb->space)
        ierror(0);
      for (i=offs; i<len; i+=sb->size)
        gst_data(f,sb->fill,sb->size);
    }
  }
}


static void stream_section(FILE *f,section *sec)
{
  utaddr pc,npc;
  atom *a;

  gst_section(f,sec);

  for (npc=sec->org,a=sec->first; a; a=a->next) {
    size_t offs = 0;
    int nrel,i;

    pc = fwpcalign(f,a,sec,npc);
    npc = pc + atom_size(a,sec,pc);
    nrel = get_sorted_rlist(a);

    for (i=0; i<nrel; i++) {
      if (sorted_rlist[i]->type <= LAST_STANDARD_RELOC) {
        nreloc *r = (nreloc *)sorted_rlist[i]->reloc;
        size_t roffs = r->byteoffset;
        uint8_t rule = r->size >> 3;
        symbol *sym = r->sym;

        stream_data(f,sec,a,offs,roffs-offs);
        offs = roffs;

        if (r->bitoffset==0 && r->mask==DEFMASK && !(r->size&7)
            && (rule==1 || rule==2 || rule==4)) {
          int refid;

          if (sym->type == IMPORT)
            refid = (sym->flags & COMMON) ? -(int)sym->idx : sym->idx;
          else if (sym->type==LABSYM && sym->sec)
            refid = -(int)sym->sec->idx;
          else
            ierror(0);

          switch (sorted_rlist[i]->type) {
            case REL_ABS:
              rule |= GSTR_UNSIGNED | GSTR_ABS;
              break;
            case REL_PC:
              rule |= GSTR_SIGNED | GSTR_PCREL;
              break;
            default:
              unsupp_reloc_error(sorted_rlist[i]);
              break;
          }
          gst_xref(f,r->addend,rule,refid);
          offs += rule & 7;  /* reloc field size */
        }
        else
          unsupp_reloc_error(sorted_rlist[i]);
      }
      else
        unsupp_reloc_error(sorted_rlist[i]);
    }
    stream_data(f,sec,a,offs,(npc-pc)-offs);
  }
  gst_dobss(f);  /* write last uninitialized block with GST_OFFSET */
}


static void common_symbols(FILE *f,symbol *sym)
{
  for (; sym; sym=sym->next) {
    if (sym->type==IMPORT && (sym->flags & COMMON)) {
      gst_common(f,-(int)sym->idx);
      gst_offset(f,get_sym_size(sym));
    }
  }
}


static void export_symbols(FILE *f,symbol *sym)
{
  for (; sym; sym=sym->next) {
    if ((sym->type==LABSYM || sym->type==EXPRESSION) &&
        !(sym->flags & VASMINTERN) && (sym->flags & EXPORT)) {
      gst_xdef(f,sym->name,get_sym_value(sym),
               (sym->type==LABSYM&&sym->sec!=NULL)?-(int)sym->sec->idx:0);
    }
  }
}


static void write_output(FILE *f,section *sec,symbol *sym)
{
  gst_source(f,filename);
  define_import_names(f,sym,define_section_names(f,sec));
  for (; sec; sec=sec->next)
    stream_section(f,sec);
  common_symbols(f,sym);
  export_symbols(f,sym);
  GSTOUT(f,GST_END);
}


static int output_args(char *p)
{
  return 0;
}


int init_output_gst(char **cp,void (**wo)(FILE *,section *,symbol *),
                    int (**oa)(char *))
{
  if (bitsperbyte != 8) {
    output_error(1, cpuname); /* output module doesn't support (cpuname) */
    return 0;
  }
  *cp = copyright;
  *wo = write_output;
  *oa = output_args;
  return 1;
}

#else

int init_output_gst(char **cp,void (**wo)(FILE *,section *,symbol *),
                    int (**oa)(char *))
{
  return 0;
}
#endif
