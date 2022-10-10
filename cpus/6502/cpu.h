/*
** cpu.h 650x/65C02/6280/45gs02/65816 cpu-description header-file
** (c) in 2002,2008,2009,2014,2018,2020,2021,2022 by Frank Wille
*/

#define BIGENDIAN 0
#define LITTLEENDIAN 1
#define VASM_CPU_650X 1

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 3

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* we use OPTS atoms for cpu-specific options */
#define HAVE_CPU_OPTS 1
typedef struct {
  uint16_t dpage;
  char asize;
  char xsize;
} cpuopts;

/* minimum instruction alignment */
#define INST_ALIGN 1

/* default alignment for n-bit data */
#define DATA_ALIGN(n) 1

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) DATAOP

/* make sure operands are cleared when parsing a new mnemonic */
#define CLEAR_OPERANDS_ON_MNEMO 1

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) cpu_available(i)

/* parse cpu-specific directives with label */
#define PARSE_CPU_LABEL(l,s) parse_cpu_label(l,s)

/* we define three additional unary operations, '<', '>' and '^' */
int ext_unary_name(char *);
int ext_unary_type(char *);
int ext_unary_eval(int,taddr,taddr *,int);
int ext_find_base(symbol **,expr *,section *,taddr);
#define LOBYTE (LAST_EXP_TYPE+1)
#define HIBYTE (LAST_EXP_TYPE+2)
#define BANKBYTE (LAST_EXP_TYPE+3)
#define EXT_UNARY_NAME(s) ext_unary_name(s)
#define EXT_UNARY_TYPE(s) ext_unary_type(s)
#define EXT_UNARY_EVAL(t,v,r,c) ext_unary_eval(t,v,r,c)
#define EXT_FIND_BASE(b,e,s,p) ext_find_base(b,e,s,p)

/* type to store each operand */
typedef struct {
  int type;
  unsigned flags;
  expr *value;
} operand;

/* operand flags */
#define OF_LO (1<<0)
#define OF_HI (1<<1)
#define OF_BK (1<<2)
#define OF_PC (1<<3)


/* additional mnemonic data */
typedef struct {
  uint8_t opcode;
  uint8_t zp_opcode;  /* !=0 means optimization to zero page allowed */
  uint8_t al_opcode;  /* !=0 means translation to absolute long allowed */
  uint16_t available;
} mnemonic_extension;

/* available */
#define M6502    1       /* standard 6502 instruction set */
#define ILL      2       /* illegal 6502 instructions */
#define DTV      4       /* C64 DTV instruction set extension */
#define M65C02   8       /* basic 65C02 extensions on 6502 instruction set */
#define WDC02    16      /* basic WDC65C02 extensions on 65C02 instr. set */
#define WDC02ALL 32      /* all WDC65C02 extensions on 65C02 instr. set */
#define CSGCE02  64      /* CSG65CE02 extensions on WDC65C02 instruction set */
#define HU6280   128     /* HuC6280 extensions on WDC65C02 instruction set */
#define M45GS02  256     /* MEGA65 45GS02 extensions on basic WDC02 instr.set */
#define WDC65816 512     /* WDC65816/65802 extensions on WDC65C02 instr. set */


/* addressing modes */
enum {
  IMPLIED=0,    /* no operand, must be zero */
  DATAOP,       /* data operand */
  ABS,          /* $1234 */
  ABSX,         /* $1234,X */
  ABSY,         /* $1234,Y */
  ABSZ,         /* $1234,Z */
  LABS,         /* $123456 - add LABS-ABS to translate from ABS/ABSX */
  LABSX,        /* $123456,X */
  DPAGE,        /* $12 - add DPAGE-ABS to optimize ABS/ABSX/ABSY/ABSZ */
  DPAGEX,       /* $12,X */
  DPAGEY,       /* $12,Y */
  DPAGEZ,       /* $12,Z */
  SR,           /* $12,S */
  INDIR,        /* ($1234) - JMP only */
  DPINDX,       /* ($12,X) */
  DPINDY,       /* ($12),Y */
  DPINDZ,       /* ($12),Z */
  DPIND,        /* ($12) */
  SRINDY,       /* ($12,S),Y */
  INDIRX,       /* ($1234,X) - JMP only */
  LINDIR,       /* [$1234] - JML only */
  LDPINDY,      /* [$12],Y */
  QDPINDZ,      /* [$12],Z - 45GS02 with prefix */
  LDPIND,       /* [$12] */
  RELJMP,       /* B!cc/JMP construction */
  REL8,         /* $1234 - 8-bit signed relative branch */
  REL16,        /* $1234 - 16-bit signed relative branch */
  IMMED,        /* #$12 or #$1234 in 65816 Accumulator 16-bit mode */
  IMMEDX,       /* #$12 or #$1234 in 65816 Index 16-bit mode */
  IMMED8,       /* #$12 */
  IMMED16,      /* #$1234 */
  WBIT,         /* bit-number (WDC65C02) */
  MVBANK,       /* memory bank number for WDC65816 MVN/MVP */
  ACCU          /* A - all following addressing modes don't need a value! */
};
#define IS_ABS(x) ((x)>=ABS && (x)<=ABSZ)
/* check for a MEGA65 quad instruction - maybe make that nicer some time ... */
#define ISQINST(c) (mnemonics[c].name[3]=='q' || (mnemonics[c].name[2] == 'q' && mnemonics[c].name[0] != 'b'))
#define FIRST_INDIR INDIR
#define LAST_INDIR INDIRX
#define FIRST_SQIND LINDIR
#define LAST_SQIND LDPIND
/* CAUTION:
   - Do not change the order of ABS,ABSX/Y/Z, LABS/X and DPAGE,DPAGEX/Y/Z
   - All addressing modes >=ACCU (and IMPLIED) do not require a value!
   - All indirect addressing modes are between FIRST_INDIR and LAST_INDIR!
   - All indirect long (square brackets) addressing modes are between
     FIRST_SQIND and LAST_SQIND
*/

/* cpu-specific symbol-flags */
#define ZPAGESYM (RSRVD_C<<0)   /* symbol will reside in the zero/direct-page */


/* exported by cpu.c */
extern uint16_t cpu_type;
int cpu_available(int);
int parse_cpu_label(char *,char **);
