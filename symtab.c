/* symtab.c  hashtable file for vasm */
/* (c) in 2002-2004,2008,2011,2014,2026 */
/* by Volker Barthelmann and Frank Wille */

#include "vasm.h"

hashtable *new_hashtable(size_t size,int nocase)
{
  hashtable *new=mymalloc(sizeof(*new));

#ifdef LOWMEM
  /* minimal hash tables */
  if(size>0x100)
    size=0x100;
#endif
  new->size=size;
  new->nocase=nocase;
  new->collisions=0;
  new->entries=mycalloc(size*sizeof(*new->entries));
  return new;
}

static size_t hashcode(const char *name,int no_case)
{
  size_t h=5381;
  int c;

  if(no_case){
    while(c=(unsigned char)*name++)
      h=((h<<5)+h)+tolower(c);
  }else{
    while(c=(unsigned char)*name++)
      h=((h<<5)+h)+c;
  }
  return h;
}

static size_t hashcodelen(const char *name,int len,int no_case)
{
  size_t h=5381;

  if(no_case){
    while(len--)
      h=((h<<5)+h)+tolower((unsigned char)*name++);
  }else{
    while(len--)
      h=((h<<5)+h)+(unsigned char)*name++;
  }
  return h;
}

/* add to hashtable; name must be unique */
void add_hashentry(hashtable *ht,const char *name,hashdata data)
{
  size_t i=hashcode(name,ht->nocase)%ht->size;
  hashentry *new=mymalloc(sizeof(*new));

  new->name=name;
  new->data=data;
  if(debug){
    if(ht->entries[i])
      ht->collisions++;
  }
  new->next=ht->entries[i];
  ht->entries[i]=new;
}

/* remove from hashtable; name must be unique */
void rem_hashentry(hashtable *ht,const char *name)
{
  size_t i=hashcode(name,ht->nocase)%ht->size;
  hashentry *p,*last;

  for(p=ht->entries[i],last=NULL;p;p=p->next){
    if(!strcmp(name,p->name)||(ht->nocase&&!cistrcmp(name,p->name))){
      if(last==NULL)
        ht->entries[i]=p->next;
      else
        last->next=p->next;
      myfree(p);
      return;
    }
    last=p;
  }
  ierror(0);
}

/* finds unique entry in hashtable */
int find_name(hashtable *ht,const char *name,hashdata *result)
{
  size_t i=hashcode(name,ht->nocase)%ht->size;
  hashentry *p;

  if (ht->nocase){
    for(p=ht->entries[i];p;p=p->next){
      if(!cistrcmp(name,p->name)){
        *result=p->data;
        return 1;
      }else
        ht->collisions++;
    }
  }else{
    for(p=ht->entries[i];p;p=p->next){
      if(!strcmp(name,p->name)){
        *result=p->data;
        return 1;
      }else
        ht->collisions++;
    }
  }
  return 0;
}

/* same as above, but uses len instead of zero-terminated string */
int find_namelen(hashtable *ht,const char *name,int len,hashdata *result)
{
  size_t i=hashcodelen(name,len,ht->nocase)%ht->size;
  hashentry *p;

  if(ht->nocase){
    for(p=ht->entries[i];p;p=p->next){
      if(!cistrncmp(name,p->name,len)&&p->name[len]==0){
        *result=p->data;
        return 1;
      }else
        ht->collisions++;
    }
  }else{
    for(p=ht->entries[i];p;p=p->next){
      if(!strncmp(name,p->name,len)&&p->name[len]==0){
        *result=p->data;
        return 1;
      }else
        ht->collisions++;
    }
  }
  return 0;
}
