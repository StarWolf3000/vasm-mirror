/* output_bin.c binary output driver for vasm */
/* (c) in 2002-2009,2013-2020 by Volker Barthelmann and Frank Wille */

#include "vasm.h"

#ifdef OUTBIN
static char *copyright="vasm binary output module 2.0 (c) 2002-2020 Volker Barthelmann and Frank Wille";

#define BINFMT_RAW          0   /* no header */
#define BINFMT_CBMPRG       1   /* Commodore VIC-20/C-64 PRG format */
#define BINFMT_ATARICOM     2   /* Atari 800 DOS COM format */
#define BINFMT_APPLEBIN     3   /* Apple DOS 3.3 binary file */
#define BINFMT_DRAGONBIN    4   /* Dragon DOS binary format */
#define BINFMT_COCOML       5   /* Tandy Color Computer machine lang. file */

static int binfmt = BINFMT_RAW;
static char *exec_symname;
static taddr exec_addr;


static int orgcmp(const void *sec1,const void *sec2)
{
  if (ULLTADDR((*(section **)sec1)->org) > ULLTADDR((*(section **)sec2)->org))
    return 1;
  if (ULLTADDR((*(section **)sec1)->org) < ULLTADDR((*(section **)sec2)->org))
    return -1;
  return 0;
}


static void write_output(FILE *f,section *sec,symbol *sym)
{
  section *s,*s2,**seclist,**slp;
  atom *p;
  size_t nsecs;
  long hdroffs;
  unsigned long long pc=0,npc,i;

  if (!sec)
    return;

  for (; sym; sym=sym->next) {
    if (sym->type == IMPORT)
      output_error(6,sym->name);  /* undefined symbol */

    if (exec_symname!=NULL && !strcmp(exec_symname,sym->name)) {
      exec_addr = sym->pc;
      exec_symname = NULL;  /* found the start-symbol */
    }
  }
  if (exec_symname != NULL)
    output_error(6,exec_symname);  /* start-symbol not found */

  /* we don't support overlapping sections */
  for (s=sec,nsecs=0; s!=NULL; s=s->next) {
    for (s2=s->next; s2; s2=s2->next) {
      if (((ULLTADDR(s2->org) >= ULLTADDR(s->org) &&
            ULLTADDR(s2->org) < ULLTADDR(s->pc)) ||
           (ULLTADDR(s2->pc) > ULLTADDR(s->org) &&
            ULLTADDR(s2->pc) <= ULLTADDR(s->pc))))
        output_error(0);
    }
    nsecs++;
  }

  /* make an array of section pointers, sorted by their start address */
  seclist = (section **)mymalloc(nsecs * sizeof(section *));
  for (s=sec,slp=seclist; s!=NULL; s=s->next)
    *slp++ = s;
  if (nsecs > 1)
    qsort(seclist,nsecs,sizeof(section *),orgcmp);

  /* write an optional header first */
  switch (binfmt) {
    case BINFMT_CBMPRG:
      /* Commodore 6502 PRG header:
       * 00: LSB of load address
       * 01: MSB of load address
       */
      fw16(f,sec->org,0);
      break;

    case BINFMT_ATARICOM:
      /* ATARI COM header: $FFFF */
      fw16(f,0xffff,0);
      break;

    case BINFMT_APPLEBIN:
      /* Apple DOS binary file header:
       * 00-01: load address (little endian)
       * 02-03: file length (little endian)
       */
      fw16(f,sec->org,0);
      hdroffs = ftell(f);  /* remember location of file length */
      fw16(f,0,0);         /* skip file length, will be patched later */
      break;

    case BINFMT_DRAGONBIN:
      /* Dragon DOS file header:
       * 00:    $55
       * 01:    filetype binary $02
       * 02-03: load address (big endian)
       * 04-05: file length (big endian)
       * 06-07: execution address (big endian)
       * 08:    $AA
       */
      fw16(f,0x5502,1);
      fw16(f,sec->org,1);
      hdroffs = ftell(f);  /* remember location of file length */
      fw16(f,0,1);         /* skip file length, will be patched later */
      fw16(f,exec_addr?exec_addr:sec->org,1);
      fw8(f,0xaa);
      break;
  }

  for (slp=seclist; nsecs>0; nsecs--) {
    s = *slp++;

    /* write optional section header or pad to next section start */
    switch (binfmt) {
      case BINFMT_ATARICOM:
        /* for each section
         * 00-01: address of first byte (little endian)
         * 02-03: address of last byte (little endian)
         */
        fw16(f,s->org,0);
        fw16(f,s->pc-1,0);
        break;

      case BINFMT_COCOML:
        /* segment header with length and load address
         * 00:    $00
         * 01-02: length of segment in bytes (big endian)
         * 03-04: load address of segment (big endian)
         */
        fw8(f,0);
        fw16(f,s->pc-s->org,1);
        fw16(f,s->org,1);
        break;

      default:
        /* fill gap between sections with pad-bytes */
        if (s!=seclist[0] && ULLTADDR(s->org) > pc)
          fwalignpattern(f,ULLTADDR(s->org)-pc,s->pad,s->padbytes);
        break;
    }

    /* write section contents */
    for (p=s->first,pc=ULLTADDR(s->org); p; p=p->next) {
      npc = ULLTADDR(fwpcalign(f,p,s,pc));

      if (p->type == DATA) {
        for (i=0; i<p->content.db->size; i++)
          fw8(f,(unsigned char)p->content.db->data[i]);
      }
      else if (p->type == SPACE) {
        fwsblock(f,p->content.sb);
      }

      pc = npc + atom_size(p,s,npc);
    }
  }

  /* patch the header or write trailer */
  switch (binfmt) {
    case BINFMT_APPLEBIN:
      fseek(f,hdroffs,SEEK_SET);
      fw16(f,pc-sec->org,0);  /* total file length */
      break;

    case BINFMT_DRAGONBIN:
      fseek(f,hdroffs,SEEK_SET);
      fw16(f,pc-sec->org,1);  /* total file length */
      break;

    case BINFMT_COCOML:
      /* Color Computer machine language trailer
       * 00-02: end of program token: $ff,$00,$00
       * 03-04: execution address (big endian)
       */
      fw8(f,0xff);
      fw16(f,0,1);
      fw16(f,exec_addr?exec_addr:sec->org,1);
      break;
  }

  myfree(seclist);
}


static int output_args(char *p)
{
  if (!strncmp(p,"-exec=",6)) {
    exec_symname = p + 6;
    return 1;
  }
  else if (!strcmp(p,"-cbm-prg")) {
    binfmt = BINFMT_CBMPRG;
    return 1;
  }
  else if (!strcmp(p,"-atari-com")) {
    binfmt = BINFMT_ATARICOM;
    return 1;
  }
  else if (!strcmp(p,"-apple-bin")) {
    binfmt = BINFMT_APPLEBIN;
    return 1;
  }
  else if (!strcmp(p,"-dragon-bin")) {
    binfmt = BINFMT_DRAGONBIN;
    return 1;
  }
  else if (!strcmp(p,"-coco-ml")) {
    binfmt = BINFMT_COCOML;
    return 1;
  }
  return 0;
}


int init_output_bin(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  *cp = copyright;
  *wo = write_output;
  *oa = output_args;
  return 1;
}

#else

int init_output_bin(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  return 0;
}
#endif
