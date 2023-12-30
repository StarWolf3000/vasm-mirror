/* woz.c binary output driver for vasm */
/* (c) in 2023 by anomie-p@protonmail.com */

#include "vasm.h"

#ifdef OUTWOZ
static char *copyright="vasm wozmon output module 0.1a (c) 2023 anomie-p@protonmail.com";

/* file, ptr, addr, len, modulus:
   Using the modulus allows for handling both space and data atoms with
   the same code by passing a modulus >= the length for data atoms, and
   a modulus == sp->size for space atoms */
static uint32_t write_bytes(FILE *f, uint8_t *p, uint32_t a,
                            size_t l, size_t m)
{
  size_t i, pc, r;

  for (i = 0, pc = a; i < l; ++i, ++pc) {
    r = pc % 8;
    if(r == 0) {
      fprintf(f, "%X:", (unsigned)pc);
    }
    fprintf(f, " %02X", p[i%m]);
    if(r == 7) {
      fprintf(f, "\r");
    }
  }
  return pc;
}

static void write_output(FILE *f,section *sec,symbol *sym)
{
  section *s;
  atom *a;
  uint32_t addr;

  /* Check for a valid section */
  if (sec == NULL)
    return;

  /* Check for undefined symbols */
  for (; sym; sym=sym->next) {
    if (sym->type==IMPORT)
      output_error(6,sym->name);
  }

  /* No support for overlapping sections. */
  chk_sec_overlap(sec);

  /* Output data and space atoms */
  for (s = sec; s != NULL; s = s->next) {
    addr = s->org;
    /* If the address is aligned on an eight byte boundary, write_bytes() will
       output the address. */
    if(addr%8 != 0) {
      fprintf(f, "%X:", addr);
    }
    /* Output the data and space for the section */
    for(a = s->first; a != NULL; a = a->next) {
      if(a->type == DATA) {
        addr = write_bytes(f, a->content.db->data, addr, a->content.db->size,
			   a->content.db->size);
      } else if(a->type == SPACE) {
        /* sb->space is in units of sb->size */
	addr = write_bytes(f, a->content.sb->fill, addr,
			   a->content.sb->space * a->content.sb->size,
			   a->content.sb->size);
      }
    }
    /* If the address is aligned on an eight byte boundary, write_bytes()
       has output the \r at the end of the section. */
    if(addr%8 != 0) {
      fprintf(f, "\r");
    }
  }
  /* Check if any written address exceeds sixteen bits */
  if(addr > UINT16_MAX+1) {
    output_error(11, addr-1);
  } 
}


static int output_args(char *p)
{
  return 0;
}


int init_output_woz(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  if (sizeof(utaddr) > sizeof(uint32_t) || bitsperbyte != 8) {
    output_error(1, cpuname); /* output module doesn't support (cpuname) */
    return 0;
  }

  *cp = copyright;
  *wo = write_output;
  *oa = output_args;
  defsecttype = emptystr;  /* default section is "org 0" */
  return 1;
}

#else

int init_output_woz(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  return 0;
}
#endif
