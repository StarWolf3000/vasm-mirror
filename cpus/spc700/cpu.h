/*
** cpu.h spc-700 cpu-description header-file
** (c) in 2025 by Frank Wille
*/

#define BIGENDIAN 0
#define LITTLEENDIAN 1
#define BITSPERBYTE 8
#define VASM_CPU_SPC700 1

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 2

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* valid parentheses for cpu's operands */
#define START_PARENTH(x) ((x)=='[')
#define END_PARENTH(x) ((x)==']')

/* data type to represent a target-address */
typedef int16_t taddr;
typedef uint16_t utaddr;

/* we use OPTS atoms for cpu-specific options */
#define HAVE_CPU_OPTS 1
typedef struct {
  uint16_t dpage;
} cpuopts;

/* minimum instruction alignment */
#define INST_ALIGN 1

/* default alignment for n-bit data */
#define DATA_ALIGN(n) 1

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) DATAOP

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) 1

/* parse cpu-specific directives with label */
#define PARSE_CPU_LABEL(l,s) parse_cpu_label(l,s)

/* type to store each operand */
typedef struct {
  int type;
  unsigned flags;
  expr *value;
  expr *bitval;
} operand;

/* operand flags */
#define OF_LO (1<<0)  /* '<' selects low-byte */
#define OF_HI (1<<1)  /* '>' selects high-byte */
#define OF_WA (1<<2)  /* '!' force 16-bit addressing mode */
#define OF_ID (1<<3)  /* '?' selects the symbol's memory/bank-id */
#define OF_PC (1<<4)  /* PC-relative */


/* additional mnemonic data */
typedef struct {
  uint8_t opcode;
  uint8_t dp_opcode;  /* !=0 means optimization to direct page allowed */
} mnemonic_extension;


/* Addressing modes (operand type) */
/* CAUTION: Order is important! */
/* See macros below and adapt the opsize[] array in cpu.c accordingly! */
enum {
  /* all addressing modes until DATAOP don't store a value */
  IMPLIED=0,    /* no operand, must be zero */
  ACCU,         /* A */
  XREG,         /* X */
  XIND,         /* (X) */
  XINDINC,      /* (X)+ */
  YREG,         /* Y */
  YIND,         /* (Y) */
  YACCU,        /* YA - 16-bit pair */
  PSW,          /* PSW */
  CFLAG,	/* C */
  SPREG,        /* SP */

  /* all following addressing modes store an expression in 'value' */
  DATAOP,       /* data operand */
  PCALL,        /* $12 - LSB of CALL $FFxx */
  IMMED,        /* #$12 */
  ABS,          /* !$1234 */
  ABSX,         /* !$1234+X */
  ABSY,         /* !$1234+Y */
  DPAGE,        /* $12 - add DPAGE-ABS to optimize ABS/ABSX/ABSY */
  DPAGEX,       /* $12+X */
  DPAGEY,       /* $12+Y */
  DPINDX,       /* [$12+X] */
  DPINDY,       /* [$12]+Y */
  JMPINDX,      /* [$1234+X] */
  DPBIT,        /* $12.0 - store bit in 'bitval' */
  ABSBIT,       /* $1234.0 - 13 bit address, store bit in 'bitval' */
  ABSBITN,      /* /$1234.0 - 13 bit address with negated bit */
  TCALL,        /* $1 - calls one of 16 vectors  $FFC0 - $FFDE */
  BREL,         /* $1234 - 8-bit signed relative branch */
  RELJMP,       /* B!cc/JMP construction */
  NUM_OPTYPES
};
#define IS_ABS(x) ((x)>=ABS && (x)<=ABSY)
#define IS_INDIR(x) ((x)>=DPINDX && (x)<=JMPINDX)

/* cpu-specific symbol-flags */
#define DPAGESYM (RSRVD_C<<0)   /* symbol will reside in the direct-page */


/* exported by cpu.c */
int parse_cpu_label(char *,char **);
