/*
 * cpu.c Jaguar RISC cpu description file
 * (c) in 2014-2017,2020,2021,2024,2025 by Frank Wille
 */

#include "vasm.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};
const int mnemonic_cnt = sizeof(mnemonics) / sizeof(mnemonics[0]);

const char *cpu_copyright = "vasm Jaguar RISC cpu backend 0.7 (c) 2014-2017,2020,2021,2024,2025 Frank Wille";
const char *cpuname = "jagrisc";
int bytespertaddr = 4;

int jag_big_endian = 1;  /* defaults to big-endian (Atari Jaguar 68000) */

static int noopt;        /* disable std. optimizations without side effects */
static int optjr = -1;   /* x=0..31: translation of JR to MOVEI/JUMP (Rx) */
static uint8_t cpu_type = GPU|DSP;
static int OC_MOVEI,OC_MOVEQ,OC_UNPACK,OC_JRABS,OC_JUMP;

/* condition codes */
static regsym cc_regsyms[] = {
  {"t",    RTYPE_CC, 0, 0x00},
  {"a",    RTYPE_CC, 0, 0x00},
  {"ne",   RTYPE_CC, 0, 0x01},
  {"eq",   RTYPE_CC, 0, 0x02},
  {"cc",   RTYPE_CC, 0, 0x04},
  {"hs",   RTYPE_CC, 0, 0x04},
  {"hi",   RTYPE_CC, 0, 0x05},
  {"cs",   RTYPE_CC, 0, 0x08},
  {"lo",   RTYPE_CC, 0, 0x08},
  {"pl",   RTYPE_CC, 0, 0x14},
  {"mi",   RTYPE_CC, 0, 0x18},
  {"f",    RTYPE_CC, 0, 0x1f},
  {"nz",   RTYPE_CC, 0, 0x01},
  {"z",    RTYPE_CC, 0, 0x02},
  {"nc",   RTYPE_CC, 0, 0x04},
  {"ncnz", RTYPE_CC, 0, 0x05},
  {"ncz" , RTYPE_CC, 0, 0x06},
  {"c",    RTYPE_CC, 0, 0x08},
  {"cnz" , RTYPE_CC, 0, 0x09},
  {"cz",   RTYPE_CC, 0, 0x0a},
  {"nn",   RTYPE_CC, 0, 0x14},
  {"nnnz", RTYPE_CC, 0, 0x15},
  {"nnz",  RTYPE_CC, 0, 0x16},
  {"n",    RTYPE_CC, 0, 0x18},
  {"n_nz", RTYPE_CC, 0, 0x19},
  {"n_z",  RTYPE_CC, 0, 0x1a},
  {NULL, 0, 0, 0}
};


int init_cpu(void)
{
  int i;
  regsym *r;

  for (i=0; i<mnemonic_cnt; i++) {
    if (!strcmp(mnemonics[i].name,"movei"))
      OC_MOVEI = i;
    else if (!strcmp(mnemonics[i].name,"moveq"))
      OC_MOVEQ = i;
    else if (!strcmp(mnemonics[i].name,"unpack"))
      OC_UNPACK = i;
    else if (!strcmp(mnemonics[i].name,"jump"))
      OC_JUMP = i;
    else if (!strcmp(mnemonics[i].name," jrabs"))
      OC_JRABS = i;
  }

  /* define all condition code register symbols */
  for (r=cc_regsyms; r->reg_name!=NULL; r++)
    add_regsym(r);

  return 1;
}


int cpu_args(char *p)
{
  if (!strncmp(p,"-m",2)) {
    p += 2;
    if (!stricmp(p,"gpu") || !stricmp(p,"tom"))
      cpu_type = GPU;
    else if (!stricmp(p,"dsp") || !stricmp(p,"jerry"))
      cpu_type = DSP;
    else if (!strcmp(p,"any"))
      cpu_type = GPU|DSP;
    else
      return 0;
  }
  else if (!strcmp(p,"-no-opt")) {
    optjr = -1;
    noopt = 1;
  }
  else if (!strncmp(p,"-opt-jr=",8)) {
    p += 8;
    if (toupper((unsigned char)*p) == 'R')
      p++;
    optjr = atoi(p) & 31;
  }
  else if (!strcmp(p,"-big"))
    jag_big_endian = 1;
  else if (!strcmp(p,"-little"))
    jag_big_endian = 0;
  else
    return 0;

  return 1;
}


static int parse_reg(char **p)
{
  int reg = -1;
  char *rp = skip(*p);
  char *s;

  if (s = skip_identifier(rp)) {
    regsym *sym = find_regsym(rp,s-rp);

    if (sym!=NULL && sym->reg_type==RTYPE_R) {
      reg = sym->reg_num;
    }
    else if (toupper((unsigned char)*rp++) == 'R') {
      if (sscanf(rp,"%d",&reg)!=1 || reg<0 || reg>31)
        reg = -1;
    }

    if (reg >= 0)
      *p = s;
  }

  return reg;
}


static expr *parse_cc(char **p)
{
  char *end;

  *p = skip(*p);

  if (end = skip_identifier(*p)) {
    regsym *sym = find_regsym_nc(*p,end-*p);

    if (sym!=NULL && sym->reg_type==RTYPE_CC) {
      *p = end;
      return number_expr((taddr)sym->reg_num);
    }
  }

  /* otherwise the condition code is any expression */
  return parse_expr(p);
}


static void jagrel5(rlist **rl,MyOpVal *opval,size_t offs)
{
  if (opval->base) {
    switch (opval->btype) {
      case BASE_OK:  /* Immediate or CC values */
        add_extnreloc(rl,opval->base,opval->val,REL_ABS,offs,5,0);
        break;
      case BASE_PCREL:  /* JR instruction */
        add_extnreloc_masked(rl,opval->base,opval->val,REL_PC,offs,5,0,~1);
        break;
      case BASE_ILLEGAL:
        general_error(38);  /* illegal relocation */
        break;
      default:
        ierror(0);
        break;
    }
  }
}


static void jagrelswap32(rlist **rl,size_t o,symbol *base,int btype,taddr val)
{
  if (base) {
    if (btype != BASE_ILLEGAL) {
      /* swapped: two relocations for LSW first, then MSW */
      add_extnreloc_masked(rl,base,val,btype==BASE_PCREL?REL_PC:REL_ABS,
                           0,16,o,0xffff);
      add_extnreloc_masked(rl,base,val,btype==BASE_PCREL?REL_PC:REL_ABS,
                           16,16,o,0xffff0000);
    }
    else
      general_error(38);  /* illegal relocation */
  }
}


static void jagswap32(unsigned char *d,int32_t w)
/* write a 32-bit word with swapped halves (Jaguar MOVEI) */
{
  if (jag_big_endian) {
    *d++ = (w >> 8) & 0xff;
    *d++ = w & 0xff;
    *d++ = (w >> 24) & 0xff;
    *d = (w >> 16) & 0xff;
  }
  else {
    /* @@@ Need to verify this! */
    *d++ = w & 0xff;
    *d++ = (w >> 8) & 0xff;
    *d++ = (w >> 16) & 0xff;
    *d = (w >> 24) & 0xff;
  }
}


char *parse_cpu_special(char *start)
/* parse cpu-specific directives; return pointer to end of cpu-specific text */
{
  strbuf *buf;
  char *name=start;
  char *s;

  if (s = skip_identifier(name)) {
    /* Atari MadMac compatibility directives */
    if (dotdirs && *name=='.')  /* ignore leading dot */
      name++;

    if (s-name==3 && !strnicmp(name,"dsp",3)) {
      cpu_type = DSP;
      eol(s);
      return skip_line(s);
    }

    else if (s-name==3 && !strnicmp(name,"gpu",3)) {
      cpu_type = GPU;
      eol(s);
      return skip_line(s);
    }

    else if (s-name==8 && !strnicmp(name,"regundef",8) ||
             s-name==9 && !strnicmp(name,"equrundef",9)) {
      /* undefine a register symbol */
      s = skip(s);
      if (buf = parse_identifier(0,&s)) {
        undef_regsym(buf->str,0,RTYPE_R);
        eol(s);
        return skip_line(s);
      }
    }

    else if (s-name==7 && !strnicmp(name,"ccundef",7)) {
      /* undefine a condition code symbol */
      s = skip(s);
      if (buf = parse_identifier(0,&s)) {
        undef_regsym(strtolower(buf->str),0,RTYPE_CC);
        eol(s);
        return skip_line(s);
      }
    }
  }

  return start;
}


int parse_cpu_label(char *labname,char **start)
/* parse cpu-specific directives following a label field,
   return zero when no valid directive was recognized */ 
{
  char *dir=*start;
  char *s;

  if (dotdirs && *dir=='.')  /* ignore leading dot */
    dir++;

  if (s = skip_identifier(dir)) {

    if (s-dir==6 && !strnicmp(dir,"regequ",6) ||
        s-dir==4 && !strnicmp(dir,"equr",4)) {
      /* label REGEQU Rn || label EQUR Rn */
      int r;

      if ((r = parse_reg(&s)) >= 0)
        new_regsym(0,0,labname,RTYPE_R,0,r);
      else
        cpu_error(2);  /* register expected */
      eol(s);
      *start = skip_line(s);
      return 1;
    }

    else if (s-dir==5 && !strnicmp(dir,"ccdef",5)) {
      /* label CCDEF expr */
      expr *ccexp;
      taddr val;

      if ((ccexp = parse_cc(&s)) != NULL) {
        if (eval_expr(ccexp,&val,NULL,0))
          new_regsym(0,0,labname,RTYPE_CC,0,(int)val);
        else
          general_error(30);  /* expression must be a constant */
      }
      else
        general_error(9);  /* @@@ */
      eol(s);
      *start = skip_line(s);
      return 1;
    }
  }

  return 0;
}


operand *new_operand(void)
{
  operand *new = mymalloc(sizeof(*new));

  new->type = NO_OP;
  return new;
}


int jag_data_operand(int bits)
/* return data operand type for these number of bits */
{
  if (bits & OPSZ_SWAP)
    return DATAI_OP;
  return bits==64 ? DATA64_OP : DATA_OP;
}


int parse_operand(char *p, int len, operand *op, int required)
{
  int reg;

  switch (required) {
    case IMM0:
    case IMM1:
    case IMM1S:
    case SIMM:
    case IMMLW:
      if (*p == '#')
        p = skip(p+1);  /* skip optional '#' */
    case REL:
    case DATA_OP:
    case DATAI_OP:
      if (required == IMM1S) {
        op->val = make_expr(SUB,number_expr(32),parse_expr(&p));
        required = IMM1;  /* turn into IMM1 32-val for SHLQ */
      }
      else
        op->val = parse_expr(&p);
      break;

    case DATA64_OP:
      op->val = parse_expr_huge(&p);
      break;

    case REG:  /* Rn */
      op->reg = parse_reg(&p);
      if (op->reg < 0)
        return PO_NOMATCH;
      break;

    case IREG:  /* (Rn) */
      if (*p++ != '(')
        return PO_NOMATCH;
      op->reg = parse_reg(&p);
      if (op->reg < 0)
        return PO_NOMATCH;
      if (*p != ')')
        return PO_NOMATCH;
      break;

    case IR14D:  /* (R14+d) */
    case IR15D:  /* (R15+d) */
      if (*p++ != '(')
        return PO_NOMATCH;
      reg = parse_reg(&p);
      if ((required==IR14D && reg!=14) || (required==IR15D && reg!=15))
        return PO_NOMATCH;
      if (*p++ != '+')
        return PO_NOMATCH;
      p = skip(p);
      op->val = parse_expr(&p);
      p = skip(p);
      if (*p != ')')
        return PO_NOMATCH;
      break;

    case IR14R:  /* (R14+Rn) */
    case IR15R:  /* (R15+Rn) */
      if (*p++ != '(')
        return PO_NOMATCH;
      reg = parse_reg(&p);
      if ((required==IR14R && reg!=14) || (required==IR15R && reg!=15))
        return PO_NOMATCH;
      if (*p++ != '+')
        return PO_NOMATCH;
      op->reg = parse_reg(&p);
      if (op->reg < 0)
        return PO_NOMATCH;
      if (*p != ')')
        return PO_NOMATCH;
      break;

    case CC:  /* condition code: t, eq, ne, mi, pl, cc, cs, ... */
      op->val = parse_cc(&p);
      break;

    case PC:  /* PC register */
      if (toupper((unsigned char)*p) != 'P' ||
          toupper((unsigned char)*(p+1)) != 'C' ||
          ISIDCHAR(*(p+2)))
        return PO_NOMATCH;
      break;

    default:
      return PO_NOMATCH;
  }

  op->type = required;
  return PO_MATCH;
}


static size_t process_instruction(instruction *ip,section *sec,taddr pc,
                                  MyOpVal values[MAX_OPERANDS+1],int final)
{
  int i,btype,optype;
  operand *op;
  size_t size;
  symbol *base;
  taddr val;

  for (i=0,size=2; i<MAX_OPERANDS; i++) {
    val = 0;
    base = NULL;
    btype = BASE_ILLEGAL;

    if ((op = ip->op[i]) != NULL) {
      switch (optype = op->type) {
        case REG:
        case IREG:
        case IR14R:
        case IR15R:
          val = op->reg;
          break;

        case IMM0:
        case IMM1:
        case SIMM:
        case IMMLW:
        case IR14D:
        case IR15D:
        case REL:
        case CC:
          if (!eval_expr(op->val,&val,sec,pc)) {
            btype = find_base(op->val,&base,sec,pc);
            if (final && btype == BASE_ILLEGAL)
              general_error(38);  /* illegal relocation */
          }

          if (optype == REL) {
            if ((base!=NULL && btype==BASE_OK && !is_pc_reloc(base,sec))
                || base == NULL) {
              /* known label from same section or absolute label */
              taddr d = (val - (pc + 2)) / 2;

              if (d<-16 || d>15) {
                if (optjr >= 0) {
                  /* JR [cc,]lab -> MOVEI lab,Rx + JUMP [cc,]Rx */
                  ip->code = OC_JRABS;
                  values[MAX_OPERANDS].val = val;  /* MOVEI 32-bit */
                  values[MAX_OPERANDS].base = base;
                  values[MAX_OPERANDS].btype = btype;
                  val = optjr & 31;  /* Rx */
                  size += 6;
                }
                else if (final)
                  cpu_error(1,-16,15);
              }
              else
                val = d;
            }
            else if (btype == BASE_OK) {
              /* external label or from a different section (distance / 2) */
              if (optjr >= 0) {
                /* JR [cc,]lab -> MOVEI lab,Rx + JUMP [cc,]Rx */
                ip->code = OC_JRABS;
                values[MAX_OPERANDS].val = val;  /* MOVEI 32-bit */
                values[MAX_OPERANDS].base = base;
                values[MAX_OPERANDS].btype = btype;
                val = optjr & 31;  /* Rx */
                size += 6;
              }
              else {
                val -= 2;
                btype = BASE_PCREL;
                break;  /* add relocation */
              }
            }
            base = NULL;
            btype = BASE_ILLEGAL;
          }
          else if (optype == IMMLW) {
            if (!noopt && base==NULL && val>=0 && val<=31) {
              /* optimize to MOVEQ */
              ip->code = OC_MOVEQ;
            }
            else {
              values[MAX_OPERANDS].val = val;  /* MOVEI 32-bit */
              values[MAX_OPERANDS].base = base;
              values[MAX_OPERANDS].btype = btype;
              val = 0;
              base = NULL;
              btype = BASE_ILLEGAL;
              size += 4;
            }
          }
          else if (optype==IR14D || optype==IR15D) {
            if (!noopt && base==NULL && val==0) {
              /* Optimize (Rn+0) to (Rn). Assume that load/store (Rn) is three
                 entries before (R14+d) and four entries before (R15+d). */
              ip->code -= optype==IR14D ? 3 : 4;  /* @OPT1@ */
              val = optype==IR14D ? 14 : 15;
            }
            else if (final && base==NULL && (val<1 || val>32))
              cpu_error(1,1,32);
          }
          else if (optype==IMM1) {
            if (final && base==NULL && (val<1 || val>32))
              cpu_error(1,1,32);
          }
          else if (optype==SIMM) {
            if (final && base==NULL && (val<-16 || val>15))
              cpu_error(1,-16,15);
          }
          else {
            if (final && base==NULL && (val<0 || val>31))
              cpu_error(1,0,31);
          }
          break;

        default:
          ierror(0);
        case PC:
          break;
      }
    }
    else if (i != 0) {
      /* operand missing: take values from previous operand (destOp = srcOp) */
      val = values[i-1].val;
      base = values[i-1].base;
      btype = values[i-1].btype;

      /* and reset previous (source) operand field */
      values[i-1].val = ip->code==OC_UNPACK ? 1 : 0;
      values[i-1].base = NULL;
      values[i-1].btype = BASE_ILLEGAL;
    }

    values[i].val = val;
    values[i].base = base;
    values[i].btype = btype;
  }

  return size;
}


size_t instruction_size(instruction *ip, section *sec, taddr pc)
{
  MyOpVal values[MAX_OPERANDS+1];

  return process_instruction(copy_inst(ip),sec,pc,values,0);
}


dblock *eval_instruction(instruction *ip, section *sec, taddr pc)
{
  MyOpVal values[MAX_OPERANDS+1];
  dblock *db = new_dblock();
  uint16_t inst;
  uint8_t flags;

  /* evaluate operands, optimize instruction and determine its size */
  db->size = process_instruction(ip,sec,pc,values,1);
  db->data = mymalloc(db->size);

  /* store and jump instructions need the second operand in the source field */
  flags = mnemonics[ip->code].ext.flags;
  if (flags & OPSWAP) {
    MyOpVal swap = values[0];
    values[0] = values[1];
    values[1] = swap;
  }

  /* construct the instruction word out of opcode and source/dest. value */
  inst = (mnemonics[ip->code].ext.opcode & 63) << 10;
  if (!(flags & EXTRA32)) {
    inst |= (values[0].val & 31) << 5;
    jagrel5(&db->relocs,&values[0],jag_big_endian?6:5);
  }
  inst |= values[1].val & 31;
  jagrel5(&db->relocs,&values[1],jag_big_endian?11:0);
  setval(jag_big_endian,db->data,2,inst);

  /* write extra words for MOVEI and JRABS */
  if (ip->code == OC_MOVEI) {
    if (db->size != 6)
      ierror(0);
    /* extra words for MOVEI are always written in the order lo-, hi-word */
    jagswap32(&db->data[2],values[2].val);
    jagrelswap32(&db->relocs,2,values[2].base,values[2].btype,values[2].val);
  }
  else if (ip->code == OC_JRABS) {
    if (db->size != 8)
      ierror(0);
    /* write jump-address as MOVEI extra-word */
    jagswap32(&db->data[2],values[2].val);
    jagrelswap32(&db->relocs,2,values[2].base,values[2].btype,values[2].val);
    /* followed by an indirect jump using the optjr register */
    inst = (mnemonics[OC_JUMP].ext.opcode & 63) << 10;
    inst |= ((values[0].val & 31) << 5) | (values[1].val & 31);
    jagrel5(&db->relocs,&values[1],jag_big_endian?11:0);
    setval(jag_big_endian,&db->data[6],2,inst);
  }
  else if (db->size != 2)
    ierror(0);

  return db;
}


dblock *eval_data(operand *op, size_t bitsize, section *sec, taddr pc)
{
  dblock *db = new_dblock();
  taddr val;

  if (bitsize!=8 && bitsize!=16 && bitsize!=32 && bitsize!=64)
    cpu_error(0,bitsize);  /* data size not supported */

  if (op->type!=DATA_OP && op->type!=DATA64_OP && op->type!=DATAI_OP)
    ierror(0);

  db->size = bitsize >> 3;
  db->data = mymalloc(db->size);

  if (op->type == DATA64_OP) {
    thuge hval;

    if (!eval_expr_huge(op->val,&hval))
      general_error(59);  /* cannot evaluate huge integer */
    huge_to_mem(jag_big_endian,db->data,db->size,hval);
  }
  else {
    if (!eval_expr(op->val,&val,sec,pc)) {
      symbol *base;
      int btype;

      btype = find_base(op->val,&base,sec,pc);
      if (base!=NULL && btype!=BASE_ILLEGAL) {
        if (op->type == DATAI_OP)
          jagrelswap32(&db->relocs,0,base,btype,val);
        else /* normal 8, 16, 32 bit relocation */
          add_extnreloc(&db->relocs,base,val,
                        btype==BASE_PCREL?REL_PC:REL_ABS,0,bitsize,0);
      }
      else
        general_error(38);  /* illegal relocation */
    }

    switch (db->size) {
      case 1:
        db->data[0] = val & 0xff;
        break;
      case 2:
      case 4:
        if (op->type == DATAI_OP)
          jagswap32(db->data,val);
        else
          setval(jag_big_endian,db->data,db->size,val);
        break;
      default:
        ierror(0);
        break;
    }
  }

  return db;
}


int cpu_available(int idx)
{
  return (mnemonics[idx].ext.flags & cpu_type) != 0;
}
