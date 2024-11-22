/*
** cpu.h unSP cpu-description header-file
** (c) in 2021-2024 by Adrien Destugues
*/

#define BIGENDIAN 0
#define LITTLEENDIAN 1
#define BITSPERBYTE 16
#define VASM_CPU_UNSP 1

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 3

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* valid parentheses for cpu's operands */
#define START_PARENTH(x) ((x)=='(' || (x)=='[')
#define END_PARENTH(x) ((x)==')' || (x)==']')

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* minimum instruction alignment */
#define INST_ALIGN 1

/* default alignment for n-bit data */
#define DATA_ALIGN(n) 1

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) MIMM

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) cpu_available(i)

/* we define two additional unary operations, '<' and '>' */
int ext_unary_eval(int,taddr,taddr *,int);
int ext_find_base(symbol **,expr *,section *,taddr);
#define LOBYTE (LAST_EXP_TYPE+1)
#define HIBYTE (LAST_EXP_TYPE+2)
#define EXT_UNARY_NAME(s) (*s=='<'||*s=='>')
#define EXT_UNARY_TYPE(s) (*s=='<'?LOBYTE:HIBYTE)
#define EXT_UNARY_EVAL(t,v,r,c) ext_unary_eval(t,v,r,c)
#define EXT_FIND_BASE(b,e,s,p) ext_find_base(b,e,s,p)

/* operand types */
enum {
  _NO_OP = 0, /* no operand */
  MIMM6   = 1, /* 6-bit immediate */
  MIMM16  = 2, /* 16-bit immediate */
  MIMM    = 3, /* 6 or 16-bit immediate */
  MBP6    = 4, /* BP-relative */
  MA6     = 8, /* 6 bit address */
  MA16    = 16, /* 16 bit address */
  MADDR   = MA6 | MA16, /* 6 or 16-bit address */
  MREG    = 32, /* register */
  MREGA   = 64, /* address register */
  MANY    = 127, /* any of the above */
  MMEMORY = MBP6 | MADDR | MREGA, /* Somewhere in memory */
  MA22    = 128, /* 22 bit address */
  MEXT    = MA22 | MA16 | MIMM16, /* needs a second 16 bit word */
  MPC6    = 256, /* 6-bit PC relative */
  MVALUE  = MADDR | MIMM | MBP6 | MPC6,
  M6BIT   = MIMM6 | MA6 | MBP6 | MPC6,
  M16BIT  = MIMM16 | MA16,
  MONOFF  = 512, /* on/off flag */
  MIRQ    = 1024 /* irq flag */
};

enum {
	_NO_FLAGS = 0,
	FPOSTDEC,
	FPOSTINC,
	FPREINC,
	FDSEG
};

/* type to store each operand */
typedef struct {
  uint16_t mode;
  uint16_t flags;
  int8_t reg;
  expr *value;
} operand;

/* evaluated expressions */
struct MyOpVal {
  taddr value;
  symbol *base;
  int btype;
};

/* additional mnemonic data */
typedef struct {
  uint16_t opcode;
  uint8_t format;
  uint8_t flags;
} mnemonic_extension;

/* instruction format */
enum {
  NO,        /* no operand */
  ALU,       /* ALU instructions */
  STORE,     /* ST instruction */
  ALUSTORE,  /* 3-op ALU instructions with destination address */
  PUSH,      /* PUSH */
  POP,       /* POP */
  MUL1,      /* MUL */
  MAC,       /* MAC */
  SO,        /* oooooooooommmrrr: single operand */
  XJMP,      /* extended conditional jump pseudo-instruction */
  FIRMOV,    /* FIR_MOV */
  FIQ        /* FIQ */
};

/* register symbols */
#define HAVE_REGSYMS
#define REGSYMHTSIZE 64
#define RTYPE_R  0       /* Register R0..R7 */

/* exported by cpu.c */
int cpu_available(int);
/*int parse_cpu_label(char *,char **);*/
