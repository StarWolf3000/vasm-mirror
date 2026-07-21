/* expr.h expression handling for vasm */
/* (c) in 2002-2020,2024-2026 by Volker Barthelmann and Frank Wille */

#include "hugeint.h"

/* Note: NUM, HUG, FLT and SYM *must* be the first types! In this order! */
enum {
  NUM=1,HUG,FLT,SYM,
  ADD,SUB,MUL,DIV,MOD,NEG,CPL,LAND,LOR,BAND,BOR,XOR,BORN,NOT,LSH,RSH,RSHU,
  LT,GT,LEQ,GEQ,NEQ,EQ
};
#define LAST_EXP_TYPE EQ

struct expr {
  int type;
  struct expr *left;
  struct expr *right;
  union {
    taddr val;
    tfloat flt;
    thuge huge;
    symbol *sym;
  } c;
};

/* strbuf-number to use for the expression parser only in
   parse_identifier() and get_local_label() */
#define EXPBUFNO 2

/* operator macros, return token length on match */
#ifndef T_LOR
#define T_LOR(s) ((*(s)=='|'&&(s)[1]=='|')?2:0)
#endif
#ifndef L_AND
#define T_LAND(s) ((*(s)=='&'&&(s)[1]=='&')?2:0)
#endif
#ifndef T_EQ
#define T_EQ(s) ((*(s)=='='&&(s)[1]=='=')?2:0)
#endif
#ifndef T_NEQ
#define T_NEQ(s) ((*(s)=='!'&&(s)[1]=='=')||(*(s)=='<'&&(s)[1]=='>')?2:0)
#endif
#ifndef T_LEQ
#define T_LEQ(s) ((*(s)=='<'&&(s)[1]=='=')?2:0)
#endif
#ifndef T_GEQ
#define T_GEQ(s) ((*(s)=='>'&&(s)[1]=='=')?2:0)
#endif
#ifndef T_LT
#define T_LT(s) (*(s)=='<'&&(s)[1]!='='&&s[1]!='<')
#endif
#ifndef T_GT
#define T_GT(s) (*(s)=='>'&&(s)[1]!='='&&s[1]!='>')
#endif
#ifndef T_ADD
#define T_ADD(s) (*(s)=='+')
#endif
#ifndef T_SUB
#define T_SUB(s) (*(s)=='-')
#endif
#ifndef T_MUL
#define T_MUL(s) (*(s)=='*')
#endif
#ifndef T_DIV
#define T_DIV(s) (*(s)=='/')
#endif
#ifndef T_MOD
#define T_MOD(s) (*(s)=='%')
#endif
#ifndef T_BOR
#define T_BOR(s) (*(s)=='|'&&(s)[1]!='|')
#endif
#ifndef T_XOR
#define T_XOR(s) (*(s)=='^')
#endif
#ifndef T_BORN
#define T_BORN(s) 0
#endif
#ifndef T_BAND
#define T_BAND(s) (*(s)=='&'&&(s)[1]!='&')
#endif
#ifndef T_LSH
#define T_LSH(s) ((*(s)=='<'&&(s)[1]=='<')?2:0)
#endif
#ifndef T_RSH
#define T_RSH(s) ((*(s)=='>'&&(s)[1]=='>')?2:0)
#endif
/* unary, prefix operator macros */
#ifndef T_PLUS
#define T_PLUS(s) (*(s)=='+')
#endif
#ifndef T_MINUS
#define T_MINUS(s) (*(s)=='-')
#endif
#ifndef T_NOT
#define T_NOT(s) (*(s)=='!')
#endif
#ifndef T_CPL
#define T_CPL(s) (*(s)=='~')
#endif

/* syntax modules may optionally transform character constants */
#ifndef CHAR_CONST_TRANSFORM
#define CHAR_CONST_TRANSFORM(c,q) (c)
#endif

/* global variables */
extern char current_pc_char;
extern int unsigned_shift;
extern int charsperexp;

/* functions */
int init_expr(void);
expr *new_expr(void);
expr *make_expr(int,expr *,expr *);
expr *copy_tree(expr *);
expr *new_sym_expr(symbol *);
expr *curpc_expr(void);
expr *parse_expr(char **);
expr *parse_expr_tmplab(char **);
expr *parse_expr_huge(char **);
expr *parse_expr_float(char **);
taddr parse_constexpr(char **);
expr *number_expr(taddr);
expr *huge_expr(thuge);
void free_expr(expr *);
int type_of_expr(expr *);
expr **find_sym_expr(expr **,char *);
void simplify_expr(expr *);
int eval_expr(expr *,taddr *,section *,taddr);
int eval_expr_huge(expr *,thuge *);
void print_expr(FILE *,expr *);
int find_base(expr *,symbol **,section *,taddr);
#if FLOAT_PARSER
expr *float_expr(tfloat);
int eval_expr_float(expr *,tfloat *);
#endif

/* find_base return codes */
#define BASE_ILLEGAL 0
#define BASE_OK 1
#define BASE_PCREL 2
