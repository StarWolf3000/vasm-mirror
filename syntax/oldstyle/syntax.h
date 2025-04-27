/* syntax.h  syntax header file for vasm */
/* (c) in 2002,2012,2014,2017,2020,2025 by Frank Wille */

/* macros to recognize identifiers */
int isidchar(char);
#define ISIDSTART(x) ((x)=='.'||(x)=='_'||isalpha((unsigned char)(x)))
#define ISIDCHAR(x) isidchar(x)
#define ISBADID(p,l) ((l)==1&&(*(p)=='.'||*(p)=='_'))
#define ISEOL(p) (*(p)=='\0' || *(p)==commentchar)

/* result of a boolean operation */
#define BOOLEAN(x) -(x)

/* we have a special skip() function for expressions, called exp_skip() */
char *exp_skip(char *);
#define EXPSKIP() s=exp_skip(s)

/* support for broken negative hex-constants: $-hex */
#define BROKEN_HEXCONST

/* operator separation characters */
#ifndef OPERSEP_COMMA
#define OPERSEP_COMMA 1
#endif
#ifndef OPERSEP_BLANK
#define OPERSEP_BLANK 0
#endif

/* ignore operand field, when the instruction has no operands */
extern int igntrail;
#define IGNORE_FIRST_EXTRA_OP (igntrail)

/* symbol which contains the current rept-endr iteration count */
#define REPTNSYM "__RPTCNT"

/* overwrite macro defaults */
#define MAXMACPARAMS 35
char *my_skip_macro_arg(char *);
#define SKIP_MACRO_ARGNAME(p) my_skip_macro_arg(p)

/* cpu-specific data sizes */
#if defined(VASM_CPU_650X) || defined(VASM_CPU_SPC700)
#define DATWORD handle_d16
#define SPCWORD handle_spc16
#define DATLONG handle_d24
#define SPCLONG handle_spc24
#define DATDWRD handle_d32
#define SPCDWRD handle_spc32
#else
#define DATWORD handle_d16
#define SPCWORD handle_spc16
#define DATLONG handle_d32
#define SPCLONG handle_spc32
#define DATDWRD handle_d32
#define SPCDWRD handle_spc32
#endif
