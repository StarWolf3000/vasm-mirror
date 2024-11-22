/* reloc.c - relocation support functions */
/* (c) in 2010-2016,2020,2022-2024 by Volker Barthelmann and Frank Wille */

#include "vasm.h"


nreloc *new_nreloc(void)
{
  nreloc *new = mymalloc(sizeof(*new));
  new->mask = DEFMASK;
  new->byteoffset = new->bitoffset = new->size = 0;
  new->addend = 0;
  return new;
}


rlist *add_extnreloc(rlist **relocs,symbol *sym,taddr addend,int type,
                     size_t bitoffs,size_t size,size_t byteoffs)
/* add_extnreloc() can specify byteoffset and bitoffset directly.
   Use add_nreloc() for the old interface, which calculates
   byteoffset and bitoffset from offset. */
{
  rlist *rl;
  nreloc *r;

  if (sym->flags & ABSLABEL)
    return NULL;  /* no relocation, when symbol is from an ORG-section */

  /* mark symbol as referenced, so we can find unreferenced imported symbols */
  sym->flags |= REFERENCED;

  r = new_nreloc();
  r->byteoffset = byteoffs;
  r->bitoffset = bitoffs;
  r->size = size;
  r->sym = sym;
  r->addend = addend;
  rl = mymalloc(sizeof(rlist));
  rl->type = type;
  rl->reloc = r;
  rl->next = *relocs;
  *relocs = rl;

  return rl;
}


rlist *add_extnreloc_masked(rlist **relocs,symbol *sym,taddr addend,int type,
                            size_t bitoffs,size_t size,size_t byteoffs,
                            utaddr mask)
/* add_extnreloc_masked() can specify byteoffset and bitoffset directly.
   Use add_nreloc_masked() for the old interface, which calculates
   byteoffset and bitoffset from offset. */
{
  rlist *rl;
  nreloc *r;

  if (rl = add_extnreloc(relocs,sym,addend,type,bitoffs,size,byteoffs)) {
    r = rl->reloc;
    r->mask = mask;
  }
  return rl;
}


int is_pc_reloc(symbol *sym,section *cur_sec)
/* Returns true when the symbol needs a REL_PC relocation to reference it
   pc-relative. This is the case when it is externally defined or a label
   from a different section (so the linker has to resolve the distance). */
{
  if (EXTREF(sym))
    return 1;
  else if (LOCREF(sym))
    return (cur_sec->flags & ABSOLUTE) ? 0 : sym->sec != cur_sec;
  ierror(0);
  return 0;
}


int std_reloc(rlist *rl)
/* return reloc type (without REL_MOD_U/S bits), when rl has a reloc-type
   between FIRST_STANDARD_RELOC and LAST_STANDARD_RELOC, -1 otherwise */
{
  if (rl->type<FIRST_CPU_RELOC &&
      STD_REL_TYPE(rl->type) >= FIRST_STANDARD_RELOC &&
      STD_REL_TYPE(rl->type) <= LAST_STANDARD_RELOC)
    return STD_REL_TYPE(rl->type);
  return -1;
}


void do_pic_check(rlist *rl)
/* generate an error on a non-PC-relative relocation */
{
  nreloc *r;
  int t;

  while (rl) {
    t = rl->type;
    if (t==REL_ABS || t==REL_UABS) {
      r = (nreloc *)rl->reloc;

      if (r->sym->type==LABSYM ||
          (r->sym->type==IMPORT && (r->sym->flags&EXPORT)))
        general_error(34);  /* relocation not allowed */
    }
    rl = rl->next;
  }
}


taddr nreloc_real_addend(nreloc *nrel)
{
  /* In vasm the addend includes the symbol's section offset for LABSYMs */
  if (nrel->sym->type == LABSYM)
    return nrel->addend - nrel->sym->pc;
  return nrel->addend;
}


void unsupp_reloc_error(atom *a,rlist *rl)
{
  if (rl->type < FIRST_CPU_RELOC) {
    nreloc *r = (nreloc *)rl->reloc;

    /* reloc type not supported */
    output_atom_error(4,a,STD_REL_TYPE(rl->type),
                      r->size,(unsigned long)r->mask,
                      r->sym->name,
                      (unsigned long)r->addend);
  }
  else
    output_atom_error(5,a,rl->type);
}


void print_nreloc(FILE *f,nreloc *p,int prtpos)
{
  if (prtpos)
    fprintf(f,"(%u,%u-%u,%#llx,%#llx,",
            (unsigned)p->byteoffset,(unsigned)p->bitoffset,
            (unsigned)(p->bitoffset+p->size-1),
            ULLTADDR(p->mask),ULLTADDR(p->addend));
  else
    fputc('(',f);
  print_symbol(f,p->sym);
  fprintf(f,") ");
}


void print_reloc(FILE *f,rlist *rl)
{
  int type;

  if ((type = std_reloc(rl)) >= 0) {
    static const char *rname[LAST_STANDARD_RELOC+1] = {
      "none","abs","pc","got","gotrel","gotoff","globdat","plt","pltrel",
      "pltoff","sd","uabs","localpc","loadrel","copy","jmpslot","secoff"
    };
    fprintf(f,"r%s",rname[type]);
    print_nreloc(f,rl->reloc,type!=REL_NONE);
  }
#ifdef LAST_CPU_RELOC
  else if (rl->type>=FIRST_CPU_RELOC && rl->type<=LAST_CPU_RELOC)
    cpu_reloc_print(f,rl);
#endif
  else
    fprintf(f,"unknown reloc(%d) ",rl->type);
}


rlist *get_relocs(atom *a)
{
  if (a->type == DATA)
    return a->content.db->relocs;
  else if (a->type == SPACE)
    return a->content.sb->relocs;
  return NULL;
}


static void *get_nreloc_ptr(atom *a,nreloc *nrel)
{
  if (a->type == DATA)
    return (char *)a->content.db->data + OCTETS(nrel->byteoffset);
  else if (a->type == SPACE)
    return (char *)a->content.sb->fill;  /* @@@ ignore offset completely? */
  return NULL;
}


static int reloc_field_overflow(int rtype,size_t numbits,taddr bitval)
/* tests if bitval fits into a relocation field with numbits and std_rtype */
{
  uint64_t val = (int64_t)bitval;

  if (rtype<FIRST_CPU_RELOC && (rtype & REL_MOD_S)) {
    uint64_t mask = ~MAKEMASK(numbits - 1);
    return (bitval < 0) ? (val & mask) != mask : (val & mask) != 0;
  }
  else if (rtype<FIRST_CPU_RELOC && (rtype & REL_MOD_U)) {
    return (val & ~(uint64_t)MAKEMASK(numbits)) != 0;
  }
  else {  /* unspecified, allow full signed and unsigned range */
    uint64_t mask = ~MAKEMASK(numbits - 1);
    return (bitval < 0) ? (val & mask) != mask :
                          (val & ~(uint64_t)MAKEMASK(numbits)) != 0;
  }
}


int patch_nreloc(atom *a,rlist *rl,taddr val,int be)
/* patch relocated value into the atom, when rlist contains an nreloc */
{
  nreloc *nrel;
  char *p;

  if (!is_nreloc(rl)) {
    unsupp_reloc_error(a,rl);
    return 0;
  }
  nrel = (nreloc *)rl->reloc;

  if (reloc_field_overflow(rl->type,nrel->size,val)) {
    output_atom_error(12,a,rl->type,(unsigned long)nrel->mask,
                      nrel->sym->name,(unsigned long)nrel->addend,nrel->size);
    return 0;
  }

  if (p = get_nreloc_ptr(a,nrel)) {
    setbits(be,p,
        ((nrel->bitoffset+nrel->size+(BITSPERBYTE-1))/BITSPERBYTE)*BITSPERBYTE,
        nrel->bitoffset,nrel->size,val);
  }
  return 1;
}
