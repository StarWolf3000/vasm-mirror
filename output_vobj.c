/* vobj format output driver for vasm */
/* (c) in 2002-2025 by Volker Barthelmann */

#include "vasm.h"

#ifdef OUTVOBJ
static char *copyright="vasm vobj output module 2.0a (c) 2002-2025 Volker Barthelmann";
static unsigned char version;

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

static void write_number(FILE *f,taddr val)
{
  int i,s,u;
  taddr tmp;

  if(val>=0&&val<=127){
    fw8(f,val);
    return;
  }
  
  if(!version){
    s=u=sizeof(taddr);
  }else{
    for(i=1,s=u=1,tmp=val;i<=sizeof(taddr);i++){
      if(tmp&0xff) s=i;
      if((tmp&0xff)!=0xff) u=i;
      tmp>>=8;
    }
  }

  if(u<s){
    fw8(f,0xC0+u);
    s=u;
  }else
    fw8(f,0x80+s);

  for(i=s;i>0;i--){
    fw8(f,val&0xff);
    val>>=8;
  }
}

static void write_string(FILE *f,const char *p)
{
  while(p&&*p){
    fw8(f,*p);
    p++;
  }
  fw8(f,0);
}

static int sym_valid(symbol *symp)
{
  if(*symp->name==' ')
    return 0;  /* ignore internal/temporary symbols */
  if(symp->flags & VASMINTERN)
    return 0;  /* ignore vasm-internal symbols */
  return 1;
}

static int count_relocs(atom *p,rlist *rl)
{
  int nrelocs;

  for(nrelocs=0; rl; rl=rl->next) {

    if(is_nreloc(rl)){
      nreloc *r = (nreloc *)rl->reloc;
      if(r->sym->type==IMPORT&&!sym_valid(r->sym))
        goto badreloc;
      else
        nrelocs++;
    }
#ifdef LAST_CPU_RELOC
    else if(rl->type>=FIRST_CPU_RELOC&&rl->type<=LAST_CPU_RELOC){
      nrelocs+=cpu_reloc_size(rl)!=0;
    }
#endif
    else{
      badreloc:
      unsupp_reloc_error(p,rl);
    }
  }
  return nrelocs;
}

static void get_section_sizes(section *sec,taddr *rsize,taddr *rdata,taddr *rnrelocs)
{
  taddr data=0,nrelocs=0;
  atom *p;
  size_t i;

  sec->pc=0;
  for(p=sec->first;p;p=p->next){
    sec->pc=pcalign(p,sec->pc);
    sec->pc+=atom_size(p,sec,sec->pc);
    if(p->type==DATA){
      data=sec->pc;
      nrelocs+=count_relocs(p,p->content.db->relocs);
    }
    else if(p->type==SPACE){
      if(p->content.sb->relocs){
        nrelocs+=count_relocs(p,p->content.sb->relocs);
        data=sec->pc;
      }else{
        for(i=0;i<OCTETS(p->content.sb->size);i++)
          if(p->content.sb->fill[i]!=0)
            data=sec->pc;
      }
    }
  }
  *rdata=data;
  *rsize=sec->pc;
  *rnrelocs=nrelocs;
}

static void write_data(FILE *f,section *sec,utaddr data)
{
  atom *p;
  sec->pc=0;
  for(p=sec->first;p;p=p->next){
    if((utaddr)sec->pc>=data)
      return;
    sec->pc=fwpcalign(f,p,sec,sec->pc);
    sec->pc+=atom_size(p,sec,sec->pc);
    if(p->type==DATA)
      fwdblock(f,p->content.db);
    else if(p->type==SPACE)
      fwsblock(f,p->content.sb);
  }
}

static int get_nreloc_sym_idx(nreloc *rel)
{
  int idx;

  if(!(idx=rel->sym->idx)){
    if(rel->sym->type==IMPORT&&!sym_valid(rel->sym))
      return 0;
    idx=rel->sym->sec->idx;  /* symbol does not exist, use section-symbol */
  }
  return idx;
}

static void write_rlist(FILE *f,section *sec,rlist *rl)
{
  int idx;
  for(;rl;rl=rl->next){
    write_number(f,rl->type);
    if(is_nreloc(rl)){
      nreloc *rel=rl->reloc;
      if (!(idx=get_nreloc_sym_idx(rel)))
        continue;
      if (rl->type>=FIRST_CPU_RELOC)
        write_number(f,0);  /* cpu-specific reloc is in nreloc format */
      write_number(f,sec->pc+rel->byteoffset);
      write_number(f,rel->bitoffset);
      write_number(f,rel->size);
      write_number(f,rel->mask);
      write_number(f,rel->addend);
      write_number(f,idx);
    }
#ifdef LAST_CPU_RELOC
    else if(rl->type>=FIRST_CPU_RELOC&&rl->type<=LAST_CPU_RELOC){
      size_t sz;

      if (sz=cpu_reloc_size(rl)){
        write_number(f,sz);
        cpu_reloc_write(f,rl);
      }
    }
#endif
    else
      ierror(0);
  }
}

static void write_relocs(FILE *f,section *sec)
{
  atom *p;
  sec->pc=0;
  for(p=sec->first;p;p=p->next){
    sec->pc=pcalign(p,sec->pc);
    if(p->type==DATA)
      write_rlist(f,sec,p->content.db->relocs);
    else if(p->type==SPACE)
      write_rlist(f,sec,p->content.sb->relocs);
    sec->pc+=atom_size(p,sec,sec->pc);
  }
}

static void write_output(FILE *f,section *sec,symbol *sym)
{
  int nsyms,nsecs;
  section *secp;
  symbol *symp,*first,*last;
  taddr size,data,nrelocs;

  /* target-bytes in sections will always be written big-endian */
  output_bytes_le=0;

  /* assign section index, make section symbols */
  for(nsecs=1,secp=sec,first=sym,last=NULL;secp;secp=secp->next){
    secp->idx=nsecs++;  /* symp->idx will become equal to secp->idx */
    symp=mymalloc(sizeof(*symp));
    symp->name=secp->name;
    symp->type=LABSYM;
    symp->flags=TYPE_SECTION;
    symp->sec=secp;
    symp->pc=0;
    symp->expr=symp->size=NULL;
    symp->align=0;
    symp->next=sym;
    if(last!=NULL)
      last->next=symp;
    else
      first=symp;
    last=symp;
  }
  /* assign symbol index to all symbols */
  for(nsyms=1,symp=first;symp;symp=symp->next){
    if(sym_valid(symp))
      symp->idx=nsyms++;
    else
      symp->idx=0;  /* use section-symbol, when needed */
  }

  fw32(f,0x564f424a,1); /* "VOBJ" */
  if(BIGENDIAN)
    fw8(f,1|version);
  else if(LITTLEENDIAN)
    fw8(f,2|version);
  else
    ierror(0);
  write_number(f,BITSPERBYTE);
  write_number(f,bytespertaddr);
  write_string(f,cpuname);
  write_number(f,nsecs-1);
  write_number(f,nsyms-1);

  for(symp=first;symp;symp=symp->next){
    if(!sym_valid(symp))
      continue;
    write_string(f,symp->name);
    write_number(f,symp->type);
    write_number(f,symp->flags);
    write_number(f,symp->sec?symp->sec->idx:0);
    write_number(f,get_sym_value(symp));
    write_number(f,get_sym_size(symp));
  }

  for(secp=sec;secp;secp=secp->next){
    write_string(f,secp->name);
    write_string(f,secp->attr);
    write_number(f,secp->flags);
    write_number(f,secp->align);
    get_section_sizes(secp,&size,&data,&nrelocs);
    write_number(f,size);
    write_number(f,nrelocs);
    write_number(f,data);
    write_data(f,secp,(utaddr)data);
    write_relocs(f,secp);
  }
}

static int output_args(char *p)
{
  if(!strcmp(p,"-vobj2")){
    version=(1<<2);
    return 1;
  }
  return 0;
}

int init_output_vobj(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  *cp=copyright;
  *wo=write_output;
  *oa=output_args;
  secname_attr = 1;  /* attribute is used to differentiate between sections */
  output_bitsperbyte = 1;  /* we do support BITSPERBYTE != 8 */
  return 1;
}

#else

int init_output_vobj(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  return 0;
}
#endif
