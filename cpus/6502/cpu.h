/*
** cpu.h 650x/65C02/6280/45gs02/65816 cpu-description header-file
** (c) in 2002,2008,2009,2014,2018,2020,2021,2022,2024 by Frank Wille
*/

#define BIGENDIAN 0
#define LITTLEENDIAN 1
#define BITSPERBYTE 8
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
  uint8_t asize;
  uint8_t xsize;
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

/* type to store each operand */
typedef struct {
  int type;
  unsigned flags;
  expr *value;
} operand;

/* operand flags */
#define OF_LO (1<<0)  /* '<' selects low-byte or 8-bit addressing */
#define OF_HI (1<<1)  /* '>' selects high-byte or 16/24-bit addressing */
#define OF_BK (1<<2)  /* '^' or '`' selects bank-byte */
#define OF_ID (1<<3)  /* '?' selects the symbol's memory/bank-id */
#define OF_WA (1<<4)  /* '!' or '|' selects 16-bit addressing */
#define OF_PC (1<<5)  /* PC-relative */


/* additional mnemonic data */
typedef struct {
  uint8_t opcode;
  uint8_t zp_opcode;  /* !=0 means optimization to zero page allowed */
  uint8_t al_opcode;  /* !=0 means translation to absolute long allowed */
  uint16_t available;
} mnemonic_extension;

/* available */
#define M6502    (1<<0)   /* standard 6502 instruction set */
#define ILL      (1<<1)   /* illegal 6502 instructions */
#define DTV      (1<<2)   /* C64 DTV instruction set extension */
#define M65C02   (1<<3)   /* basic 65C02 extensions on 6502 instruction set */
#define WDC02    (1<<4)   /* basic WDC65C02 extensions on 65C02 instr. set */
#define WDC02ALL (1<<5)   /* all WDC65C02 extensions on 65C02 instr. set */
#define CSGCE02  (1<<6)   /* CSG65CE02 extensions on WDC65C02 instruction set */
#define HU6280   (1<<7)   /* HuC6280 extensions on WDC65C02 instruction set */
#define M45GS02  (1<<8)   /* MEGA65 45GS02, extends WDC02 instruction set */
#define M45GS02Q (1<<9)   /* MEGA65 quad instr., extends WDC02 instr.set */
#define WDC65816 (1<<10)  /* WDC65816/65802 extensions on WDC65C02 instr. set */


/* Addressing modes (operand type) */
/* CAUTION: Order is important! */
/* See macros below and adapt the opsize[] array in cpu.c accordingly! */
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
  QDPIND,       /* [$12] - 45GS02 with prefix */
  RELJMP,       /* B!cc/JMP construction */
  REL8,         /* $1234 - 8-bit signed relative branch */
  REL16,        /* $1234 - 16-bit signed relative branch */
  IMMED,        /* #$12 or #$1234 in 65816 Accumulator 16-bit mode */
  IMMEDX,       /* #$12 or #$1234 in 65816 Index 16-bit mode */
  IMMED8,       /* #$12 */
  IMMED16,      /* #$1234 */
  WBIT,         /* bit-number (WDC65C02) */
  MVBANK,       /* memory bank number for WDC65816 MVN/MVP */
  ACCU,         /* A - all following addressing modes don't need a value! */
  NUM_OPTYPES
};
#define IS_INDIR(x) ((x)>=INDIR && (x)<=INDIRX)
#define IS_SQIND(x) ((x)>=LINDIR && (x)<=QDPIND)
#define IS_ABS(x) ((x)>=ABS && (x)<=ABSZ)
/* CAUTION:
   - opsize[] gives the size in bytes for each addressing mode listed above!
   - Do not change the order of ABS,ABSX/Y/Z, LABS/X and DPAGE,DPAGEX/Y/Z
   - All addressing modes >=ACCU (and IMPLIED) do not require a value!
   - All indirect addressing modes are defined by IS_INDIR()
   - All indirect long (square brackets) addr.modes are defined by IS_SQIND()
   - All absolute addressing modes which can be converted to zero/direct-page
     or long (24-bit) are defined by IS_ABS()
*/

/* cpu-specific symbol-flags */
#define ZPAGESYM (RSRVD_C<<0)   /* symbol will reside in the zero/direct-page */


/* exported by cpu.c */
extern uint16_t cpu_type;
int cpu_available(int);
int parse_cpu_label(char *,char **);
