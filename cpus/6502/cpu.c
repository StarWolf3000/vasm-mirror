/*
** cpu.c 650x/65C02/6280/45gs02/65816 cpu-description file
** (c) in 2002,2006,2008-2012,2014-2023 by Frank Wille
*/

#include "vasm.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};
const int mnemonic_cnt=sizeof(mnemonics)/sizeof(mnemonics[0]);

const char *cpu_copyright="vasm 6502 cpu backend 0.12 (c) 2002,2006,2008-2012,2014-2023 Frank Wille";
const char *cpuname = "6502";
int bitsperbyte = 8;
int bytespertaddr = 2;

uint16_t cpu_type = M6502;
static int branchopt;
static int modifier;      /* set by find_base() */
static uint16_t dpage;    /* zero/direct page (default 0) - set with SETDP */
static uint8_t asize = 8; /* Accumulator is 8 bits by default */
static uint8_t xsize = 8; /* Index registers are 8 bits by default */

static char lo_c = '<';   /* select low-byte or zero/direct page */
static char hi_c = '>';   /* select high-byte or full absolute */
static char bk_c = '^';   /* select bank-byte (bits 16..23) from address */

static int OC_JMPABS,OC_BRA,OC_MVN,OC_MVP;

/* table for cpu specific extra directives */
struct ExtraDirectives {
  char *name;
  unsigned avail;
  char *(*func)(char *);
};
static hashtable *cpudirhash;


int ext_unary_name(char *s)
{
  return *s==bk_c || *s==lo_c || *s==hi_c;
}


int ext_unary_type(char *s)
{
  if (*s == bk_c)
    return BANKBYTE;
  else if (*s == hi_c)
    return HIBYTE;
  else if (*s != lo_c)
    ierror(0);
  return LOBYTE;
}


int ext_unary_eval(int type,taddr val,taddr *result,int cnst)
{
  switch (type) {
    case LOBYTE:
      *result = cnst ? (val & 0xff) : val;
      return 1;
    case HIBYTE:
      *result = cnst ? ((val >> 8) & 0xff) : val;
      return 1;
    case BANKBYTE:
      *result = cnst ? ((val >> 16) & 0xff) : val;
      return 1;
    default:
      break;
  }
  return 0;  /* unknown type */
}


int ext_find_base(symbol **base,expr *p,section *sec,taddr pc)
{
  if (p->right!=NULL && p->right->type==NUM) {
    if (cpu_type & WDC65816) {
      /* only bank-byte and low-byte at the moment */
      switch (p->type) {
        case DIV:
          if (p->right->c.val == 65536)
            p->type = BANKBYTE;
          break;
        case RSH:
        case RSHU:
          if (p->right->c.val == 16)
            p->type = BANKBYTE;
          break;
        case MOD:
          if (p->right->c.val == 256)
            p->type = LOBYTE;
          break;
        case BAND:
          if (p->right->c.val == 255)
            p->type = LOBYTE;
          break;
      }
    }
    else {
      /* addr/256 equals >addr, addr%256 and addr&255 equal <addr */
      switch (p->type) {
        case DIV:
        case MOD:
          if (p->right->c.val == 256)
            p->type = p->type == DIV ? HIBYTE : LOBYTE;
          break;
        case RSH:
        case RSHU:
          if (p->right->c.val == 8)
            p->type = HIBYTE;
          break;
        case BAND:
          if (p->right->c.val == 255)
            p->type = LOBYTE;
          break;
      }
    }
  }

  if (p->type==LOBYTE || p->type==HIBYTE || p->type==BANKBYTE) {
    modifier = p->type;
    return find_base(p->left,base,sec,pc);
  }
  return BASE_ILLEGAL;
}


void cpu_opts(void *opts)
/* set cpu options for following atoms */
{
  dpage = ((cpuopts *)opts)->dpage;
  asize = ((cpuopts *)opts)->asize;
  xsize = ((cpuopts *)opts)->xsize;
}


void cpu_opts_init(section *s)
/* add a current cpu opts atom */
{
  cpuopts *new = mymalloc(sizeof(cpuopts));

  new->dpage = dpage;
  new->asize = asize;
  new->xsize = xsize;
  add_atom(s,new_opts_atom(new));
}


void print_cpu_opts(FILE *f,void *opts)
{
  fprintf(f,"opts: dp=0x%04x a=%d x=%d",(unsigned)((cpuopts *)opts)->dpage,
          (int)((cpuopts *)opts)->asize,(int)((cpuopts *)opts)->xsize);
}


int parse_operand(char *p,int len,operand *op,int required)
{
  char *start = p;
  int indir = 0;
  int ret = PO_MATCH;

  p = skip(p);

  if (!op->type) {
    if (len>0 && required!=DATAOP && (check_indir(p,start+len) || *p=='[')) {
      indir = *p=='[' ? 2 : 1;
      p = skip(++p);
    }

    switch (required) {
      case IMMED:
      case IMMEDX:
      case IMMED8:
      case IMMED16:
        if (*p!='#' || indir)
          return PO_NOMATCH;
        p = skip(++p);
        break;
      case INDIR:
      case INDIRX:
      case DPINDX:
      case DPINDY:
      case DPINDZ:
      case DPIND:
      case SRINDY:
        if (indir != 1)
          return PO_NOMATCH;
        break;
      case LINDIR:
      case LDPINDY:
      case QDPINDZ:
      case LDPIND:
        if (indir != 2)
          return PO_NOMATCH;
        break;
      case WBIT:
        if (*p == '#')  /* # is optional */
          p = skip(++p);
        if (indir)
          return PO_NOMATCH;
        break;
      case ABS:
      case ABSX:
      case ABSY:
      case LABS:
        if (*p == hi_c) {
          p = skip(++p);
          if (cpu_type & WDC65816)
            op->flags |= OF_BK;  /* force full 24-bit addressing on 65816 */
          else
            op->flags |= OF_HI;  /* force absolute addressing mode */
        }
        else if (*p=='!' || *p=='|') {
          p = skip(++p);
          op->flags |= OF_HI;  /* force 16-bit absolute addressing mode */
        }
        else if (*p == lo_c) {
          p = skip(++p);
          op->flags |= OF_LO;  /* force zero/direct page addressing mode */
        }
      default:
        if (indir)
          return PO_NOMATCH;
        break;
    }

    if (required < ACCU)
      op->value = parse_expr(&p);
    else
      op->value = NULL;

    switch (required) {
      case DPINDX:
      case INDIRX:
        if (*p++ == ',') {
          p = skip(p);
          if (toupper((unsigned char)*p++) == 'X')
            break;
        }
        return PO_NOMATCH;
      case SRINDY:
        if (*p++ == ',') {
          p = skip(p);
          if (toupper((unsigned char)*p++) == 'S')
            break;
        }
        return PO_NOMATCH;
      case ACCU:
        if (len != 0) {
          if (len!=1 || toupper((unsigned char)*p++) != 'A')
            return PO_NOMATCH;
        }
        break;
    }

    if (required>=FIRST_INDIR && required<=LAST_INDIR) {
      p = skip(p);
      if (*p++ != ')')
        return PO_NOMATCH;
    }

    if (required>=FIRST_SQIND && required<=LAST_SQIND) {
      p = skip(p);
      if (*p++ != ']')
        return PO_NOMATCH;
    }

    switch (required) {
      case IMMED:
        required = asize==16 ? IMMED16 : IMMED8;
        break;
      case IMMEDX:
        required = xsize==16 ? IMMED16 : IMMED8;
        break;
      case ABSX:
      case ABSY:
      case ABSZ:
      case DPINDY:
      case DPINDZ:
      case LDPINDY:
      case QDPINDZ:
      case SRINDY:
      case SR:
        ret = PO_COMB_REQ;  /* 2nd pass with same op to parse ",X/Y/Z" */
        break;
    }
    op->type = required;
  }
  else {
    /* with the same operand, to parse everything behind a comma */
    switch (op->type) {
      case ABSX:
        if (toupper((unsigned char)*p++) != 'X')
          return PO_NOMATCH;
        break;
      case ABSY:
      case DPINDY:
      case LDPINDY:
      case SRINDY:
        if (toupper((unsigned char)*p++) != 'Y')
          return PO_NOMATCH;
        break;
      case ABSZ:
      case DPINDZ:
      case QDPINDZ:
        if (toupper((unsigned char)*p++) != 'Z')
          return PO_NOMATCH;
        break;
      case SR:
        if (toupper((unsigned char)*p++) != 'S')
          return PO_NOMATCH;
        break;
      default:
        return PO_NOMATCH;
    }
  }

  p = skip(p);
  if (*p && p-start<len)
    cpu_error(1);  /* trailing garbage in operand */
  return ret;
}


static char *handle_setdp(char *s)
{
  dpage = (utaddr)parse_constexpr(&s);
  cpu_opts_init(NULL);
  return s;
}

static char *handle_zpage(char *s)
{
  strbuf *buf;

  for (;;) {
    if (buf = parse_identifier(0,&s)) {
      symbol *sym = new_import(buf->str);
      sym->flags |= ZPAGESYM;
    }
    else
      cpu_error(8);  /* identifier expected */
    s = skip(s);
    if (*s == ',')
      s = skip(s+1);
    else
      break;
  }
  return s;
}

static char *handle_zero(char *s)
{
  static const char zeroname[] = ".zero";
  section *sec = new_section(dotdirs ?
                             (char *)zeroname : (char *)zeroname+1,
                             "aurw",1);

  sec->flags |= NEAR_ADDRESSING;  /* meaning of zero-page addressing */
  set_section(sec);
  return s;
}

static char *handle_asize8(char *s)
{
  asize = 8;
  cpu_opts_init(NULL);
  return s;
}

static char *handle_asize16(char *s)
{
  asize = 16;
  cpu_opts_init(NULL);
  return s;
}

static char *handle_xsize8(char *s)
{
  xsize = 8;
  cpu_opts_init(NULL);
  return s;
}

static char *handle_xsize16(char *s)
{
  xsize = 16;
  cpu_opts_init(NULL);
  return s;
}

static struct ExtraDirectives cpudirs[] = {
  "setdp",~0,handle_setdp,
  "zpage",~0,handle_zpage,
  "zero",~0,handle_zero,
  "a8",WDC65816,handle_asize8,
  "a16",WDC65816,handle_asize16,
  "x8",WDC65816,handle_xsize8,
  "x16",WDC65816,handle_xsize16,
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
      if (cpu_type & cpudirs[data.idx].avail) {
        s = cpudirs[data.idx].func(skip(s));
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
  char *dir=*start,*s=*start;

  if (ISIDSTART(*s)) {
    s++;
    while (ISIDCHAR(*s))
      s++;
    if (dotdirs && *dir=='.')
      dir++;

    if (s-dir==3 && !strnicmp(dir,"ezp",3)) {
      /* label EZP <expression> */
      symbol *sym;

      s = skip(s);
      sym = new_equate(labname,parse_expr_tmplab(&s));
      sym->flags |= ZPAGESYM;
      eol(s);
      *start = skip_line(s);
      return 1;
    }
  }
  return 0;
}


static void optimize_instruction(instruction *ip,section *sec,
                                 taddr pc,int final)
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
          if (final) {
            if (!mnemo->ext.zp_opcode && (op->flags & OF_LO))
              cpu_error(10);  /* zp/dp not available */
            if (!mnemo->ext.al_opcode && (op->flags & OF_BK))
              cpu_error(12);  /* abslong not available */
          }
          if (mnemo->ext.zp_opcode && ((op->flags & OF_LO) ||
                                       (!(op->flags & (OF_HI|OF_BK)) &&
              ((base==NULL && ((val>=0 && val<=0xff) ||
                               ((utaddr)val>=(utaddr)dpage &&
                                (utaddr)val<=(utaddr)dpage+0xff))) ||
               (base!=NULL && ((base->flags & ZPAGESYM) || (LOCREF(base) &&
                               (base->sec->flags & NEAR_ADDRESSING)))))
             ))) {
            /* convert to a zero page addressing mode */
            op->type += DPAGE - ABS;
          }
          else if ((cpu_type & WDC65816) && mnemo->ext.al_opcode &&
                      ((op->flags & OF_BK) ||
                       (!(op->flags & (OF_HI|OF_LO)) &&
                          ((base==NULL && val>0xffff) ||
                           (base!=NULL && LOCREF(base) &&
                               (base->sec->flags & FAR_ADDRESSING)))))) {
            /* convert to an absolute long addressing mode */
            op->type += LABS - ABS;
          }
        }
        else if (op->type==REL8 && (base==NULL || !is_pc_reloc(base,sec))) {
          taddr bd = val - (pc + 2);

          if ((bd<-0x80 || bd>0x7f) && branchopt) {
            if (mnemo->ext.opcode==0x80 || mnemo->ext.opcode==0x12) {
              /* translate out of range 65C02/DTV BRA to JMP */
              ip->code = OC_JMPABS;
              op->type = ABS;
            }
            else  /* branch dest. out of range: use a B!cc/JMP combination */
              op->type = RELJMP;
          }
        }
        else if (ip->code==OC_JMPABS && (cpu_type&(DTV|M65C02))!=0 &&
                 branchopt && !(base!=NULL && is_pc_reloc(base,sec))) {
          taddr bd = val - (pc + 2);

          if (bd>=-128 && bd<=128) {
            /* JMP may be optimized to a BRA */
            ip->code = OC_BRA;
            op->type = REL8;
          }
        }
      }
    }
  }
}


static size_t get_inst_size(instruction *ip)
{
  size_t sz = 1;
  int i;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] != NULL) {
      switch (ip->op[i]->type) {
        case IMMED8:
        case REL8:
        case DPINDX:
        case DPINDY:
        case DPINDZ:
        case DPIND:
        case DPAGE:
        case DPAGEX:
        case DPAGEY:
        case DPAGEZ:
        case LDPIND:
        case LDPINDY:
        case SR:
        case SRINDY:
        case MVBANK:
          sz += 1;
          break;
        case QDPINDZ:
        case IMMED16:
        case REL16:
        case ABS:
        case ABSX:
        case ABSY:
        case ABSZ:
        case INDIR:
        case INDIRX:
        case LINDIR:
          sz += 2;
          break;
        case LABS:
        case LABSX:
          sz += 3;
          break;
        case RELJMP:
          sz += 4;
          break;
      }
    }
  }
  if(ISQINST(ip->code))
    sz += 2;
  return sz;
}


size_t instruction_size(instruction *ip,section *sec,taddr pc)
{
  instruction *ipcopy;

  ipcopy = copy_inst(ip);
  optimize_instruction(ipcopy,sec,pc,0);
  return get_inst_size(ipcopy);
}


static void rangecheck(instruction *ip,symbol *base,taddr val,operand *op)
{
  switch (op->type) {
    case ABS:
    case ABSX:
    case ABSY:
    case ABSZ:
    case LINDIR:
      if (base==NULL && (val<0 || val>0xffff))
        cpu_error(5,16); /* operand doesn't fit into 16 bits */
      break;
    case DPAGE:
    case DPAGEX:
    case DPAGEY:
    case DPAGEZ:
    case DPINDX:
    case DPINDY:
    case DPINDZ:
    case LDPIND:
    case LDPINDY:
    case QDPINDZ:
    case DPIND:
    case SR:
    case SRINDY:
      if (base==NULL && (val<0 || val>0xff) &&
          ((utaddr)val<(utaddr)dpage || (utaddr)val>(utaddr)dpage+0xff))
        cpu_error(11);   /* operand not in zero/direct page */
      break;
    case IMMED8:
      if (val<-0x80 || val>0xff)
        cpu_error(5,8); /* operand doesn't fit into 8 bits */
      break;
    case IMMED16:
      if (val<-0x8000 || val>0xffff)
        cpu_error(5,16); /* operand doesn't fit into 16 bits */
      break;
    case REL8:
      if (val<-0x80 || val>0x7f)
        cpu_error(6);   /* branch destination out of range */
      break;
    case REL16:
      if (val<-0x8000 || val>0x7fff)
        cpu_error(6);   /* branch destination out of range */
      break;
    case WBIT:
      if (val<0 || val>7)
        cpu_error(7);   /* illegal bit number */
      break;
  }
}


dblock *eval_instruction(instruction *ip,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  unsigned char *d,oc;
  int optype,i;
  taddr val;

  optimize_instruction(ip,sec,pc,1);  /* really execute optimizations now */

  db->size = get_inst_size(ip);
  d = db->data = mymalloc(db->size);

  /* write opcode */
  oc = mnemonics[ip->code].ext.opcode;
  for (i=0; i<MAX_OPERANDS; i++) {
    optype = ip->op[i]!=NULL ? ip->op[i]->type : IMPLIED;
    switch (optype) {
      case LABS:
      case LABSX:
        oc = mnemonics[ip->code].ext.al_opcode;
        break;
      case DPAGE:
      case DPAGEX:
      case DPAGEY:
      case DPAGEZ:
        oc = mnemonics[ip->code].ext.zp_opcode;
        break;
      case RELJMP:
        oc ^= 0x20;  /* B!cc branch */
        break;
    }
  }

  if (ISQINST(ip->code)) {
    *d++ = 0x42;
    *d++ = 0x42;
  }
  else if (ip->code==OC_MVN || ip->code==OC_MVP) {
    /* swap source and destination operand */
    if (ip->op[0] && ip->op[1]) {
      operand *op = ip->op[0];
      ip->op[0] = ip->op[1];
      ip->op[1] = op;
    }
  }
  *d++ = oc;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] != NULL) {
      operand *op = ip->op[i];
      int offs = d - db->data;
      symbol *base;

      optype = (int)op->type;
      if (op->value != NULL) {
        if (!eval_expr(op->value,&val,sec,pc)) {
          taddr add = 0;
          int btype;

          modifier = 0;
          btype = find_base(op->value,&base,sec,pc);
          if (btype==BASE_PCREL && (optype==IMMED8 || optype==IMMED16))
            op->flags |= OF_PC;  /* immediate value with pc-rel. relocation */

          if (optype==WBIT || btype==BASE_ILLEGAL ||
              (btype==BASE_PCREL && !(op->flags & OF_PC))) {
            general_error(38);  /* illegal relocation */
          }
          else {
            if (modifier) {
              if (op->flags & (OF_LO||OF_HI|OF_BK))
                cpu_error(9);  /* multiple hi/lo modifiers */
              switch (modifier) {
                case LOBYTE: op->flags |= OF_LO; break;
                case HIBYTE: op->flags |= OF_HI; break;
                case BANKBYTE: op->flags |= OF_BK; break;
              }
            }

            if ((optype==REL8 || optype==REL16) && !is_pc_reloc(base,sec)) {
              /* relative branch requires no relocation */
              val = val - (pc + offs + (optype==REL8 ? 1 : 2));
            }
            else {
              int type = REL_ABS;
              int size;
              rlist *rl;

              switch (optype) {
                case ABS:
                case ABSX:
                case ABSY:
                case ABSZ:
                  op->flags &= ~(OF_LO|OF_HI|OF_BK);
                case INDIR:
                case INDIRX:
                case LINDIR:
                  size = 16;
                  break;
                case LABS:
                case LABSX:
                  if (op->flags & (OF_LO|OF_HI))
                    cpu_error(2);  /* addressing mode selector ignored */
                  op->flags &= ~(OF_LO|OF_HI|OF_BK);
                  size = 24;
                  break;
                case QDPINDZ:
		  offs++;
		  size = 8;
		  break;
                case DPAGE:
                case DPAGEX:
                case DPAGEY:
                case DPAGEZ:
                  op->flags &= ~(OF_LO||OF_HI|OF_BK);
                case DPINDX:
                case DPINDY:
                case DPINDZ:
                case DPIND:
                case LDPIND:
                case LDPINDY:
                case SR:
                case SRINDY:
                  size = 8;
                  break;
                case MVBANK:
                  op->flags = OF_BK;
                  size = 8;
                  break;
                case IMMED8:
                  if (op->flags & OF_PC) {
                    type = REL_PC;
                    val += offs;
                  }
                  size = 8;
                  break;
                case IMMED16:
                  if (op->flags & OF_PC) {
                    type = REL_PC;
                    val += offs;
                  }
                  size = 16;
                  break;
                case RELJMP:
                  size = 16;
                  offs = 3;
                  break;
                case REL8:
                  type = REL_PC;
                  size = 8;
                  add = -1;  /* 6502 addend correction */
                  break;
                case REL16:
                  type = REL_PC;
                  size = 16;
                  add = -2;  /* 6502 addend correction */
                  break;
                default:
                  ierror(0);
                  break;
              }

              rl = add_extnreloc(&db->relocs,base,val+add,type,0,size,offs);
              if (op->flags & OF_LO) {
                if (rl)
                  ((nreloc *)rl->reloc)->mask = 0xff;
                val = val & 0xff;
              }
              else if (op->flags & OF_HI) {
                if (rl)
                  ((nreloc *)rl->reloc)->mask = 0xff00;
                val = (val >> 8) & 0xff;
              }
              else if (op->flags & OF_BK) {
                if (rl)
                  ((nreloc *)rl->reloc)->mask = 0xff0000;
                val = (val >> 16) & 0xff;
              }
            }
          }
        }
        else {
          /* constant/absolute value */
          base = NULL;
          if (optype == REL8)
            val -= pc + offs + 1;
          else if (optype == REL16)
            val -= pc + offs + 2;
          else if (optype==MVBANK && val>0xff)
            val = (val >> 16) & 0xff;
        }

        rangecheck(ip,base,val,op);

        /* write operand data */
        switch (optype) {
          case ABSX:
          case ABSY:
          case ABSZ:
            if (!*(db->data)) /* STX/STY allow only ZeroPage addressing mode */
              cpu_error(5,8); /* operand doesn't fit into 8 bits */
          case ABS:
          case INDIR:
          case INDIRX:
          case LINDIR:
          case REL16:
	  case IMMED16:
            d = setval(0,d,2,val);
            break;
          case LABS:
          case LABSX:
            d = setval(0,d,3,val);
            break;
          case QDPINDZ:
	    *d = d[-1];
	    d[-1] = 0xea;
	    d++;
          case LDPIND:
          case LDPINDY:
          case DPIND:
          case DPINDX:
          case DPINDY:
          case DPINDZ:
          case DPAGE:
          case DPAGEX:
          case DPAGEY:
          case DPAGEZ:
            if ((utaddr)val>=(utaddr)dpage && (utaddr)val<=(utaddr)dpage+0xff)
              val -= dpage;
          case IMMED8:
          case REL8:
          case SR:
          case SRINDY:
          case MVBANK:
            *d++ = val & 0xff;
            break;
          case RELJMP:
            if (d - db->data > 1)
              ierror(0);
            *d++ = 3;     /* B!cc *+3 */
            *d++ = 0x4c;  /* JMP */
            d = setval(0,d,2,val);
            break;
          case WBIT:
            *(db->data) |= (val&7) << 4;  /* set bit number in opcode */
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
  taddr val;

  if (bitsize>32 || (bitsize&7))
    cpu_error(3,bitsize);  /* data size not supported */

  db->size = bitsize >> 3;
  db->data = mymalloc(db->size);
  if (!eval_expr(op->value,&val,sec,pc)) {
    symbol *base;
    int btype;
    rlist *rl;
    
    modifier = 0;
    btype = find_base(op->value,&base,sec,pc);
    if (btype==BASE_OK || (btype==BASE_PCREL && modifier==0)) {
      rl = add_extnreloc(&db->relocs,base,val,
                         btype==BASE_PCREL?REL_PC:REL_ABS,0,bitsize,0);
      switch (modifier) {
        case LOBYTE:
          if (rl)
            ((nreloc *)rl->reloc)->mask = 0xff;
          val = val & 0xff;
          break;
        case HIBYTE:
          if (rl)
            ((nreloc *)rl->reloc)->mask = 0xff00;
          val = (val >> 8) & 0xff;
          break;
        case BANKBYTE:
          if (rl)
            ((nreloc *)rl->reloc)->mask = 0xff0000;
          val = (val >> 16) & 0xff;
          break;
      }
    }
    else if (btype != BASE_NONE)
      general_error(38);  /* illegal relocation */
  }
  if (bitsize < 16) {
    if (val<-0x80 || val>0xff)
      cpu_error(5,8);  /* operand doesn't fit into 8 bits */
  } else if (bitsize < 24) {
    if (val<-0x8000 || val>0xffff)
      cpu_error(5,16);  /* operand doesn't fit into 16 bits */
  } else if (bitsize < 32) {
    if (val<-0x800000 || val>0xffffff)
      cpu_error(5,24);  /* operand doesn't fit into 24 bits */
  }

  setval(0,db->data,db->size,val);
  return db;
}


operand *new_operand()
{
  operand *new = mymalloc(sizeof(*new));
  new->type = 0;
  new->flags = 0;
  return new;
}


int cpu_available(int idx)
{
  return (mnemonics[idx].ext.available & cpu_type) != 0;
}


int init_cpu()
{
  hashdata data;
  int i;

  for (i=0; i<mnemonic_cnt; i++) {
    if (mnemonics[i].ext.available & cpu_type) {
      if (!strcmp(mnemonics[i].name,"jmp") && mnemonics[i].operand_type[0]==ABS)
        OC_JMPABS = i;
      else if (!strcmp(mnemonics[i].name,"bra"))
        OC_BRA = i;
      else if (!strcmp(mnemonics[i].name,"mvn"))
        OC_MVN = i;
      else if (!strcmp(mnemonics[i].name,"mvp"))
        OC_MVP = i;
    }
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
  if (!strcmp(p,"-bbcade")) {
    /* GMGM - BBC ADE assembler swaps meaning of < and > */
    lo_c = '>';
    hi_c = '<';
  }
  else if (!strcmp(p,"-opt-branch"))
    branchopt = 1;
  else if (!strcmp(p,"-illegal"))
    cpu_type |= ILL;
  else if (!strcmp(p,"-dtv"))
    cpu_type |= DTV;
  else if (!strcmp(p,"-c02"))
    cpu_type = M6502 | M65C02;
  else if (!strcmp(p,"-wdc02"))
    cpu_type = M6502 | M65C02 | WDC02 | WDC02ALL;
  else if (!strcmp(p,"-ce02"))
    cpu_type = M6502 | M65C02 | WDC02 | WDC02ALL | CSGCE02;
  else if (!strcmp(p,"-6280"))
    cpu_type = M6502 | M65C02 | WDC02 | WDC02ALL | HU6280;
  else if (!strcmp(p,"-mega65"))
    cpu_type = M6502 | M65C02 | WDC02 | CSGCE02 | M45GS02;
  else if (!strcmp(p,"-816") || !strcmp(p,"-802")) {
    cpu_type = M6502 | M65C02 | WDC02ALL | WDC65816;
    bytespertaddr = 3;
  }
  else
    return 0;

  return 1;
}
