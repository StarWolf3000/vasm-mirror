/* output_atari_com.c Atari DOS COM output driver for vasm */
/* (c) in 2002-2009,2013,2015,2017 by Volker Barthelmann and Frank Wille */

#include "vasm.h"

#ifdef OUTATARICOM
static char *copyright="vasm atari com output module 1.8a (c) 2002-2009,2013,2015,2017 Volker Barthelmann";

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
  unsigned long long pc,npc,i;

  if (!sec)
    return;

  for (; sym; sym=sym->next) {
    if (sym->type == IMPORT)
      output_error(6,sym->name);  /* undefined symbol */
  }

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

  /* write Atari DOS COM file header ($FFFF) */
  fw8(f, 0xff);
  fw8(f, 0xff);

  /* make an array of section pointers, sorted by their start address */
  seclist = (section **)mymalloc(nsecs * sizeof(section *));
  for (s=sec,slp=seclist; s!=NULL; s=s->next)
    *slp++ = s;
  if (nsecs > 1)
    qsort(seclist,nsecs,sizeof(section *),orgcmp);

  for (slp=seclist; nsecs>0; nsecs--) {
    s = *slp++;
    /* for each section start with load address 
     * 00: LSB of load address
     * 01: MSB of load address
     */
    fw8(f,s->org&0xff);
    fw8(f,(s->org>>8)&0xff);
    /* for each section followed by last byte address
     * 00: LSB of last byte address
     * 01: MSB of last byte address
     */
    fw8(f,(s->pc-1)&0xff);
    fw8(f,((s->pc-1)>>8)&0xff);

    if (s!=seclist[0] && ULLTADDR(s->org)>pc) {
      /* do not fill gap between sections with zeros */
    }
    else
      pc = ULLTADDR(s->org);

    for (p=s->first; p; p=p->next) {
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
  free(seclist);
}


static int output_args(char *p)
{
  return 0;
}


int init_output_atari_com(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  *cp = copyright;
  *wo = write_output;
  *oa = output_args;
  return 1;
}

#else

int init_output_atari_com(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  return 0;
}
#endif
