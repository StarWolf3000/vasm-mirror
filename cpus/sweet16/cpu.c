/*
 * cpu.c SWEET16 cpu description file
 * Refer to: http://www.6502.org/source/interpreters/sweet16.htm
 */

#include "vasm.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};
const int mnemonic_cnt = sizeof(mnemonics) / sizeof(mnemonics[0]);

const char *cpu_copyright = "vasm sweet16 cpu backend 0.1a (c)2026 Frank Wille";
const char *cpuname = "sweet16";
int bytespertaddr = 2;


int parse_operand(char *p,int len,operand *op,int required)
{
  char *start = p;

  p = skip(p);
  if (*p == '@') {
    if (required != MRIN)
      return PO_NOMATCH;
    p = skip(++p);
  }
  else if (required == MRIN)
    return PO_NOMATCH;

  op->val = parse_expr(&p);

  if (skip(p)-start < len) {
    cpu_error(0);  /* trailing garbage */
    return PO_CORRUPT;
  }
  op->mode = required;
  return PO_MATCH;
}


char *parse_cpu_special(char *s)
{
  return s;  /* nothing special */
}


size_t instruction_size(instruction *ip,section *sec,taddr pc)
{
  if (ip->op[1] != NULL)
    return 3;  /* SET instruction is the only one with a 16-bit operand */

  /* otherwise all instructions are single-byte, except the branches */
  return (ip->op[0]!=NULL && ip->op[0]->mode==MREL) ? 2 : 1;
}


dblock *eval_instruction(instruction *ip,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  symbol *base = NULL;
  taddr val;

  db->size = instruction_size(ip,sec,pc);
  db->data = mymalloc(db->size);
  db->data[0] = mnemonics[ip->code].ext.opcode;

  if (ip->op[0]) {
    if (!eval_expr(ip->op[0]->val,&val,sec,pc)) {
      if (find_base(ip->op[0]->val,&base,sec,pc) != BASE_OK)
        general_error(38);  /* illegal relocation */
    }
    if (ip->op[0]->mode == MREL) {  /* relative branch */
      if (base) {
        if (!is_pc_reloc(base,sec)) {
          val -= pc + 2;  /* local label from same section: calculate distance */
          base = NULL;
        }
        else  /* external label or different section */
          add_extnreloc(&db->relocs,base,val-1,REL_PC|REL_MOD_S,0,8,1);
      }
      else if (val<-0x80 || val>=0x7f)
        cpu_error(1);  /* branch destination out of range */
      db->data[1] = val;
    }
    else if (ip->op[0]->mode==MREG || ip->op[0]->mode==MRIN) {
      /* register direct or register indirect addressing mode */
      if (base)  /* externally defined register */
        add_extnreloc(&db->relocs,base,val,REL_ABS|REL_MOD_U,0,4,0);
      else if (val<0 || val>15)
        cpu_error(4,(int)val);  /* cannot be a register */
      db->data[0] |= val & 15;
    }
    else
      ierror(0);
  }

  if (ip->op[1]) {
    if (ip->op[1]->mode == MDAT) {  /* 16 bit immediate value (SET) */
      if (!eval_expr(ip->op[1]->val,&val,sec,pc)) {
        int btype = find_base(ip->op[1]->val,&base,sec,pc);

        if (btype==BASE_OK || btype==BASE_PCREL)
          add_extnreloc(&db->relocs,base,btype==BASE_PCREL?val+1:val,
                        btype==BASE_PCREL?REL_PC:REL_ABS,
                        0,16,1);
        else
          general_error(38);  /* illegal relocation */
      }
      setval(0,&db->data[1],2,val);  /* little-endian 16-bit */
    }
    else
      ierror(0);
  }

  return db;
}


dblock *eval_data(operand *op,size_t bitsize,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  taddr val;

  if (bitsize>32 || (bitsize&7))
    cpu_error(2,bitsize);  /* data size not supported */

  db->size = bitsize >> 3;
  db->data = mymalloc(db->size);

  if (!eval_expr(op->val,&val,sec,pc)) {
    symbol *base;
    int btype;
    
    btype = find_base(op->val,&base,sec,pc);
    if (btype==BASE_OK || btype==BASE_PCREL) {
      add_extnreloc(&db->relocs,base,val,
                    btype==BASE_PCREL?REL_PC:REL_ABS,0,bitsize,0);
    }
    else
      general_error(38);  /* illegal relocation */
  }
  else {
    if (bitsize < 16) {
      if (val<-0x80 || val>0xff)
        cpu_error(3,8);   /* data doesn't fit into 8-bits */
    } else if (bitsize < 24) {
      if (val<-0x8000 || val>0xffff)
        cpu_error(3,16);  /* data doesn't fit into 16-bits */
    } else if (bitsize < 32) {
      if (val<-0x800000 || val>0xffffff)
        cpu_error(3,24);  /* data doesn't fit into 24-bits */
    }
  }

  setval(0,db->data,db->size,val);
  return db;
}


operand *new_operand(void)
{
  operand *new = mymalloc(sizeof(*new));
  new->val = NULL;
  return new;
}


int init_cpu(void)
{
  return 1;
}


int cpu_args(char *p)
{
  return 0;
}
