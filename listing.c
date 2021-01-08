/* listing.h - listing file */
/* (c) in 2020 by Volker Barthelmann */

#include "vasm.h"

int produce_listing;
int listena,listformfeed=1,listlinesperpage=40,listnosyms;
listing *first_listing,*last_listing,*cur_listing;

static char **listtitles;
static int *listtitlelines;
static int listtitlecnt;


void set_listing(int on)
{
  listena = on && produce_listing;
}

void set_list_title(char *p,int len)
{
  listtitlecnt++;
  listtitles=myrealloc(listtitles,listtitlecnt*sizeof(*listtitles));
  listtitles[listtitlecnt-1]=mymalloc(len+1);
  strncpy(listtitles[listtitlecnt-1],p,len);
  listtitles[listtitlecnt-1][len]=0;
  listtitlelines=myrealloc(listtitlelines,listtitlecnt*sizeof(*listtitlelines));
  listtitlelines[listtitlecnt-1]=cur_src->line;
}

#if VASM_CPU_OIL
static void print_list_header(FILE *f,int cnt)
{
  if(cnt%listlinesperpage==0){
    if(cnt!=0&&listformfeed)
      fprintf(f,"\f");
    if(listtitlecnt>0){
      int i,t;
      for(i=0,t=-1;i<listtitlecnt;i++){
        if(listtitlelines[i]<=cnt+listlinesperpage)
          t=i;
      }
      if(t>=0){
        int sp=(120-strlen(listtitles[t]))/2;
        while(--sp)
          fprintf(f," ");
        fprintf(f,"%s\n",listtitles[t]);
      }
      cnt++;
    }
    fprintf(f,"Err  Line Loc.  S Object1  Object2  M Source\n");
  }
}

void write_listing(char *listname,section *first_section)
{
  FILE *f;
  int nsecs,i,cnt=0,nl;
  section *secp;
  listing *p;
  atom *a;
  symbol *sym;
  taddr pc;
  char rel;

  if(!(f=fopen(listname,"w"))){
    general_error(13,listname);
    return;
  }
  for(nsecs=0,secp=first_section;secp;secp=secp->next)
    secp->idx=nsecs++;
  for(p=first_listing;p;p=p->next){
    if(!p->src||p->src->id!=0)
      continue;
    print_list_header(f,cnt++);
    if(p->error!=0)
      fprintf(f,"%04d ",p->error);
    else
      fprintf(f,"     ");
    fprintf(f,"%4d ",p->line);
    a=p->atom;
    while(a&&a->type!=DATA&&a->next&&a->next->line==a->line&&a->next->src==a->src)
      a=a->next;
    if(a&&a->type==DATA){
      int size=a->content.db->size;
      char *dp=a->content.db->data;
      pc=p->pc;
      fprintf(f,"%05lX %d ",(unsigned long)pc,(int)(p->sec?p->sec->idx:0));
      for(i=0;i<8;i++){
        if(i==4)
          fprintf(f," ");
        if(i<size){
          fprintf(f,"%02X",(unsigned char)*dp++);
          pc++;
        }else
          fprintf(f,"  ");
        /* append following atoms with align 1 directly */
        if(i==size-1&&i<7&&a->next&&a->next->align<=a->align&&a->next->type==DATA&&a->next->line==a->line&&a->next->src==a->src){
          a=a->next;
          size+=a->content.db->size;
          dp=a->content.db->data;
        }
      }
      fprintf(f," ");
      if(a->content.db->relocs){
        symbol *s=((nreloc *)(a->content.db->relocs->reloc))->sym;
        if(s->type==IMPORT)
          rel='X';
        else
          rel='0'+p->sec->idx;
      }else
        rel='A';
      fprintf(f,"%c ",rel);
    }else
      fprintf(f,"                           ");

    fprintf(f," %-.77s",p->txt);

    /* bei laengeren Daten den Rest ueberspringen */
    /* Block entfernen, wenn alles ausgegeben werden soll */
    if(a&&a->type==DATA&&i<a->content.db->size){
      pc+=a->content.db->size-i;
      i=a->content.db->size;
    }

    /* restliche DATA-Zeilen, wenn noetig */
    while(a){
      if(a->type==DATA){
        int size=a->content.db->size;
        char *dp=a->content.db->data+i;

        if(i<size){
          for(;i<size;i++){
            if((i&7)==0){
              fprintf(f,"\n");
              print_list_header(f,cnt++);
              fprintf(f,"          %05lX %d ",(unsigned long)pc,(int)(p->sec?p->sec->idx:0));
            }else if((i&3)==0)
              fprintf(f," ");
            fprintf(f,"%02X",(unsigned char)*dp++);
            pc++;
            /* append following atoms with align 1 directly */
            if(i==size-1&&a->next&&a->next->align<=a->align&&a->next->type==DATA&&a->next->line==a->line&&a->next->src==a->src){
              a=a->next;
              size+=a->content.db->size;
              dp=a->content.db->data;
            }
          }
          i=8-(i&7);
          if(i>=4)
            fprintf(f," ");
          while(i--){
            fprintf(f,"  ");
          }
          fprintf(f," %c",rel);
        }
        i=0;
      }
      if(a->next&&a->next->line==a->line&&a->next->src==a->src){
        a=a->next;
        pc=pcalign(a,pc);
        if(a->type==DATA&&a->content.db->relocs){
          symbol *s=((nreloc *)(a->content.db->relocs->reloc))->sym;
          if(s->type==IMPORT)
            rel='X';
          else
            rel='0'+p->sec->idx;
        }else
          rel='A';
      }else
        a=0;
    }
    fprintf(f,"\n");
  }
  fprintf(f,"\n\nSections:\n");
  for(secp=first_section;secp;secp=secp->next)
    fprintf(f,"%d  %s\n",(int)secp->idx,secp->name);
  if(!listnosyms){
    fprintf(f,"\n\nSymbols:\n");
    {
      symbol *last=0,*cur,*symo;
      for(symo=first_symbol;symo;symo=symo->next){
        cur=0;
        for(sym=first_symbol;sym;sym=sym->next){
          if(!last||stricmp(sym->name,last->name)>0)
            if(!cur||stricmp(sym->name,cur->name)<0)
              cur=sym;
        }
        if(cur){
          print_symbol(f,cur);
          fprintf(f,"\n");
          last=cur;
        }
      }
    }
  }
  if(errors==0)
    fprintf(f,"\nThere have been no errors.\n");
  else
    fprintf(f,"\nThere have been %d errors!\n",errors);
  fclose(f);
  for(p=first_listing;p;){
    listing *m=p->next;
    myfree(p);
    p=m;
  }
}
#else
void write_listing(char *listname,section *first_section)
{
  unsigned long i,maxsrc=0;
  FILE *f;
  int nsecs;
  section *secp;
  listing *p;
  atom *a;
  symbol *sym;
  taddr pc;

  if(!(f=fopen(listname,"w"))){
    general_error(13,listname);
    return;
  }
  for(nsecs=1,secp=first_section;secp;secp=secp->next)
    secp->idx=nsecs++;
  for(p=first_listing;p;p=p->next){
    char err[6];
    if(p->error!=0)
      sprintf(err,"E%04d",p->error);
    else
      sprintf(err,"     ");
    if(p->src&&p->src->id>maxsrc)
      maxsrc=p->src->id;
    fprintf(f,"F%02d:%04d %s %s",(int)(p->src?p->src->id:0),p->line,err,p->txt);
    a=p->atom;
    pc=p->pc;
    while(a){
      if(a->type==DATA){
        unsigned long size=a->content.db->size;
        for(i=0;i<size&&i<32;i++){
          if((i&15)==0)
            fprintf(f,"\n               S%02d:%08lX: ",(int)(p->sec?p->sec->idx:0),(unsigned long)(pc));
          fprintf(f," %02X",(unsigned char)a->content.db->data[i]);
          pc++;
        }
        if(a->content.db->relocs)
          fprintf(f," [R]");
      }
      if(a->next&&a->next->list==a->list){
        a=a->next;
        pc=pcalign(a,pc);
      }else
        a=0;
    }
    fprintf(f,"\n");
  }
  fprintf(f,"\n\nSections:\n");
  for(secp=first_section;secp;secp=secp->next)
    fprintf(f,"S%02d  %s\n",(int)secp->idx,secp->name);
  fprintf(f,"\n\nSources:\n");
  for(i=0;i<=maxsrc;i++){
    for(p=first_listing;p;p=p->next){
      if(p->src&&p->src->id==i){
        fprintf(f,"F%02lu  %s\n",i,p->src->name);
        break;
      }
    }
  }
  fprintf(f,"\n\nSymbols:\n");
  for(sym=first_symbol;sym;sym=sym->next){
    print_symbol(f,sym);
    fprintf(f,"\n");
  }
  if(errors==0)
    fprintf(f,"\nThere have been no errors.\n");
  else
    fprintf(f,"\nThere have been %d errors!\n",errors);
  fclose(f);
  for(p=first_listing;p;){
    listing *m=p->next;
    myfree(p);
    p=m;
  }
}
#endif
