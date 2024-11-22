/*
 * vobjdump
 * Views the contents of a VOBJ file.
 * Written by Frank Wille <frank@phoenix.owl.de>.
 */

/*
  Format:

  .byte 0x56,0x4f,0x42,0x4a
  .byte flags
    Bits 0-1:
     1: BIGENDIAN
     2: LITTLENDIAN
    Bits 2-7:
     VOBJ-Version (0-based)
  .number bitsperbyte 
  .number bytespertaddr
  .string cpu
  .number nsections [1-based]
  .number nsymbols [1-based]
  
nsymbols
  .string name
  .number type
  .number flags
  .number secindex
  .number val
  .number size (in target-bytes)

nsections
  .string name
  .string attr
  .number flags
  .number align
  .number size (in target-bytes)
  .number nrelocs
  .number databytes (in target-bytes)
  .byte[databytes*(BITSPERBYTE+7)/8]

nrelocs [standard|special]
standard
   .number type
   .number byteoffset
   .number bitoffset
   .number size
   .number mask
   .number addend
   .number symbolindex | 0 (sectionbase)

special
    .number type
    .number size (0 means standard nreloc)
    .byte[size]

.number:[taddr]
    .byte 0--127 [0--127]
    .byte 128-191 [x-0x80 bytes little-endian], fill remaining with 0
    .byte 192-255 [x-0xC0 bytes little-endian], fill remaining with 0xff [vobj version 2+]
*/

#include "vobjdump.h"

static const char *endian_name[] = { "big", "little" };
static const char emptystr[] = "";
static const char sstr[] = "s";
static const char unknown[] = "UNKNOWN";

static const char *std_reloc_name[] = {
  "NONE","ABS","PC","GOT","GOTPC","GOTOFF","GLOBDAT","PLT","PLTPC","PLTOFF",
  "SD","UABS","LOCALPC","LOADREL","COPY","JMPSLOT","SECOFF"
};
static const int num_std_relocs = sizeof(std_reloc_name)/sizeof(std_reloc_name[0]);

static const char *ppc_reloc_name[] = {
  "EABISDA2","EABISDA21","EABISDAI16","EABISDA2I16",
  "MOSDREL","AOSBREL"
};
static const int num_ppc_relocs = sizeof(ppc_reloc_name)/sizeof(ppc_reloc_name[0]);

static const char *type_name[] = {
  "","obj","func","sect","file",NULL
};

static ubyte *vobj;   /* base address of VOBJ buffer */
static size_t vlen;   /* length of VOBJ file in buffer */
static ubyte *p;      /* current object pointer */
static int bpb,bpt;   /* bits per target-byte, target-bytes per taddr */
static int bitspertaddr;
static size_t opb;    /* octets (host-bytes) per target-byte */
static taddr bptmask; /* mask LSB to fit bytes per taddr */
static const char *cpu_name;

#define BPTMASK(x) (unsigned long long)((x)&bptmask)


static taddr read_number(int is_signed)
{
  taddr val;
  ubyte n;
  int i;

  if (p<vobj || p>=vobj+vlen) {
    corrupt:
    fprintf(stderr,"\nObject file is corrupt! Aborting.\n");
    exit(1);
  }

  if ((n = *p++) <= 0x7f)
    return (taddr)n;

  if (p + (n&0x3f) > vobj + vlen)
    goto corrupt;

  if (n >= 0xc0) {  /* version 2 negative numbers */
    n -= 0xc0;
    for (i=0,val=~makemask(n*8); n--; i+=8)
      val |= (taddr)*p++ << i;
  }
  else {
    for (i=0,n-=0x80,val=0; n--; i+=8)
      val |= (taddr)*p++ << i;
    if (is_signed && (val & (1LL<<(bitspertaddr-1))))
      val |= ~makemask(bitspertaddr);  /* version 1 negative numbers support */
  }
  return val;
}


static void skip_string(void)
{
  if (p < vobj)
    goto corrupt;
  while (*p) {
    p++;
    if (p >= vobj+vlen) {
      corrupt:
      fprintf(stderr,"\nObject file is corrupt! Aborting.\n");
      exit(1);
    }
  }
  p++;
}


static void print_sep(void)
{
  printf("\n------------------------------------------------------------"
         "------------------\n");
}


static int print_nreloc(const char *relname,struct vobj_section *vsect,
                        struct vobj_symbol *vsym,int nsyms,
                        taddr offs,int bpos,int bsiz,taddr mask,
                        taddr addend,int sym)
{
  const char *basesym;

  if (offs<0 || offs+((bpos+bsiz-1)/bpb)>=vsect->dsize) {
    printf("offset %#llx is outside of section!\n",
            BPTMASK(offs+(bpos+bsiz-1)/bpb));
    return 0;
  }
  if (sym<0 || sym>=nsyms) {
    printf("symbol index %d is illegal!\n",sym+1);
    return 0;
  }
  if (bsiz<0 || bsiz>bitspertaddr) {
    printf("size of %d bits is illegal!\n",bsiz);
    return 0;
  }
  if (bpos<0 || bpos+bsiz>bitspertaddr) {
    printf("bit field start=%d, size=%d doesn't fit into target address "
           "type (%d bits)!\n",bpos,bsiz,bitspertaddr);
    return 0;
  }

  basesym = vsym[sym].name;
  if (!strncmp(basesym," *current pc",12)) {
    basesym = vsect->name;
    /*addend += offs;*/
  }

  printf("%08llx  %02d %02d %8llx %-8s %s%+" PRId64 "\n",
         BPTMASK(offs),bpos,bsiz,BPTMASK(mask),relname,basesym,addend);
  return 1;
}


static int print_cpureloc(int type,ubyte *rstart)
{
  return 0;  /* unknown cpu specific relocation */
}


static const char *cpu_reloc_name(int type)
{
  if (type >= FIRST_CPU_RELOC) {
    type -= FIRST_CPU_RELOC;

    if (!strcmp(cpu_name,"PowerPC") && type<num_ppc_relocs)
      return ppc_reloc_name[type];
  }
  return unknown;
}


static const char *standard_reloc_name(int type)
{
  int basetype = STD_REL_TYPE(type);

  if (basetype < num_std_relocs) {
    static char buf[32];

    if (strlen(std_reloc_name[basetype])+3 < 32) {
      strcpy(buf,std_reloc_name[basetype]);
      if (type & REL_MOD_S)
        strcat(buf,"_S");
      else if (type & REL_MOD_U)
        strcat(buf,"_U");
      return buf;
    }
  }
  return unknown;
}


static void read_symbol(struct vobj_symbol *vsym)
{
  vsym->offs = p - vobj;
  vsym->name = (const char *)p;
  skip_string();
  vsym->type = (int)read_number(0);
  vsym->flags = (int)read_number(0);
  vsym->sec = (int)read_number(0);
  vsym->val = read_number(1);
  vsym->size = (int)read_number(0);
}


static void read_section(struct vobj_section *vsect,
                         struct vobj_symbol *vsym,int nsyms)
{
  const char *attr;
  unsigned long flags;
  int align,nrelocs,i;

  vsect->offs = p - vobj;
  vsect->name = (char *)p;
  skip_string();
  attr = (char *)p;
  skip_string();
  flags = (unsigned long)read_number(0);
  align = (int)read_number(0);
  vsect->dsize = read_number(0);
  nrelocs = (int)read_number(0);
  vsect->fsize = read_number(0);

  print_sep();
  printf("%08llx: SECTION \"%s\" (attributes=\"%s\")\n"
         "Flags: %-8lx  Alignment: %-6d "
         "Total size: %-9" PRId64 " File size: %-9" PRId64 "\n",
         BPTMASK(vsect->offs),vsect->name,attr,flags,align,
         vsect->dsize,vsect->fsize);
  if (nrelocs)
    printf("%d Relocation%s present.\n",nrelocs,nrelocs==1?emptystr:sstr);

  p += vsect->fsize * opb;  /* skip section contents */

  /* read and print relocations for this section */
  for (i=0; i<nrelocs; i++) {
    int type;
    size_t len;

    if (i == 0) {
      /* print header */
      printf("\nfile offs sectoffs pos sz mask     type     symbol+addend\n");
    }
    printf("%08llx: ",BPTMASK(p-vobj));

    type = read_number(0);
    if (type >= FIRST_CPU_RELOC)
      len = read_number(0);  /* length of spec. reloc entry in octets */
    else
      len = 0;

    if (type<FIRST_CPU_RELOC || len==0) {
      /* we have either a standard nreloc or a cpu-specific nreloc */
      taddr offs,mask,addend;
      int bpos,bsiz,sym;

      if (type<FIRST_CPU_RELOC && STD_REL_TYPE(type)>=num_std_relocs) {
        fprintf(stderr,"Illegal standard reloc type: %d\n",type);
        exit(1);
      }
      offs = read_number(0);
      bpos = (int)read_number(0);
      bsiz = (int)read_number(0);
      mask = read_number(1);
      addend = read_number(1);
      sym = (int)read_number(0) - 1;  /* symbol index */
      print_nreloc(type >= FIRST_CPU_RELOC ?
                   cpu_reloc_name(type) : standard_reloc_name(type),
                   vsect,vsym,nsyms,offs,bpos,bsiz,mask,addend,sym);
    }
    else {
      /* special relocation in cpu-specific format */
      if (!print_cpureloc(type,p)) {
        char sname[32];

        sprintf(sname,"%.10s special reloc",cpu_name);
        printf("%-24s %d with a size of %u octets\n",sname,type,(unsigned)len);
      }
      p += len;
      if (p<vobj || p>vobj+vlen)
        break;
    }
  }

  if (p<vobj || p>vobj+vlen) {
    fprintf(stderr,"\nSection \"%s\" is corrupt! Aborting.\n",vsect->name);
    exit(1);
  }
}


static const char *bind_name(int flags)
{
  if (flags & WEAK)
    return "WEAK";
  if (flags & EXPORT)
    return "GLOB";
  if (flags & COMMON)
    return "COMM";
  return "LOCL";
}


static const char *def_name(struct vobj_symbol *vs,
                            struct vobj_section *sec,int nsecs)
{
  switch (vs->type) {
    case EXPRESSION:
      return "*ABS*";
    case IMPORT:
      return "*UND*";
    case LABSYM:
      if (vs->sec>0 && vs->sec<=nsecs)
        return sec[vs->sec-1].name;
  }
  return "???";
}


static int vobjdump(void)
{
  p = vobj;

  if (vlen>4 && p[0]==0x56 && p[1]==0x4f && p[2]==0x42 && p[3]==0x4a) {
    int endian,ver,nsecs,nsyms,i;
    struct vobj_symbol *vsymbols = NULL;
    struct vobj_section *vsect = NULL;

    p += 4;	/* skip ID */
    endian = (int)*p++;  /* endianness and version */

    ver = (endian>>2)+1;
    if (ver > VOBJ_MAX_VERSION) {
      fprintf(stderr,"Version %d not supported!\n",ver);
      return 1;
    }

    endian &= 3;
    if (endian<1 || endian>2) {
      fprintf(stderr,"Bad endianness: %d\n",endian);
      return 1;
    }

    bpb = (int)read_number(0);  /* bits per byte */
    if (bpb % 8) {
      fprintf(stderr,"%d bits per byte currently not supported!\n",bpb);
      return 1;
    }
    opb = (bpb + 7) / 8;

    bpt = (int)read_number(0);  /* bytes per taddr */
    bitspertaddr = bpb * bpt;   /* bits per taddr */
    if ((bitspertaddr+7)/8 > sizeof(taddr)) {
      fprintf(stderr,"%d bytes per taddr not supported!\n",bpt);
      return 1;
    }
    bptmask = makemask(bitspertaddr);

    cpu_name = (char *)p;
    skip_string();  /* skip cpu-string */
    nsecs = (int)read_number(0);  /* number of sections */
    nsyms = (int)read_number(0);  /* number of symbols */

    /* print header */
    print_sep();
    printf("VOBJ v%d %s (%s endian), %d bits per byte, %d byte%s per address.\n"
           "%d symbol%s.\n%d section%s.\n",
           ver,cpu_name,endian_name[endian-1],bpb,bpt,bpt==1?emptystr:sstr,
           nsyms,nsyms==1?emptystr:sstr,nsecs,nsecs==1?emptystr:sstr);

    /* read symbols */
    if (nsyms) {
      if (vsymbols = malloc(nsyms * sizeof(struct vobj_symbol))) {
        for (i=0; i<nsyms; i++)
          read_symbol(&vsymbols[i]);
      }
      else {
        fprintf(stderr,"Cannot allocate %ld bytes for symbols!\n",
                (long)(nsyms * sizeof(struct vobj_symbol)));
        return 1;
      }
    }

    /* read and print sections */
    if (vsect = malloc(nsecs * sizeof(struct vobj_section))) {
      for (i=0; i<nsecs; i++)
        read_section(&vsect[i],vsymbols,nsyms);
    }
    else {
      fprintf(stderr,"Cannot allocate %ld bytes for sections!\n",
              (long)(nsecs * sizeof(struct vobj_section)));
      return 1;
    }

    /* print symbols */
    for (i=0; i<nsyms; i++) {
      struct vobj_symbol *vs = &vsymbols[i];

      if (i == 0) {
        printf("\n");
        print_sep();
        printf("SYMBOL TABLE\n"
               "file offs bind size     type def      value    name\n");
      }
      if (!strncmp(vs->name," *current pc",12))
        continue;
      printf("%08llx: %-4s %08x %-4s %8.8s %8llx %s\n",
             BPTMASK(vs->offs),bind_name(vs->flags),(unsigned)vs->size,
             type_name[TYPE(vs)],def_name(vs,vsect,nsecs),
             BPTMASK(vs->val),vs->name);
    }
  }
  else {
    fprintf(stderr,"Not a VOBJ file!\n");
    return 1;
  }

  return 0;
}


static size_t filesize(FILE *fp,const char *name)
{
  long size;

  if (fgetc(fp) != EOF)
    if (fseek(fp,0,SEEK_END) >= 0)
      if ((size = ftell(fp)) >= 0)
        if (fseek(fp,0,SEEK_SET) >= 0)
          return (size_t)size;
  fprintf(stderr,"Cannot determine size of file \"%s\"!\n",name);
  return 0;
}


int main(int argc,char *argv[])
{
  int rc = 1;

  if (argc == 2) {
    FILE *f;

    if (f = fopen(argv[1],"rb")) {
      if (vlen = filesize(f,argv[1])) {
        if (vobj = malloc(vlen)) {
          if (fread(vobj,1,vlen,f) == vlen)
            rc = vobjdump();
          else
            fprintf(stderr,"Read error on \"%s\"!\n",argv[1]);
          free(vobj);
        }
        else
          fprintf(stderr,"Unable to allocate %lu bytes "
                  "to buffer file \"%s\"!\n",vlen,argv[1]);
      }
      fclose(f);
    }    
    else
      fprintf(stderr,"Cannot open \"%s\" for reading!\n",argv[1]);
  }
  else
    fprintf(stderr,"vobjdump V0.7\nWritten by Frank Wille\n"
            "Usage: %s <file name>\n",argv[0]);

  return rc;
}
