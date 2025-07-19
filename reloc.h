/* reloc.h  reloc header file for vasm */
/* (c) in 2002,2005-11,2016,2023-2025 by Volker Barthelmann and Frank Wille */

#ifndef RELOC_H
#define RELOC_H

/* part of standard relocations */

#ifndef FIRST_STANDARD_RELOC
#define FIRST_STANDARD_RELOC 0
#endif

#define REL_NONE FIRST_STANDARD_RELOC
#define REL_ABS (REL_NONE+1)         /* standard absolute relocation */
#define REL_PC  (REL_ABS+1)          /* PC-relative */
#define REL_GOT (REL_PC+1)           /* symbol's pointer in global off.table */
#define REL_GOTPC (REL_GOT+1)        /* global offset table PC-relative */
#define REL_GOTOFF (REL_GOTPC+1)     /* offset to global offset table */
#define REL_GLOBDAT (REL_GOTOFF+1)   /* global data */
#define REL_PLT (REL_GLOBDAT+1)      /* procedure linkage table */
#define REL_PLTPC (REL_PLT+1)        /* procedure linkage table PC-relative */
#define REL_PLTOFF (REL_PLTPC+1)     /* offset to procedure linkage table */
#define REL_SD (REL_PLTOFF+1)        /* small data base relative */
#define REL_UABS (REL_SD+1)          /* unaligned absolute addr. relocation */
#define REL_LOCALPC (REL_UABS+1)     /* pc-relative to local symbol */
#define REL_LOADREL (REL_LOCALPC+1)  /* relative to load addr., no symbol */
#define REL_COPY (REL_LOADREL+1)     /* copy from shared object */
#define REL_JMPSLOT (REL_COPY+1)     /* procedure linkage table entry */
#define REL_SECOFF (REL_JMPSLOT+1)   /* symbol's offset to start of section */
#define REL_MEMID (REL_SECOFF+1)     /* ID of section's destination memory */
#define LAST_STANDARD_RELOC REL_MEMID
#if LAST_STANDARD_RELOC>0x1f
#error too many standard relocation types
#endif
#define STD_REL_TYPE(t) ((t)&0x1f)   /* masks out the standard type */

/* By default relocations have unspecified signedness, which makes signed
   and unsigned ranges both valid for checks.
   The backend may define an explicit signedness with the following flags: */
#define REL_MOD_S 0x20               /* signed */
#define REL_MOD_U 0x40               /* unsigned */

/* CPU-specific relocations start here */
#define FIRST_CPU_RELOC 0x80
/* LAST_CPU_RELOC must be defined by CPU module, when it supports additional
   relocations. */

/* standard reloc struct */
typedef struct nreloc {
  size_t byteoffset;  /* byte-offset in data atom to beginning of relocation */
  size_t bitoffset;   /* bit-offset adds to byte-off. - start of reloc.field */
  size_t size;        /* size of relocation field in bits */
  utaddr mask;
  taddr addend;
  symbol *sym;
} nreloc;

struct rlist {
  struct rlist *next;
  void *reloc;
  int type;
};

#define DEFMASK (~(utaddr)0)
#define MAKEMASK(x) (((x)>=sizeof(unsigned long long)*CHAR_BIT)?(~(unsigned long long)0):((((unsigned long long)1)<<(x))-1u))


nreloc *new_nreloc(void);
rlist *add_extnreloc(rlist **,symbol *,taddr,int,size_t,size_t,size_t);
rlist *add_extnreloc_masked(rlist **,symbol *,taddr,int,size_t,size_t,
                            size_t,utaddr);
/* old interface: byteoffset and bitoffset are calculated from offset 'o' */
#define add_nreloc(r,y,a,t,s,o) \
  add_extnreloc(r,y,a,t,(o)%BITSPERBYTE,s,(o)/BITSPERBYTE)
#define add_nreloc_masked(r,y,a,t,s,o,m) \
  add_extnreloc_masked(r,y,a,t,(o)%BITSPERBYTE,s,(o)/BITSPERBYTE,m)

#ifndef LAST_CPU_RELOC
#define cpu_reloc_size(r) 0
#endif
int is_pc_reloc(symbol *,section *);
int std_reloc(rlist *);
#define is_std_reloc(r) (std_reloc(r)>=0)
#define is_nreloc(r) (std_reloc(r)>=0 || cpu_reloc_size(r)==0)
void do_pic_check(rlist *);
taddr nreloc_real_addend(nreloc *);
void unsupp_reloc_error(atom *,rlist *);
void checkdefined(rlist *,section *,taddr,atom *);
void print_nreloc(FILE *,nreloc *,int);
void print_reloc(FILE *,rlist *);
rlist *get_relocs(atom *);
int patch_nreloc(atom *,rlist *,taddr,int);

#endif
