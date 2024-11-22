/*
** cpu.h HANS cpu-description header-file
** (c) in 2023 by Frank Wille and Yannik Stamm
*/

#define BIGENDIAN 1
#define LITTLEENDIAN 0
#define BITSPERBYTE 32
#define VASM_CPU_HANS 1

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 3

/* make sure operand is cleared upon first entry into parse_operand() */
#define CLEAR_OPERANDS_ON_START 1

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* minimum instruction alignment */
#define INST_ALIGN 1

/* default alignment for n-bit data */
#define DATA_ALIGN(n) 1

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) Data

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) 1

/* parse cpu-specific directives with label */
/*#define PARSE_CPU_LABEL(l,s) parse_cpu_label(l,s)*/


/* operand types */
enum {
  None=0,                   /* none */
  Data,                     /* n-bit data */
  SourceReg1,               /* general purpose register 1 */
  SourceReg2,               /* general purpose register 2 */
  TargetReg,                /* target general purpose register */
  SourceFloatReg1,          /* floating point register 1 */
  SourceFloatReg2,          /* floating point register 2 */
  TargetFloatReg,           /* target floating point register */
  Immediate16,              /* 16-bit signed immediate for I-format */
  Immediate16Plus1,         /* 16-bit signed immediate for I-format. Immediate increased by 1 */
  Immediate16Minus1,        /* 16-bit signed immediate for I-format. Immediate decreased by 1 */
  Immediate16Label,         /* 16-bit PC-relative label for I-format */
  Immediate26Label          /* 26-bit PC-relative label for J-format */
};

enum
{
    DefaultLabel,
    LowLabel,
    HighLabel,
    HighAlgebraicLabel
};

/* type to store each operand */
typedef struct {
  int reg;
  expr *exp;
  int labelType;
} operand;

/* additional mnemonic data */
typedef struct {
  uint32_t opcode;
} mnemonic_extension;

/* instruction formats */
#define FORMR(x) ((x)&0x3f)
#define FORMI(x) ((0x20|((x)&0x1f))<<26)
#define FORMJ(x) ((0x10|((x)&0xf))<<26)

/* register symbols */
#define HAVE_REGSYMS
#define REGSYMHTSIZE 64
#define RTYPE_R  0       /* Register R0..R31 */
#define RTYPE_F  1       /* Register F0..F31 */

/* exported by cpu.c */
/*int cpu_available(int);*/
/*int parse_cpu_label(char *,char **);*/
