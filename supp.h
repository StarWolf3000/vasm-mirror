/* supp.h miscellaneous support routines */
/* (c) in 2008-2024 by Frank Wille */

#ifndef SUPP_H
#define SUPP_H

struct node {
  struct node *next;
  struct node *pred;
};

struct list {
  struct node *first;
  struct node *dummy;
  struct node *last;
};

void initlist(struct list *);
void addtail(struct list *,struct node *);
struct node *remnode(struct node *);
struct node *remhead(struct list *);

void *mymalloc(size_t);
void *mycalloc(size_t);
void *myrealloc(const void *,size_t);
void myfree(void *);

#if BITSPERBYTE == 8
#define readbyte(p) (utaddr)(*(uint8_t *)(p))
#define writebyte(p,v) *((uint8_t *)(p)) = (uint8_t)(v)
#else
utaddr readbyte(void *);
void writebyte(void *,utaddr);
#endif
taddr bf_sign_extend(taddr,int);
uint64_t readval(int,void *,size_t);
void *setval(int,void *,size_t,uint64_t);
void *setval_signext(int,void *,size_t,size_t,int64_t);
uint64_t readbits(int,void *,unsigned,unsigned,unsigned);
void setbits(int,void *,unsigned,unsigned,unsigned,uint64_t);
int countbits(taddr);
int tffs(taddr);
void copy_cpu_taddr(void *,taddr,size_t);

#if FLOAT_PARSER
void conv2ieee32(int,uint8_t *,tfloat);
void conv2ieee64(int,uint8_t *,tfloat);
int flt_chkrange(tfloat,int);
#endif

void fw8(FILE *,uint8_t);
void fw16(FILE *,uint16_t,int);
void fw24(FILE *,uint32_t,int);
void fw32(FILE *,uint32_t,int);
void fwdata(FILE *,const void *,size_t);
void fwbytes(FILE *,void *,size_t);
#if BITSPERBYTE == 8
#define fwdblock(f,d) fwdata(f,(d)->data,(d)->size)
#else
#define fwdblock(f,d) fwbytes(f,(d)->data,(d)->size)
#endif
void fwsblock(FILE *,sblock *);
void fwspace(FILE *,size_t);
void fwalign(FILE *,taddr,taddr);
int fwpattern(FILE *,taddr,uint8_t *,int);
taddr fwpcalign(FILE *,atom *,section *,taddr);
size_t filesize(FILE *);

int stricmp(const char *,const char *);
int strnicmp(const char *,const char *,size_t);
char *mystrdup(const char *);
char *cnvstr(const char *,int);
char *cutstr(strbuf *,const char *,size_t);
char *strbuf_alloc(strbuf *,size_t);
char *strtolower(char *);
int str_is_graph(const char *);
const char *trim(const char *);
char *get_str_arg(const char *);

taddr balign(taddr,taddr);
taddr palign(taddr,int);
taddr pcalign(atom *,taddr);
int make_padding(taddr,uint8_t *,int);

size_t chk_sec_overlap(section *);
taddr get_sym_value(symbol *);
taddr get_sym_size(symbol *);
utaddr get_sec_size(section *);
int get_sec_type(section *);

/* section types returned by get_sec_type */
#define S_MISS -1
#define S_TEXT 0
#define S_DATA 1
#define S_BSS 2
#define S_ABS 3

#endif
