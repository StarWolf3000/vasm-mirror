/* symtab.h  hashtable header file for vasm */
/* (c) in 2002-2004,2008,2014 by Volker Barthelmann and Frank Wille */

#include <stdlib.h>

typedef union hashdata {
  void *ptr;
  uint32_t idx;
} hashdata;

typedef struct hashentry {
  const char *name;
  hashdata data;
  struct hashentry *next;
} hashentry;

typedef struct hashtable {
  hashentry **entries;
  size_t size;
  int collisions;
} hashtable;

hashtable *new_hashtable(size_t);
size_t hashcode(const char *);
size_t hashcodelen(const char *,int);
size_t hashcode_nc(const char *);
size_t hashcodelen_nc(const char *,int);
void add_hashentry(hashtable *,const char *,hashdata);
void rem_hashentry(hashtable *,const char *,int);
int find_name(hashtable *,const char *,hashdata *);
int find_namelen(hashtable *,const char *,int,hashdata *);
int find_name_nc(hashtable *,const char *,hashdata *);
int find_namelen_nc(hashtable *,const char *,int,hashdata *);
