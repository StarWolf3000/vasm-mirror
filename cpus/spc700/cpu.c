/*
** cpu.c spc-700 cpu-description file
** (c) in 2025 by Frank Wille
*/

#include "vasm.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};
const int mnemonic_cnt=sizeof(mnemonics)/sizeof(mnemonics[0]);

const char *cpu_copyright="vasm spc700 cpu backend 0.1a (c) 2025 Frank Wille";
const char *cpuname = "spc700";
int bytespertaddr = 2;

static int auto_mask,branchopt,dp_offset;
static uint16_t dpage;
static int OC_JMPABS,OC_BRA;

/* sizes for all operand types - refer to addressing modes enum in cpu.h */
static const uint8_t opsize[NUM_OPTYPES] = {
  0,0,0,0,0,0,0,0,0,0,0,0,1,1,2,2,2,1,1,1,1,1,2,1,2,2,0,1,4
};

/* table for cpu specific extra directives */
struct ExtraDirectives {
  char *name;
  char *(*func)(char *);
};
static hashtable *cpudirhash;


void cpu_opts(void *opts)
/* set cpu options for following atoms */
{
  dpage = ((cpuopts *)opts)->dpage;
}


void cpu_opts_init(section *s)
/* add a current cpu opts atom */
{
  if (s || current_section) {
    cpuopts *new = mymalloc(sizeof(cpuopts));

    new->dpage = dpage;
    add_atom(s,new_opts_atom(new));
  }
}


void print_cpu_opts(FILE *f,void *opts)
{
  fprintf(f,"opts: P=%d",((cpuopts *)opts)->dpage!=0);
}


static char strip_index(char *p,char **end)
/* strip "+X" or "+Y" index from end of string, terminate it with '\0',
   optionally return a pointer to the new string-end */
{
  char *q = NULL;

  while (*p) {
    if (p[0]=='+' && (p[1]&0xde)=='X')
      q = p++;
    else if (!isspace((unsigned char)*p))
      q = NULL;
    p++;
  }
  if (q) {
    /* remove +X / +Y from end of string and return it */
    if (end)
      *end = q;
    *q++ = '\0';
    return (*q)&0xdf;
  }
  if (end)
    *end = p;
  return 0;
}


static char *strip_trailing_char(char *s,char *e,char c)
/* Delete last c character in preceding string and terminate it there,
   return pointer to that new string end.
   Note: e points to the current string-termination character! */
{
  char *p;

  for (p=e; p>=s; p--) {
    if (*p == c) {
      *p = '\0';
      return p;
    }
  }
  return e;
}


int parse_operand(char *p,int len,operand *op,int required)
{
  static const char *regnames[] = { /* must have same order as enum in cpu.h */
    NULL,"A","X","(X)","(X)+","Y","(Y)","YA","PSW","C","SP"
  };
  static const size_t reglengths[] = {  /* length of regnames above */
    0,1,1,3,4,1,3,2,3,1,2
  };
  static strbuf opbuf;
  int i;

  op->flags = 0;
  p = skip(cutstr(&opbuf,p,len));   /* copy operand to our buffer */

  /* start with the immediate check */
  if (*p == '#') {
    if (required != IMMED)
      return PO_NOMATCH;
    p = skip(++p);
  }
  else if (required == IMMED) {
    return PO_NOMATCH;
  }
  else {
    /* then compare with all register names first */
    for (i=ACCU; i<DATAOP; i++) {
      size_t rlen = reglengths[i];

      if (!strnicmp(p,regnames[i],rlen)) {
        char *q = skip(p+rlen);

        if (*q == '\0') {
          if (required == i) {
            op->type = required;
            op->value = NULL;
            return PO_MATCH;
          }
          return PO_NOMATCH;
        }
      }
    }
    if (required < DATAOP)
      return PO_NOMATCH;
  }

  if (required > IMMED) {
    char *end,idx;

    idx = strip_index(p,&end);  /* find/remove a potential +X or +Y */

    if (IS_INDIR(required)) {
      if (idx=='X' || !check_indir(p,end,'[',']'))
        return PO_NOMATCH;

      /* strip brackets from both ends */
      p = skip(++p);
      end = strip_trailing_char(p,end,']');

      if (*p == '!') {
        op->flags |= OF_WA;
        p = skip(++p);
      }

      if (!idx) {
        idx = strip_index(p,&end);
        if (idx == 'Y')
          return PO_NOMATCH;
      }

      switch (required) {
        case DPINDX:
        case JMPINDX:
          if (idx != 'X')
            return PO_NOMATCH;
          break;
        case DPINDY:
          if (idx != 'Y')
            return PO_NOMATCH;
          break;
        default:
          return PO_NOMATCH;
      }
      op->value = parse_expr(&p);
    }
    else {
      /* absolute or direct-page indexed */
      if (idx=='X' && required!=ABSX && required!=DPAGEX)
        return PO_NOMATCH;
      if (idx=='Y' && required!=ABSY && required!=DPAGEY)
        return PO_NOMATCH;
      if (!idx && (required==ABSX || required==ABSY ||
                   required==DPAGEX || required==DPAGEY))
        return PO_NOMATCH;

      if (check_indir(p,end,'[',']'))
        return PO_NOMATCH;

      if (*p == '!') {
        op->flags |= OF_WA;
        p = skip(++p);
      }

      if (*p == '/') {
        if (required != ABSBITN)
          return PO_NOMATCH;
        p = skip(++p);
      }
      else if (required == ABSBITN)
        return PO_NOMATCH;

      op->value = parse_expr(&p);

      if (required==DPBIT || required==ABSBIT || required==ABSBITN) {
        if (*p++ != '.') {
          free_expr(op->value);
          return PO_NOMATCH;
        }
        op->bitval = parse_expr(&p);
      }
    }
  }
  else {
    /* DATAOP, PCALL, IMMED */
    if (*p == '<') {
      op->flags |= OF_LO;
      p = skip(++p);
    }
    else if (*p == '>') {
      op->flags |= OF_HI;
      p = skip(++p);
    }
    else if (*p == '?') {
      op->flags |= OF_ID;
      p = skip(++p);
    }
    op->value = parse_expr(&p);
  }

  p = skip(p);
  if (*p)
    cpu_error(1);  /* trailing garbage in operand */

  op->type = required;
  return PO_MATCH;
}


static char *handle_setp0(char *s)
{
  dpage = 0;
  cpu_opts_init(NULL);
  return s;
}

static char *handle_setp1(char *s)
{
  dpage = 0x100;
  cpu_opts_init(NULL);
  return s;
}

static char *handle_dpage(char *s)
{
  strbuf *buf;

  for (;;) {
    if (buf = parse_identifier(0,&s)) {
      symbol *sym = new_import(buf->str);
      sym->flags |= DPAGESYM;
    }
    else
      cpu_error(7);  /* identifier expected */
    s = skip(s);
    if (*s == ',')
      s = skip(s+1);
    else
      break;
  }
  return s;
}

static char *handle_direct(char *s)
{
  static const char dpname[] = ".direct";
  section *sec = new_section(dotdirs ? dpname : dpname+1, "aurwz",1);

  sec->flags |= NEAR_ADDRESSING;  /* meaning of direct-page addressing */
  set_section(sec);
  return s;
}

static struct ExtraDirectives cpudirs[] = {
  "p0",handle_setp0,
  "p1",handle_setp1,
  "dpage",handle_dpage,
  "direct",handle_direct,
};


char *parse_cpu_special(char *start)
{
  char *name=start,*s=start;
  hashdata data;

  if (dotdirs && *s=='.') {
    s++;
    name++;
  }
  if (ISIDSTART(*s)) {
    s++;
    while (ISIDCHAR(*s))
      s++;
    if (find_namelen_nc(cpudirhash,name,s-name,&data)) {
      s = cpudirs[data.idx].func(skip(s));
      eol(s);
      return skip_line(s);
    }
  }
  return start;
}


int parse_cpu_label(char *labname,char **start)
/* parse cpu-specific directives following a label field,
   return zero when no valid directive was recognized */
{
  char *dir=*start,*s=*start;

  if (ISIDSTART(*s)) {
    s++;
    while (ISIDCHAR(*s))
      s++;
    if (dotdirs && *dir=='.')
      dir++;

    if (s-dir==4 && !strnicmp(dir,"equd",3)) {
      /* label EQUD <expression> */
      symbol *sym;

      s = skip(s);
      sym = new_equate(labname,parse_expr_tmplab(&s));
      sym->flags |= DPAGESYM;
      eol(s);
      *start = skip_line(s);
      return 1;
    }
  }
  return 0;
}


static void optimize_instruction(instruction *ip,section *sec,taddr pc)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  symbol *base;
  operand *op;
  taddr val;
  int i;

  for (i=0; i<MAX_OPERANDS; i++) {
    if ((op = ip->op[i]) != NULL) {
      if (op->value != NULL) {
        if (eval_expr(op->value,&val,sec,pc))
          base = NULL;  /* val is constant/absolute */
        else
          find_base(op->value,&base,sec,pc);  /* get base-symbol */

        if (IS_ABS(op->type)) {
          /* we have an operand which may be 8-, 16-bit addressing */
          if (mnemo->ext.dp_opcode && !(op->flags & OF_WA) &&
              ((base==NULL && (((utaddr)val>=(utaddr)dpage &&
                                (utaddr)val<=(utaddr)dpage+0xff))) ||
               (base!=NULL && ((base->flags & DPAGESYM) || (LOCREF(base) &&
                               (base->sec->flags & NEAR_ADDRESSING)))))
             ) {
            /* convert abs to a direct page addressing mode */
            op->type += DPAGE - ABS;
          }
        }

        if (branchopt) {
          taddr bd = val - (pc + 2);

          if (op->type==BREL && (base==NULL || !is_pc_reloc(base,sec)) &&
              (bd<-0x80 || bd>0x7f)) {
            if (mnemo->ext.opcode==0x2f) {
              /* translate BRA to JMP */
              ip->code = OC_JMPABS;
              op->type = ABS;
            }
            else  /* branch dest. out of range: use a B!cc/JMP combination */
              op->type = RELJMP;
          }
          else if (ip->code==OC_JMPABS &&
                   (base==NULL || !is_pc_reloc(base,sec)) &&
                   bd>=-0x80 && bd<=0x80) {
            /* JMP may be optimized to a BRA */
            ip->code = OC_BRA;
            op->type = BREL;
          }
        }
      }
    }
    else
      break;
  }
}


static size_t get_inst_size(instruction *ip)
{
  size_t sz = 1;
  int i;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] != NULL)
      sz += opsize[ip->op[i]->type];
    else
      break;
  }
  return sz;
}


size_t instruction_size(instruction *ip,section *sec,taddr pc)
{
  instruction *ipcopy;

  ipcopy = copy_inst(ip);
  optimize_instruction(ipcopy,sec,pc);
  return get_inst_size(ipcopy);
}


static void check_value(int optype,symbol *base,taddr val)
{
  switch (optype) {
    case ABS:
    case ABSX:
    case ABSY:
    case JMPINDX:
    case RELJMP:
      if (base==NULL && (val<0 || val>0xffff))
        cpu_error(4,16); /* operand doesn't fit into 16 bits */
      break;
    case ABSBIT:
    case ABSBITN:
      if (base==NULL && (val<0 || val>0x1fff))
        cpu_error(4,13); /* operand doesn't fit into 13 bits */
      break;
    case DPAGE:
    case DPAGEX:
    case DPAGEY:
    case DPINDX:
    case DPINDY:
    case DPBIT:
    case PCALL:
      if (base==NULL && (val<0 || val>0xff))
        cpu_error(8);   /* operand not in direct page */
      break;
    case IMMED:
      if (val<-0x80 || val>0xff)
        cpu_error(4,8); /* operand doesn't fit into 8 bits */
      break;
    case BREL:
      if (val<-0x80 || val>0x7f)
        cpu_error(5);   /* branch destination out of range */
      break;
    case TCALL:
      if (base==NULL && (val<0 || val>0xf))
        cpu_error(4,4); /* operand doesn't fit into 4 bits */
      break;
  }
}


static void check_bitval(symbol *base,taddr val)
{
  if (base==NULL && (val<0 || val>7))
    cpu_error(4,3); /* operand doesn't fit into 3 bits */
}


dblock *eval_instruction(instruction *ip,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  uint8_t *d,oc;
  int i;

  optimize_instruction(ip,sec,pc);  /* really execute optimizations now */
  db->size = get_inst_size(ip);
  d = db->data = mymalloc(db->size);

  /* write opcode */
  oc = mnemonics[ip->code].ext.opcode;
  for (i=0; i<MAX_OPERANDS; i++) {
    switch (ip->op[i]!=NULL ? ip->op[i]->type : IMPLIED) {
      case DPAGE:
      case DPAGEX:
      case DPAGEY:
        oc = mnemonics[ip->code].ext.dp_opcode;
        break;
      case RELJMP:
        oc ^= 0x20;  /* B!cc branch */
        break;
    }
  }
  *d++ = oc;

  if (ip->op[1]!=NULL && ip->op[1]->type==BREL && ip->op[0]!=NULL) {
    /* swap operands when second operand is a branch destination */
    operand *swop = ip->op[0];
    ip->op[0] = ip->op[1];
    ip->op[1] = swop;
  }

  for (i=MAX_OPERANDS-1; i>=0; i--) {
    if (ip->op[i] != NULL) {
      operand *op = ip->op[i];
      int optype = op->type;
      int offs = d - db->data;
      symbol *base,*bbase;
      taddr val,bval;

      if (optype==DPBIT || optype==ABSBIT || optype==ABSBITN) {
        /* evaluate bit expression in ADDR.BIT addressing modes */
        if (!eval_expr(op->bitval,&bval,sec,pc)) {
          if (find_base(op->bitval,&bbase,sec,pc) != BASE_OK) {
            general_error(38);  /* illegal relocation */
            bbase = NULL;
          }
        }
        else
          bbase = NULL;
      }

      if (op->value != NULL) {
        if (!eval_expr(op->value,&val,sec,pc)) {
          int btype = find_base(op->value,&base,sec,pc);

          if (btype==BASE_PCREL && optype==IMMED)
            op->flags |= OF_PC;  /* immediate value with pc-rel. relocation */

          if (btype==BASE_ILLEGAL ||
              (btype==BASE_PCREL && !(op->flags & OF_PC))) {
            general_error(38);  /* illegal relocation */
          }
          else {
            if (optype==BREL && !is_pc_reloc(base,sec)) {
              /* relative branch requires no relocation */
              val -= pc + offs + 1;
            }
            else {
              taddr mask = -1; /* default mask */
              int bo = 0;      /* default bit-offset into reloc-field */
              int size = 8;    /* default reloc-field size */
              taddr add;       /* reloc addend */
              int type;        /* reloc type */

              if (op->flags & OF_ID) {
                type = REL_MEMID;
                if (LOCREF(base))
                  val -= base->pc;  /* take symbol's offset out of the addend */
              }
              else
                type = REL_ABS;

              add = val;

              switch (optype) {
                case RELJMP:
                  offs += 2;  /* skip branch offset and JMP-opcode */
                case ABS:
                case ABSX:
                case ABSY:
                case JMPINDX:
                  size = 16;
                  break;
                case DPAGE:
                case DPAGEX:
                case DPAGEY:
                case DPINDX:
                case DPINDY:
                  if (dp_offset)
                    type = REL_SECOFF;  /* 8-bit offset to DP-section */
                  else
                    mask = 0xff;
                  if (op->flags & OF_WA)
                    cpu_error(2);  /* selector prefix ignored */
                  break;
                case IMMED:
                case PCALL:
                  if (op->flags & OF_PC) {
                    type = REL_PC;
                    add += offs;
                    val += offs;
                    if (op->flags & (OF_LO|OF_HI|OF_ID))
                      cpu_error(2);  /* selector prefix ignored */
                  }
                  else if (op->flags & OF_HI) {
                    mask = ~0xff;
                    val = (val >> 8) & 0xff;
                  }
                  else if ((op->flags & OF_LO) || auto_mask) {
                    mask = 0xff;
                    val &= 0xff;
                  }
                  break;
                case BREL:
                  type = REL_PC;
                  add -= 1;  /* branch addend correction */
                  break;
                case DPBIT:
                  if (bbase)
                    add_extnreloc(&db->relocs,bbase,bval,REL_ABS|REL_MOD_U,
                                  5,3,0);
                  break;
                case ABSBIT:
                case ABSBITN:
                  size = 13;
                  if (bbase)
                    add_extnreloc(&db->relocs,bbase,bval,REL_ABS|REL_MOD_U,
                                  13,3,offs);
                  break;
                case TCALL:  /* call vector 0-15 */
                  size = 4;
                  offs = 0;
                  bo = 4;
                  if (type == REL_ABS)
                    type |= REL_MOD_U;
                  break;
                default:
                  ierror(0);
                  break;
              }
              add_extnreloc_masked(&db->relocs,base,add,type,bo,size,offs,mask);
            }
          }
        }
        else {
          /* constant/absolute value */
          base = NULL;

          if (op->flags & OF_ID) {
            if (optype==IMMED || optype==PCALL) {
              cpu_error(10);  /* no memory-id for absolute symbols */
              val = 0;  /* @@@ we should only subtract the symbol's value here */
            }
            else
              cpu_error(9);  /* unsuitable addressing mode for memory id */
            val = 0;
          }
          else {
            switch (optype) {
             case DPBIT:
               if (bbase)
                 add_extnreloc(&db->relocs,bbase,bval,REL_ABS|REL_MOD_U,5,3,0);
              case DPAGE:
              case DPAGEX:
              case DPAGEY:
              case DPINDX:
              case DPINDY:
                val -= dpage;
                if (op->flags & OF_WA)
                  cpu_error(2);  /* selector prefix ignored */
                break;
              case ABSBIT:
              case ABSBITN:
                if (bbase)
                  add_extnreloc(&db->relocs,bbase,bval,REL_ABS|REL_MOD_U,
                                13,3,offs);
                break;
              case BREL:
                val -= pc + offs + 1;
                break;
              case IMMED:
              case PCALL:
                if (op->flags & OF_HI)
                  val = (val >> 8) & 0xff;
                else if ((op->flags & OF_LO) || auto_mask)
                  val &= 0xff;
                break;
            }
          }
        }

        check_value(op->type,base,val);

        /* write operand data */
        switch (optype) {
          case ABSX:
          case ABSY:
            if (!*(db->data)) /* only Direct Page addressing mode allowed */
              cpu_error(0);
          case ABS:
          case JMPINDX:
            d = setval(0,d,2,val);
            break;
          case DPBIT:
            check_bitval(bbase,bval);
            *(db->data) |= (bval&7) << 5;  /* set bit number in opcode */
          case DPAGE:
          case DPAGEX:
          case DPAGEY:
          case DPINDX:
          case DPINDY:
          case IMMED:
          case BREL:
          case PCALL:
            *d++ = val & 0xff;
            break;
          case RELJMP:
            if (d - db->data > 1)
              ierror(0);
            *d++ = 3;     /* B!cc *+3 */
            *d++ = 0x5f;  /* JMP */
            d = setval(0,d,2,val);
            break;
          case ABSBIT:
          case ABSBITN:
            check_bitval(bbase,bval);
            d = setval(0,d,2,((bval&7)<<13)|(val&0x1fff));
            break;
          case TCALL:
            *(db->data) |= (val&15) << 4;  /* set vector number in opcode */
            break;
          default:
            ierror(0);
            break;
        }
      }
    }
  }

  return db;
}


dblock *eval_data(operand *op,size_t bitsize,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  rlist *rl = NULL;
  taddr val;

  if (bitsize>16 || (bitsize&7))
    cpu_error(3,bitsize);  /* data size not supported */

  db->size = bitsize >> 3;
  db->data = mymalloc(db->size);

  if (!eval_expr(op->value,&val,sec,pc)) {
    symbol *base;
    int btype = find_base(op->value,&base,sec,pc);

    if (btype==BASE_OK ||
        (btype==BASE_PCREL && !(op->flags&(OF_LO|OF_HI|OF_WA|OF_ID)))) {
      int rtype;

      if (op->flags & OF_ID) {
        rtype = REL_MEMID;
        if (LOCREF(base))
          val -= base->pc;  /* take symbol's offset out of the addend */
      }
      else
        rtype = btype==BASE_PCREL ? REL_PC : REL_ABS;

      rl = add_extnreloc(&db->relocs,base,val,rtype,0,bitsize,0);
    }
    else
      general_error(38);  /* illegal relocation */
  }
  else {
    if (op->flags & OF_ID) {
      cpu_error(10);  /* no memory-id for absolute symbols */
      val = 0;  /* @@@ we should only subtract the symbol's value here */
    }
  }

  switch (bitsize) {
    case 8:
      if ((op->flags & OF_LO) || auto_mask) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = 0xff;
        val &= 0xff;
      }
      else if (op->flags & OF_HI) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = 0xff00;
        val = (val >> 8) & 0xff;
      }
      if (val<-0x80 || val>0xff)
        cpu_error(4,8);  /* operand doesn't fit into 8 bits */
      break;

    case 16:
      if ((op->flags & OF_LO) || auto_mask) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = 0xffff;
        val &= 0xffff;
      }
      else if (op->flags & OF_HI) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = 0xff00;
        val = (val >> 8) & 0xffff;
      }
      if (val<-0x8000 || val>0xffff)
        cpu_error(4,16);  /* operand doesn't fit into 16 bits */
      break;
  }

  setval(0,db->data,db->size,val);
  return db;
}


operand *new_operand(void)
{
  operand *new = mymalloc(sizeof(*new));
  return new;
}


int init_cpu(void)
{
  hashdata data;
  int i;

  for (i=0; i<mnemonic_cnt; i++) {
    if (!strcmp(mnemonics[i].name,"jmp") && mnemonics[i].operand_type[0]==ABS)
      OC_JMPABS = i;
    else if (!strcmp(mnemonics[i].name,"bra"))
      OC_BRA = i;
  }

  cpudirhash = new_hashtable(0x100);
  for (i=0; i<sizeof(cpudirs)/sizeof(cpudirs[0]); i++) {
    data.idx = i;
    add_hashentry(cpudirhash,cpudirs[i].name,data);
  }
  if (debug && cpudirhash->collisions)
    fprintf(stderr,"*** %d cpu directive collisions!!\n",cpudirhash->collisions);

  return 1;
}


int cpu_args(char *p)
{
  if (!strcmp(p,"-am"))
    auto_mask = 1;
  else if (!strcmp(p,"-dpo"))
    dp_offset = 1;
  else if (!strcmp(p,"-opt-branch"))
    branchopt = 1;
  else
    return 0;

  return 1;
}
