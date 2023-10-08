/* tos.c Atari TOS executable output driver for vasm */
/* (c) in 2009-2016,2020,2021,2023 by Frank Wille */

#include "vasm.h"
#include "output_tos.h"
#if defined(OUTTOS) && defined(VASM_CPU_M68K)
static char *copyright="vasm tos output module 2.3 (c) 2009-2016,2020,2021,2023 Frank Wille";
int tos_hisoft_dri = 1;
int sozobonx_dri;

static int tosflags,textbasedsyms;
static int max_relocs_per_atom;
static section *sections[3];
static utaddr secsize[3];
static utaddr secoffs[3];
static utaddr sdabase,lastoffs;
static rlist **sorted_rlist;

#define SECT_ALIGN 2  /* TOS sections have to be aligned to 16 bits */



static int tos_initwrite(section *sec,symbol *sym)
{
  int nsyms,i;

  if (!exec_out || sozobonx_dri)
    tos_hisoft_dri = 0;  /* 8 character symbols only, in object files */

  /* find exactly one text, data and bss section for DRI */
  sections[S_TEXT] = sections[S_DATA] = sections[S_BSS] = NULL;
  secsize[S_TEXT] = secsize[S_DATA] = secsize[S_BSS] = 0;

  for (; sec; sec=sec->next) {
    if (get_sec_size(sec) > 0 || (sec->flags & HAS_SYMBOLS)) {
      i = get_sec_type(sec);
      if (i<S_TEXT || i>S_BSS) {
        output_error(3,sec->attr);  /* section attributes not supported */
        i = S_TEXT;
      }
      if (!sections[i]) {
        sections[i] = sec;
        secsize[i] = (get_sec_size(sec) + SECT_ALIGN - 1) /
                     SECT_ALIGN * SECT_ALIGN;
        sec->idx = i;  /* section index 0:text, 1:data, 2:bss */
      }
      else
        output_error(7,sec->name);
    }
  }

  max_relocs_per_atom = 1;
  secoffs[S_TEXT] = 0;
  secoffs[S_DATA] = secsize[S_TEXT] + balign(secsize[S_TEXT],SECT_ALIGN);
  secoffs[S_BSS] = secoffs[S_DATA] + secsize[S_DATA] +
                  balign(secsize[S_DATA],SECT_ALIGN);
  /* define small data base as .data+32768 @@@FIXME! */
  sdabase = secoffs[S_DATA] + 0x8000;

  /* count symbols */
  nsyms = sozobonx_dri ? 1 : 0;  /* first symbol may be "SozobonX" */
  for (; sym; sym=sym->next) {
    /* ignore symbols preceded by a '.' and internal symbols */
    if (*sym->name!='.' && *sym->name!=' ' && !(sym->flags & VASMINTERN)) {
      if (sym->flags & WEAK)
        output_error(10,sym->name);  /* weak symbol treated as global */

      if (!exec_out || (!(sym->flags & COMMON) &&
          (sym->type==LABSYM || sym->type==EXPRESSION))) {
        sym->idx = (unsigned long)nsyms++;
        if (strlen(sym->name) > DRI_NAMELEN) {
          if (sozobonx_dri)
            nsyms += (strlen(sym->name) - 1) / DRI_NAMELEN;
          else if (tos_hisoft_dri)
            nsyms++;  /* one extra symbol for long name */
        }
      }
    }
    else {
      if (!strcmp(sym->name," TOSFLAGS")) {
        if (tosflags == 0)  /* not defined by command line? */
          tosflags = (int)get_sym_value(sym);
      }
      sym->flags |= VASMINTERN;
    }
  }
  return (exec_out && no_symbols) ? 0 : nsyms;
}


static void tos_header(FILE *f,unsigned long tsize,unsigned long dsize,
                        unsigned long bsize,unsigned long ssize,
                        unsigned long flags)
{
  PH hdr;

  setval(1,hdr.ph_branch,2,0x601a);
  setval(1,hdr.ph_tlen,4,tsize);
  setval(1,hdr.ph_dlen,4,dsize);
  setval(1,hdr.ph_blen,4,bsize);
  setval(1,hdr.ph_slen,4,ssize);
  setval(1,hdr.ph_magic,4,0);
  setval(1,hdr.ph_flags,4,flags);
  setval(1,hdr.ph_abs,2,0);
  fwdata(f,&hdr,sizeof(PH));
}


static void checkdefined(rlist *rl,section *sec,taddr pc,atom *a)
{
  if (rl->type <= LAST_STANDARD_RELOC) {
    nreloc *r = (nreloc *)rl->reloc;

    if (EXTREF(r->sym))
      output_atom_error(8,a,r->sym->name,sec->name,
                        (unsigned long)pc+r->byteoffset,rl->type);
  }
  else
    ierror(0);
}


static taddr tos_sym_value(symbol *sym,int textbased)
{
  taddr val = get_sym_value(sym);

  /* all sections form a contiguous block, so add section offset */
  if (textbased && sym->type==LABSYM && sym->sec!=NULL)
    val += secoffs[sym->sec->idx];

  return val;
}


static void do_relocs(section *asec,taddr pc,atom *a)
/* Try to resolve all relocations in a DATA or SPACE atom.
   Very simple implementation which can only handle basic 68k relocs. */
{
  rlist *rl = get_relocs(a);
  int rcnt = 0;
  section *sec;

  while (rl) {
    switch (rl->type) {
      case REL_SD:
        checkdefined(rl,asec,pc,a);
        patch_nreloc(a,rl,1,
                     (tos_sym_value(((nreloc *)rl->reloc)->sym,1)
                      + nreloc_real_addend(rl->reloc)) - sdabase,1);
        break;
      case REL_PC:
        checkdefined(rl,asec,pc,a);
        patch_nreloc(a,rl,1,
                     (tos_sym_value(((nreloc *)rl->reloc)->sym,1)
                     + nreloc_real_addend(rl->reloc)) -
                     (pc + ((nreloc *)rl->reloc)->byteoffset),1);
        break;
      case REL_ABS:
        checkdefined(rl,asec,pc,a);
        sec = ((nreloc *)rl->reloc)->sym->sec;
        if (!patch_nreloc(a,rl,0,
                          secoffs[sec?sec->idx:0] +
                          ((nreloc *)rl->reloc)->addend,1))
          break;  /* field overflow */
        if (((nreloc *)rl->reloc)->size == 32)
          break;  /* only support 32-bit absolute */
      default:
        unsupp_reloc_error(rl);
        break;
    }
    rcnt++;
    if (a->type == SPACE)
      break;  /* all SPACE relocs are identical, one is enough */
    rl = rl->next;
  }

  if (rcnt > max_relocs_per_atom)
    max_relocs_per_atom = rcnt;
}


static void tos_writesection(FILE *f,section *sec,taddr sec_align)
{
  if (sec) {
    utaddr pc = secoffs[sec->idx];
    utaddr npc;
    atom *a;

    for (a=sec->first; a; a=a->next) {
      npc = fwpcalign(f,a,sec,pc);
      if (exec_out)
        do_relocs(sec,npc,a);
      if (a->type == DATA)
        fwdata(f,a->content.db->data,a->content.db->size);
      else if (a->type == SPACE)
        fwsblock(f,a->content.sb);
      pc = npc + atom_size(a,sec,npc);
    }
    fwalign(f,pc,sec_align);
  }
}


static void write_dri_sym(FILE *f,const char *name,int type,taddr value)
{
  struct DRIsym stab;
  int namelen = strlen(name);
  int longname = (namelen > DRI_NAMELEN) && tos_hisoft_dri;
  int szb_extensions = sozobonx_dri ? (namelen-1) / DRI_NAMELEN : 0;

  strncpy(stab.name,name,DRI_NAMELEN);
  setval(1,stab.type,sizeof(stab.type),longname?(type|STYP_LONGNAME):type);
  setval(1,stab.value,sizeof(stab.value),value);
  fwdata(f,&stab,sizeof(struct DRIsym));

  if (longname) {
    char rest_of_name[sizeof(struct DRIsym)];

    memset(rest_of_name,0,sizeof(struct DRIsym));
    strncpy(rest_of_name,name+DRI_NAMELEN,sizeof(struct DRIsym));
    fwdata(f,rest_of_name,sizeof(struct DRIsym));
  }
  else {
    int i = DRI_NAMELEN;

    while (szb_extensions--) {
      strncpy(stab.name,name+i,DRI_NAMELEN);
      setval(1,stab.type,sizeof(stab.type),STYP_XFLAGS);
      setval(1,stab.value,sizeof(stab.value),XVALUE);
      fwdata(f,&stab,sizeof(struct DRIsym));
      i += DRI_NAMELEN;
    }
  }
}


static const int labtype[] = { STYP_TEXT,STYP_DATA,STYP_BSS };


static void tos_symboltable(FILE *f,symbol *sym)
{
  int t;

  for (; sym; sym=sym->next) {
    /* The TOS symbol table in executables contains all labels and
       equates, no matter if global or local. */
    if (!(sym->flags & (VASMINTERN|COMMON)) &&
        (sym->type==LABSYM || sym->type==EXPRESSION)) {
      t = (sym->type==LABSYM ? labtype[sym->sec->idx] : STYP_EQUATED) |
          STYP_DEFINED;
      if (sym->flags & EXPORT)
        t |= STYP_GLOBAL;
      write_dri_sym(f,sym->name,t,tos_sym_value(sym,textbasedsyms));
    }
  }
}


static void dri_symboltable(FILE *f,symbol *sym)
{
  int t;

  for (; sym; sym=sym->next) {
    if (!(sym->flags & VASMINTERN)) {
      switch (sym->type) {
        case LABSYM:
          t = labtype[sym->sec->idx] | STYP_DEFINED;
          break;
        case IMPORT:
          t = STYP_EXTERNAL | STYP_DEFINED;
          break;
        case EXPRESSION:
          t = STYP_EQUATED | STYP_DEFINED;
          break;
        default:
          ierror(0);
      }
      if ((sym->flags & EXPORT) && sym->type!=IMPORT)
        t |= STYP_GLOBAL;

      if (sym->flags & COMMON)
        write_dri_sym(f,sym->name,t,get_sym_size(sym));
      else
        write_dri_sym(f,sym->name,t,get_sym_value(sym));
    }
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


static int tos_writerelocs(FILE *f,section *sec)
{
  int n = 0;

  if (sec) {
    utaddr pc = secoffs[sec->idx];
    atom *a;

    for (a=sec->first; a; a=a->next) {
      int nrel;

      pc = pcalign(a,pc);

      if (nrel = get_sorted_rlist(a)) {
        utaddr newoffs;
        int i;

        /* write differences between reloc offsets */
        for (i=0; i<nrel; i++) {
          /* make sure to process 32-bit absolute relocations only! */
          if (sorted_rlist[i]->type==REL_ABS
              && ((nreloc *)sorted_rlist[i]->reloc)->size==32) {
            newoffs = pc + ((nreloc *)sorted_rlist[i]->reloc)->byteoffset;
            n++;

            if (lastoffs) {
              /* determine 8bit difference to next relocation */
              taddr diff = newoffs - lastoffs;

              if (diff < 0)
                ierror(0);
              while (diff > 254) {
                fw8(f,1);
                diff -= 254;
              }
              fw8(f,(uint8_t)diff);
            }
            else  /* first entry is a 32 bits offset */
              fw32(f,newoffs,1);
            lastoffs = newoffs;
          }
        }
      }
      pc += atom_size(a,sec,pc);
    }
  }

  return n;
}


static void dri_writerelocs(FILE *f,section *sec,taddr sec_align)
{
  static const uint16_t sect_relocs[] = { 2, 1, 3 };  /* text, data, bss */

  if (sec) {
    utaddr pc,npc;
    atom *a;

    /* The DRI reloc table has the same size as the section itself,
       but all words which have a relocation (or external reference)
       are indicated by a reloc-type in the least significant three
       bits (0-7). The remaining 13 bits are used as an optional index
       into the symbol table. */
    for (npc=0,a=sec->first; a; a=a->next) {
      size_t offs = 0;
      int nrel,i;

      pc = fwpcalign(f,a,sec,npc);
      npc = pc + atom_size(a,sec,pc);
      nrel = get_sorted_rlist(a);

      for (i=0; i<nrel; i++) {
        if (sorted_rlist[i]->type <= LAST_STANDARD_RELOC) {
          nreloc *r = (nreloc *)sorted_rlist[i]->reloc;
          size_t roffs = r->byteoffset;
          symbol *sym = r->sym;
          uint16_t t = 0;

          fwspace(f,roffs-offs);
          offs = roffs;

          switch (sorted_rlist[i]->type) {
            case REL_ABS:
              if (r->size==16 || r->size==32) {    /* @@@ restrict to 32? */
                if (sym->type==LABSYM && sym->sec)
                  t = sect_relocs[sym->sec->idx];  /* text, data, bss reloc */
                else if (sym->type == IMPORT)
                  t = 4;                           /* absolute xref */
              }
              break;
            case REL_PC:
              if ((sym->type==LABSYM || sym->type==IMPORT)
                  && r->size==16)
                t = 6;                             /* PC-rel xref always 16-bit */
              break;
            case REL_SD:  /* uses normal ABS reloc type with word-size */
              if ((sym->type==LABSYM || sym->type==IMPORT)
                  && r->size==16)
                t = 4;                             /* Baserel is always 16-bit */
              break;
          }

          if (t!=0 && r->bitoffset==0 && r->mask==DEFMASK) {
            /* DRI only supports 16- and 32-bit relocations */
            if (r->size == 32) {
              fw16(f,5,1);  /* write longword type indicator to MSW */
              offs += 2;
            }
            if (t==4 || t==6) {
              /* external reference requires symbol index in bits 3..15 */
              if (sym->idx > 0x1fff)
                output_error(19);  /* too many symbols */
              t |= sym->idx << 3;
            }
            fw16(f,t,1);
            offs += 2;
          }
          else
            unsupp_reloc_error(sorted_rlist[i]);
        }
        else
          unsupp_reloc_error(sorted_rlist[i]);
      }
      fwspace(f,(size_t)(npc-pc)-offs);
    }
    fwalign(f,pc,sec_align);
  }
}


static void write_output(FILE *f,section *sec,symbol *sym)
{
  int nsyms = tos_initwrite(sec,sym);
  int nrelocs = 0;

  if (sozobonx_dri!=0 && nsyms==1)
    nsyms = 0;

  tos_header(f,secsize[S_TEXT],secsize[S_DATA],secsize[S_BSS],
             nsyms*sizeof(struct DRIsym),exec_out?tosflags:0);

  tos_writesection(f,sections[S_TEXT],SECT_ALIGN);
  tos_writesection(f,sections[S_DATA],SECT_ALIGN);

  if (nsyms) {
    if (sozobonx_dri)
      write_dri_sym(f,XNAME,STYP_XFLAGS,XVALUE);
    if (exec_out)
      tos_symboltable(f,sym);
    else
      dri_symboltable(f,sym);
  }

  sorted_rlist = mymalloc(max_relocs_per_atom*sizeof(rlist **));

  if (exec_out) {
    nrelocs += tos_writerelocs(f,sections[S_TEXT]);
    nrelocs += tos_writerelocs(f,sections[S_DATA]);
    if (nrelocs)
      fw8(f,0);
    else
      fw32(f,0,1);
  }
  else {
    dri_writerelocs(f,sections[S_TEXT],SECT_ALIGN);
    dri_writerelocs(f,sections[S_DATA],SECT_ALIGN);
  }

  myfree(sorted_rlist);
}


static int output_args(char *p)
{
  if (!strncmp(p,"-tos-flags=",11)) {
    tosflags = atoi(p+11);
    return 1;
  }
  else if (!strcmp(p,"-monst")) {
    textbasedsyms = 1;
    return 1;
  }
  else if (!strcmp(p,"-stdsymbols")) {
    tos_hisoft_dri = 0;
    return 1;
  }
  else if (!strcmp(p,"-szbx")) {
    sozobonx_dri = 1;
    return 1;
  }
  return 0;
}


int init_output_tos(char **cp,void (**wo)(FILE *,section *,symbol *),
                    int (**oa)(char *))
{
  *cp = copyright;
  *wo = write_output;
  *oa = output_args;
  unnamed_sections = 1;  /* output format doesn't support named sections */
  secname_attr = 1;
  return 1;
}

#else

int init_output_tos(char **cp,void (**wo)(FILE *,section *,symbol *),
                    int (**oa)(char *))
{
  return 0;
}
#endif
