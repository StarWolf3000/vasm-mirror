/* aof.c Acorn/ARM Object Format output driver for vasm */
/* (c) in 2025,2026 by Frank Wille */

#include "vasm.h"
#if defined(OUTAOF) && defined(VASM_CPU_ARM)
#include "output_aof.h"
static char *copyright="vasm AOF output module 0.2 (c) 2025,2026 Frank Wille";

/* chunks */
static struct aof_chunk *first_chunk,*last_chunk;
static size_t chunk_offset = 4*3 + CHUNKS_RESERVED*16;
static int num_chunks;

/* OBJ_HEAD */
static struct area_hdr *first_area,*last_area;

/* OBJ_AREA */
static uint32_t area_chunksize;

/* OBJ_SYMT */
static struct aof_symbol *aofsym_first,*aofsym_last;
static int aofsym_cnt;

/* OBJ_STRT */
static hashtable *strtab_hash;
static struct aof_strtab *strtab_first,*strtab_last;
static uint32_t strtab_size = 4;


static uint32_t add_strt(const char *name)
{
  hashdata data;

  if (!find_name(strtab_hash,name,&data)) {
    struct aof_strtab *new = mymalloc(sizeof(struct aof_strtab));

    new->next = NULL;
    new->str = name;
    new->offs = strtab_size;
    strtab_size += strlen(name) + 1;
    if (strtab_last) {
      strtab_last->next = new;
      strtab_last = new;
    }
    else
      strtab_first = strtab_last = new;
    data.ptr = new;
    add_hashentry(strtab_hash,name,data);
  }
  return ((struct aof_strtab *)data.ptr)->offs;
}


static void add_areahdr(struct area_hdr *hdr)
{
  struct area_hdr *new = mymalloc(sizeof(struct area_hdr));

  *new = *hdr;
  new->next = NULL;

  if (last_area) {
    last_area->next = new;
    last_area = new;
  }
  else
    first_area = last_area = new;

  if (!(hdr->attr_align & AA_NOINIT))
    area_chunksize += hdr->aligned_size + hdr->num_relocs * 8;
}


static void bad_reloc(atom *a,int type,nreloc *r)
{
  output_atom_error(4,a,reloc_name(type),
                    r->size,(unsigned long)r->mask,
                    r->sym->name,
                    (unsigned long)r->addend);
}


static struct aof_reloc *new_aof_reloc(int type,atom *a,uint32_t offs,
                                       nreloc *r1,nreloc *r2)
{
  struct aof_reloc *ar = mymalloc(sizeof(struct aof_reloc));
  size_t isz;

  ar->offset = offs;

  switch (type) {
    case REL_ABS: ar->type = 0; break;
    case REL_PC: ar->type = 1; break;
    case REL_SD: ar->type = 2; break;
    default:
      bad_reloc(a,type,r1);
      return NULL;
  }

  /* determine possible instruction size, can be multipe instructions */
  isz = a->type==DATA ? a->content.db->size : 0;

  if (isz>=2 && (r2!=NULL || (r1->size!=16 && r1->size!=32))) {
    /* potential ARM or Thumb instruction relocation */
    ar->ft = 3;

    if (r1->size==11 || r1->size==5 ||
        (r1->size==8 && r2==NULL && type==REL_ABS))
      ar->offset |= 1;  /* offset bit 0 indicates Thumb relocation */
    ar->ii = r2!=NULL ? 2 : 1;  /* number of instructions affected */
  }
  else {
    /* 8, 16 or 32 bit data relocation */
    if (r1->byteoffset!=0 || r1->bitoffset!=0 || r1->mask!=DEFMASK)
      r1->size = 0;  /* bad reloc */
    switch (r1->size) {
      case 8: ar->ft = 0; break;
      case 16: ar->ft = 1; break;
      case 32: ar->ft = 2; break;
      default:
        bad_reloc(a,type,r1);
        return NULL;
    }
    ar->ii = 0;
  }

  if (r1->sym->type == IMPORT) {
    /* external reference, use symbol id in SID */
    ar->a = 1;
    ar->ref.sym = r1->sym;
  }
  else {
    /* local, section-based relocation, usea area id in SID */
    ar->a = 0;
    if (r1->sym==NULL || r1->sym->sec==NULL)
      ierror(0);
    ar->ref.sec = r1->sym->sec;
  }
  return ar;
}


static int make_areas(section *sec)
{
  int sec_idx;

  for (sec_idx=0; sec; sec=sec->next) {
    struct area_hdr hdr;
    int thumb;
    utaddr pc;
    atom *a;
    char *p;

    hdr.sec = sec;
    hdr.name_offs = add_strt(sec->name);
    hdr.attr_align = sec->align < 2 ? 2 : (sec->align & 0xff);
    hdr.num_relocs = 0;
    hdr.relocs = NULL;

    /* scan for relocs and store them as AOF relocs */
    for (pc=sec->org,thumb=-1,a=sec->first; a!=sec->end; a=a->next) {
      rlist *rl;
      int type;

      pc = pcalign(a,pc);

      for (rl=get_relocs(a); rl!=NULL; rl=rl->next) {
        if ((type = std_reloc(rl)) >= 0) {
          struct aof_reloc *ar;
          struct nreloc *nrel1=rl->reloc;
          struct nreloc *nrel2;

          /* check if two vasm relocs form one ARM reloc */
          if (rl->next!=NULL && rl->next->type==type) {
            nrel2 = rl->next->reloc;
            if (nrel1->byteoffset==nrel2->byteoffset &&
                nrel1->addend==nrel2->addend &&
                (nrel1->bitoffset!=nrel2->bitoffset ||
                 nrel1->mask!=nrel2->mask))
              rl = rl->next;  /* these two nrelocs belong together */
            else
              nrel2 = NULL;
          }
          else
            nrel2 = NULL;

          /* add new AOF reloc record to the area */
          if (ar = new_aof_reloc(type,a,pc-sec->org,nrel1,nrel2)) {
            hdr.num_relocs++;
            ar->next = hdr.relocs;
            hdr.relocs = ar;

            if (ar->ft == 3) {
              /* ARM or Thumb instruction relocs in this section? */
              if (thumb < 0)
                thumb = (ar->offset & 1) ? 1 : 0;
              else if (thumb && !(ar->offset & 1))
                thumb = 0;
            }
          }
        }
      }
      pc += atom_size(a,sec,pc);
    }

    /* store 32-bit area size, aligned to the next multiple of 4 bytes */
    if (pc - sec->org <= 0xfffffffc)
      hdr.aligned_size = ((pc - sec->org) + 3) & ~3;
    else
      output_error(23,sec->name,0xfffffffc,pc-sec->org); /* max size exceeded */

    /* convert section attributes */
    hdr.attr_align |= AA_READONLY;
    for (p=sec->attr; *p; p++) {
      switch (*p) {
        case 'c':
        case 'x':
          hdr.attr_align |= AA_CODE;
          break;
        case 'w':
          hdr.attr_align &= ~AA_READONLY;
          break;
        case 'u':
          hdr.attr_align |= AA_NOINIT;
          break;
      }
    }
    if (hdr.attr_align & AA_CODE) {
      if (cpu_type & ~AA2)
        hdr.attr_align |= AA_APCS32;  /* 32 bits since AA3 */
      if (cpu_type & AA4TUP)
        hdr.attr_align |= AA_HALFRELOC;  /* set for ARM7TDMI and higher @@@? */
      if (thumb > 0)
        hdr.attr_align |= AA_THUMBRELOC;  /* contains only Thumb relocs */
    }
    if (sec->flags & ABSOLUTE) {
      hdr.base_addr = sec->org;
      hdr.attr_align |= AA_ABSOLUTE;  /* not really supported by most linkers */
    }
    else {
      hdr.base_addr = 0;
      if (hdr.num_relocs == 0)  /* @@@ PC/Base relocs should be ok? */
        hdr.attr_align |= AA_PIC;
    }

    add_areahdr(&hdr);
    sec->idx = sec_idx++;
  }
  return sec_idx;  /* number of areas created */
}


static int add_symt(uint32_t sname,uint32_t attr,uint32_t val,uint32_t aname)
{
  struct aof_symbol *asym = mymalloc(sizeof(struct aof_symbol));

  asym->next = NULL;
  asym->symname = sname;
  asym->attr = attr;
  asym->value = val;
  asym->areaname = aname;
  if (aofsym_last) {
    aofsym_last->next = asym;
    aofsym_last = asym;
  }
  else
    aofsym_first = aofsym_last = asym;
  return aofsym_cnt++;
}


static void make_symt(symbol *sym)
{
  for (; sym; sym=sym->next) {
    if (!(sym->flags & VASMINTERN) && *sym->name!=' ') {
      uint32_t val = get_sym_value(sym);
      uint32_t attr=0,area=0;

      if (sym->type == IMPORT) {
        attr = AOFSYM_EXTERN;
        if (nocase)
          attr |= AOFSYM_NOCASE;
        if (sym->flags & COMMON) {
          attr |= AOFSYM_COMMON;
          val = get_sym_size(sym);
        }
        else if (sym->flags & WEAK)
          attr |= AOFSYM_WEAK;
      }
      else {
        attr = (sym->flags & EXPORT) ? AOFSYM_GLOBAL : AOFSYM_LOCAL;
        if (sym->type == LABSYM) {
          area = add_strt(sym->sec->name);
        }
        else if (sym->type == EXPRESSION) {
          attr |= AOFSYM_ABS;
        }
        else
          continue;  /* unknown symbol type */
      }

      sym->idx = add_symt(add_strt(sym->name),attr,val,area);
    }
  }
}


static char *make_idfn(void)
{
  size_t len = strlen(vasmname);
  char *id = mymalloc(len + strlen(cpuname) + 2);

  sprintf(id,"%s %s",vasmname,cpuname);
  return id;
}


static void add_chunk(const char *id,size_t len)
{
  struct aof_chunk *ch = mymalloc(sizeof(struct aof_chunk));

  if (strlen(id)!=8 || (len&3))
    ierror(0);

  /* HEAD chunk is always first in list (for compatibility) */
  if (!strcmp(id,"OBJ_HEAD")) {
    ch->next = first_chunk;
    first_chunk = ch;
  }
  else {
    ch->next = NULL;
    if (last_chunk) {
      last_chunk->next = ch;
      last_chunk = ch;
    }
    else
      first_chunk = last_chunk = ch;
  }

  memcpy(ch->id,id,8);
  ch->offs = chunk_offset;
  ch->size = len;
  chunk_offset += len;
  num_chunks++;
}


static void write_chunk_header(FILE *f,int en)
{
  struct aof_chunk *ch;
  int i;

  fw32(f,CHUNK_FILE_ID,en);
  fw32(f,CHUNKS_RESERVED,en);
  fw32(f,num_chunks,en);
  for (i=0,ch=first_chunk; ch; i++,ch=ch->next) {
    fwdata(f,ch->id,8);
    fw32(f,ch->offs,en);
    fw32(f,ch->size,en);
  }
  if (i!=num_chunks || num_chunks>CHUNKS_RESERVED)
    ierror(0);
  fwspace(f,(CHUNKS_RESERVED-num_chunks)*16);  /* clear remaining slots */
}


static void write_areas(FILE *f,int en,struct area_hdr *area,int areacnt)
{
  for (; area; area=area->next,areacnt--) {
    if (!(area->attr_align & AA_NOINIT)) {
      section *sec = area->sec;
      struct aof_reloc *r;
      utaddr pc;
      atom *a;
      int rcnt;

      /* section data */
      for (pc=area->base_addr,a=sec->first; a!=sec->end; a=a->next) {
        pc = pcalign(a,pc);
        if (a->type == DATA)
          fwdblock(f,a->content.db);
        else if (a->type == SPACE)
          fwsblock(f,a->content.sb);
        pc += atom_size(a,sec,pc);
      }
      if ((uint32_t)pc-area->base_addr > area->aligned_size)
        ierror(0);
      fwspace(f,area->aligned_size-((utaddr)pc-area->base_addr));

      /* type 2 section relocs */
      for (r=area->relocs,rcnt=0; r; r=r->next,rcnt++) {
        uint32_t info = 0;
        symbol *sym;
        section *sec;

        fw32(f,r->offset,en);
        if (r->a) {
          if (sym = r->ref.sym)
            info = sym->idx & 0xffffff;
        }
        else {
          if (sec = r->ref.sec)
            info = sec->idx & 0xffffff;
        }
        info |= ((r->ft&3)<<24) | ((r->type&1)<<26) | ((r->a&1)<<27) |
                ((r->type&2)<<27) | ((r->ii&3)<<29);
        fw32(f,0x80000000|info,en);  /* write type-2 reloc info */
      }
      if (rcnt != (int)area->num_relocs)
        ierror(0);
    }
  }
  if (areacnt)
    ierror(0);
}


static void write_symbols(FILE *f,int en,struct aof_symbol *asym,int symcnt)
{
  for (; asym; asym=asym->next,symcnt--) {
    fw32(f,asym->symname,en);
    fw32(f,asym->attr,en);
    fw32(f,asym->value,en);
    fw32(f,asym->areaname,en);
  }
  if (symcnt)
    ierror(0);
}


static void write_strtab(FILE *f,int en,struct aof_strtab *strt,size_t totlen)
{
  size_t len;
  uint32_t offs;

  fw32(f,totlen,en);
  for (offs=4; strt; strt=strt->next) {
    if (strt->offs != offs)
      ierror(0);
    len = strlen(strt->str) + 1;
    fwdata(f,strt->str,len);
    offs += len;
  }
  if (offs != totlen)
    ierror(0);
  fwalign(f,totlen,4);
}


static void write_obj_head(FILE *f,int en,struct area_hdr *ahdr,int areacnt)
{
  fw32(f,AOF_RELOC_OBJ,en);
  fw32(f,AOF_VERSION,en);
  fw32(f,areacnt,en);
  fw32(f,aofsym_cnt,en);
  fw32(f,0,en);  /* @@@ entry area index */
  fw32(f,0,en);  /* @@@ entry area offset */

  /* output area headers */
  for (; ahdr; ahdr=ahdr->next,areacnt--) {
    fw32(f,ahdr->name_offs,en);
    fw32(f,ahdr->attr_align,en);
    fw32(f,ahdr->aligned_size,en);
    fw32(f,ahdr->num_relocs,en);
    fw32(f,ahdr->base_addr,en);
  }
  if (areacnt)
    ierror(0);
}


static void write_output(FILE *f,section *sec,symbol *sym)
{
  char *idfn_str;
  size_t idfn_len;
  int num_areas;

  strtab_hash = new_hashtable_c(AOFSTRHTABSIZE);
  idfn_str = make_idfn();
  idfn_len = strlen(idfn_str) + 1;

  num_areas = make_areas(sec);
  make_symt(sym);

  if (area_chunksize)
    add_chunk("OBJ_AREA",area_chunksize);
  add_chunk("OBJ_IDFN",(idfn_len+3)&~3);
  if (aofsym_cnt)
    add_chunk("OBJ_SYMT",aofsym_cnt*16);
  if (strtab_size > 4)
    add_chunk("OBJ_STRT",(strtab_size+3)&~3);
  add_chunk("OBJ_HEAD",4*(6+num_areas*5));
  write_chunk_header(f,BIGENDIAN);

  if (area_chunksize)
    write_areas(f,BIGENDIAN,first_area,num_areas);
  fwdata(f,idfn_str,idfn_len);
  fwalign(f,idfn_len,4);
  if (aofsym_cnt)
    write_symbols(f,BIGENDIAN,aofsym_first,aofsym_cnt);
  if (strtab_size > 4)
    write_strtab(f,BIGENDIAN,strtab_first,strtab_size);
  write_obj_head(f,BIGENDIAN,first_area,num_areas);
}


static int output_args(char *p)
{
  return 0;
}


int init_output_aof(char **cp,void (**wo)(FILE *,section *,symbol *),
                    int (**oa)(char *))
{
  *cp = copyright;
  *wo = write_output;
  *oa = output_args;
  return 1;
}

#else

int init_output_aof(char **cp,void (**wo)(FILE *,section *,symbol *),
                    int (**oa)(char *))
{
  return 0;
}
#endif
