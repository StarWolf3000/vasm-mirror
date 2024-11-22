/*
** cpu.c 650x/65C02/6280/45gs02/65816 cpu-description file
** (c) in 2002,2006,2008-2012,2014-2024 by Frank Wille
*/

#include "vasm.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};
const int mnemonic_cnt=sizeof(mnemonics)/sizeof(mnemonics[0]);

const char *cpu_copyright="vasm 6502 cpu backend 1.0 (c) 2002,2006,2008-2012,2014-2024 Frank Wille";
const char *cpuname = "6502";
int bytespertaddr = 2;

uint16_t cpu_type = M6502;
static int auto_mask,branchopt,dp_offset;
static uint16_t dpage;    /* zero/direct page (default 0) - set with SETDP */
static uint8_t asize = 8; /* Accumulator is 8 bits by default */
static uint8_t xsize = 8; /* Index registers are 8 bits by default */

static char lo_c = '<';   /* select low-byte or zero/direct page */
static char hi_c = '>';   /* select high-byte or full absolute */

static int OC_JMPABS,OC_BRA,OC_FIRSTMV,OC_LASTMV;

/* sizes for all operand types - refer to addressing modes enum in cpu.h */
const uint8_t opsize[NUM_OPTYPES] = {
  0,0,2,2,2,2,3,3,1,1,1,1,1,2,1,1,1,1,1,2,2,1,2,1,4,1,2,0,0,1,2,0,1,0
};

/* table for cpu specific extra directives */
struct ExtraDirectives {
  char *name;
  unsigned avail;
  char *(*func)(char *);
};
static hashtable *cpudirhash;


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
  if (s || current_section) {
    cpuopts *new = mymalloc(sizeof(cpuopts));

    new->dpage = dpage;
    new->asize = asize;
    new->xsize = xsize;
    add_atom(s,new_opts_atom(new));
  }
}


void print_cpu_opts(FILE *f,void *opts)
{
  fprintf(f,"opts: dp=%#04x a=%d x=%d",(unsigned)((cpuopts *)opts)->dpage,
          (int)((cpuopts *)opts)->asize,(int)((cpuopts *)opts)->xsize);
}


static int set_cpu_type(const char *n)
{
  int bpt = 2;

  if (!strnicmp(n,"ill",3) || !strnicmp(n,"6502i",5))
    cpu_type |= ILL;
  else if (!strnicmp(n,"dtv",3) || !strnicmp(n,"c64dtv",6)) {
    cpu_type &= ILL;
    cpu_type |= M6502 | DTV;
  }
  else if (!strncmp(n,"650",3) || !strncmp(n,"651",3)) {
    cpu_type &= ILL;
    cpu_type |= M6502;
  }
  else if (!stricmp(n,"65c02") || !stricmp(n,"c02"))
    cpu_type = M6502 | M65C02;
  else if (!stricmp(n,"wdc02") || !stricmp(n,"wdc65c02"))
    cpu_type = M6502 | M65C02 | WDC02 | WDC02ALL;
  else if (!stricmp(n,"ce02") || !stricmp(n,"65ce02"))
    cpu_type = M6502 | M65C02 | WDC02 | WDC02ALL | CSGCE02;
  else if (!strcmp(n,"mega65") || !strnicmp(n,"m45",3) || !strncmp(n,"45",2))
    cpu_type = M6502 | M65C02 | WDC02 | CSGCE02 | M45GS02 | M45GS02Q;
  else if (!strcmp(n,"6280") || !stricmp(n,"hu6280")) {
    cpu_type = M6502 | M65C02 | WDC02 | WDC02ALL | HU6280;
    dpage = 0x2000;
  }
  else if (!strcmp(n,"816") || !strcmp(n,"802") || !strncmp(n,"658",3)) {
    cpu_type = M6502 | M65C02 | WDC02ALL | WDC65816;
    bpt = 3;
  }
  else
    return 0;

  bytespertaddr = bpt;
  set_taddr();  /* changed bytes per address */
  return 1;
}


int parse_operand(char *p,int len,operand *op,int required)
{
  char *start = p;
  int indir = 0;
  int pfx = 2;
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
      case DATAOP:
        pfx = 1;  /* immediate/data allows different selector prefixes */
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
      case MVBANK:
      case REL8:
      case REL16:
      case ACCU:
        pfx = 0;  /* no prefix selector allowed */
      default:
        if (indir)
          return PO_NOMATCH;
        break;
    }

    if (required < ACCU) {
      /* Read optional bitstream/addressing-mode selector prefixes before */
      /* the expression. Their meaning is determined later in */
      /* eval_instruction() or eval_data() by the operand type, cpu */
      /* and accumular/index register width. */
      if (pfx && *p==lo_c) {
        p = skip(++p);
        op->flags |= OF_LO;  /* low-byte or 8-bit addressing */
      }
      else if (pfx && *p==hi_c) {
        p = skip(++p);
        op->flags |= OF_HI;  /* high-byte or 16/24-bit addressing */
      }
      else if (pfx==1 && (*p=='^' || *p=='`')) {
        p = skip(++p);
        op->flags |= OF_BK;  /* bank-byte */
      }
      else if (pfx==2 && (*p=='!' || *p=='|')) {
        p = skip(++p);
        op->flags |= OF_WA;  /* force 16-bit addressing mode */
      }
      op->value = parse_expr(&p);
    }
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

    if (IS_INDIR(required)) {  /* 16-bit indirect via dpage: (expr) */
      p = skip(p);
      if (*p++ != ')')
        return PO_NOMATCH;
    }

    if (IS_SQIND(required)) {  /* 24-bit indirect via dpage or abs: [expr] */
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
        ret = PO_COMB_REQ;  /* 2nd pass with same op to parse ",S/X/Y/Z" */
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


static char *handle_cpu(char *s)
{
  /* CPU directive only allowed before any code is generated */
  if (current_section == NULL) {
    if (!set_cpu_type(s))
      cpu_error(14,s);  /* unknown cpu model */
  }
  else
    cpu_error(13);  /* cpu must be defined before any code is generated */
  return skip_line(s);
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
  section *sec = new_section(dotdirs ? zeroname : zeroname+1, "aurwz",1);

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

static char *handle_longa(char *s)
{
  if (!strnicmp(s,"on",2))
    return handle_asize16(s+2);
  else if (!strnicmp(s,"off",3))
    return handle_asize8(s+3);
  cpu_error(9);  /* bad operand */
  return s;
}

static char *handle_longi(char *s)
{
  if (!strnicmp(s,"on",2))
    return handle_xsize16(s+2);
  else if (!strnicmp(s,"off",3))
    return handle_xsize8(s+3);
  cpu_error(9);  /* bad operand */
  return s;
}

static struct ExtraDirectives cpudirs[] = {
  "cpu",~0,handle_cpu,
  "setdp",~0,handle_setdp,
  "zpage",~0,handle_zpage,
  "zero",~0,handle_zero,
  "a8",WDC65816,handle_asize8,
  "a16",WDC65816,handle_asize16,
  "x8",WDC65816,handle_xsize8,
  "x16",WDC65816,handle_xsize16,
  "as",WDC65816,handle_asize8,
  "al",WDC65816,handle_asize16,
  "xs",WDC65816,handle_xsize8,
  "xl",WDC65816,handle_xsize16,
  "longa",WDC65816,handle_longa,
  "longi",WDC65816,handle_longi,
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
          /* we have an operand which may be 8-, 16- or 24-bit addressing */
          if (final) {
            if ((op->flags & OF_LO) && !mnemo->ext.zp_opcode)
              cpu_error(10);  /* zp/dp not available */
            if ((cpu_type & WDC65816) && (op->flags & OF_HI)
                && !mnemo->ext.al_opcode)
              cpu_error(12);  /* abslong not available */
          }
          if (mnemo->ext.zp_opcode && ((op->flags & OF_LO) ||
                                       (!(op->flags & (OF_HI|OF_WA)) &&
              ((base==NULL && (((utaddr)val>=(utaddr)dpage &&
                                (utaddr)val<=(utaddr)dpage+0xff))) ||
               (base!=NULL && ((base->flags & ZPAGESYM) || (LOCREF(base) &&
                               (base->sec->flags & NEAR_ADDRESSING)))))
             ))) {
            /* convert abs to a zero page addressing mode */
            op->type += DPAGE - ABS;
          }
          else if ((cpu_type & WDC65816) && mnemo->ext.al_opcode &&
                   ((op->flags & OF_HI) || (!(op->flags & (OF_WA|OF_LO)) &&
                    ((base==NULL && val>0xffff) ||
                     (base!=NULL && LOCREF(base) &&
                                    (base->sec->flags & FAR_ADDRESSING)))))) {
            /* convert abs to an absolute long addressing mode */
            op->type += LABS - ABS;
          }
        }

        else if (branchopt) {
          taddr bd = val - (pc + 2);

          if (op->type==REL8 && (base==NULL || !is_pc_reloc(base,sec)) &&
              (bd<-0x80 || bd>0x7f)) {
            if (mnemo->ext.opcode==0x80 || mnemo->ext.opcode==0x12) {
              /* translate out of range 65C02/DTV BRA to JMP */
              ip->code = OC_JMPABS;
              op->type = ABS;
            }
            else  /* branch dest. out of range: use a B!cc/JMP combination */
              op->type = RELJMP;
          }
          else if (ip->code==OC_JMPABS && (cpu_type&(DTV|M65C02))!=0 &&
                   (base==NULL || !is_pc_reloc(base,sec)) &&
                   bd>=-0x80 && bd<=0x80) {
            /* JMP may be optimized to a BRA */
            ip->code = OC_BRA;
            op->type = REL8;
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
  if (mnemonics[ip->code].ext.available & M45GS02Q)
    sz += 2;  /* add prefix for MEGA65 32-bit direct instructions */
  return sz;
}


size_t instruction_size(instruction *ip,section *sec,taddr pc)
{
  instruction *ipcopy;

  ipcopy = copy_inst(ip);
  optimize_instruction(ipcopy,sec,pc,0);
  return get_inst_size(ipcopy);
}


static void rangecheck(symbol *base,taddr val,operand *op)
{
  switch (op->type) {
    case ABS:
    case ABSX:
    case ABSY:
    case ABSZ:
    case INDIR:
    case INDIRX:
    case LINDIR:
    case RELJMP:
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
      if (base==NULL && (val<0 || val>0xff))
        cpu_error(11);   /* operand not in zero/direct page */
      break;
    case SR:
    case SRINDY:
      if (val<0 || val>0xff)
        cpu_error(5,8); /* operand doesn't fit into 8 bits */
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
  taddr val;
  int i;

  optimize_instruction(ip,sec,pc,1);  /* really execute optimizations now */

  db->size = get_inst_size(ip);
  d = db->data = mymalloc(db->size);

  /* write opcode */
  oc = mnemonics[ip->code].ext.opcode;
  for (i=0; i<MAX_OPERANDS; i++) {
    switch (ip->op[i]!=NULL ? ip->op[i]->type : IMPLIED) {
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

  if (mnemonics[ip->code].ext.available & M45GS02Q) {
    /* prefix for MEGA65 32-bit direct instructions using registers AXYZ */
    *d++ = 0x42;
    *d++ = 0x42;
  }
  else if (ip->code>=OC_FIRSTMV && ip->code<=OC_LASTMV) {
    /* swap source and destination operand of MVN and MVP instructions */
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
      int optype = op->type;
      int offs = d - db->data;
      symbol *base;

      if (op->value != NULL) {
        if (!eval_expr(op->value,&val,sec,pc)) {
          int btype = find_base(op->value,&base,sec,pc);

          if (btype==BASE_PCREL && (optype==IMMED8 || optype==IMMED16))
            op->flags |= OF_PC;  /* immediate value with pc-rel. relocation */

          if (optype==WBIT || btype==BASE_ILLEGAL ||
              (btype==BASE_PCREL && !(op->flags & OF_PC))) {
            general_error(38);  /* illegal relocation */
          }
          else {
            if ((optype==REL8 || optype==REL16) && !is_pc_reloc(base,sec)) {
              /* relative branch requires no relocation */
              val = val - (pc + offs + (optype==REL8 ? 1 : 2));
            }
            else {
              taddr add = val;  /* reloc addend */
              taddr mask = -1;
              int type = REL_ABS;
              int size;

              switch (optype) {
                case LABS:
                case LABSX:
                  if (op->flags & (OF_LO|OF_WA))
                    cpu_error(2);  /* selector prefix ignored */
                  size = 24;
                  break;
                case RELJMP:
                  offs += 2;  /* skip branch offset and JMP-opcode */
                case ABS:
                case ABSX:
                case ABSY:
                case ABSZ:
                case INDIR:
                case INDIRX:
                case LINDIR:
                  if ((cpu_type & WDC65816) || (op->flags&(OF_HI|OF_WA)))
                    mask = 0xffff;
                  else if (op->flags & OF_LO)
                    cpu_error(2);  /* selector prefix ignored */
                  size = 16;
                  break;
                case QDPINDZ:
                  offs++;  /* include prefix-byte */
                case DPAGE:
                case DPAGEX:
                case DPAGEY:
                case DPAGEZ:
                case DPINDX:
                case DPINDY:
                case DPINDZ:
                case DPIND:
                case LDPIND:
                case LDPINDY:
                  if (dp_offset)
                    type = REL_SECOFF;  /* 8-bit offset to DP-section */
                  if (!dp_offset || (op->flags & OF_LO))
                    mask = 0xff;
                  if (op->flags & (OF_HI|OF_WA))
                    cpu_error(2);  /* selector prefix ignored */
                  size = 8;
                  break;
                case SR:
                case SRINDY:
                  if (op->flags & (OF_LO|OF_WA|OF_HI))
                    cpu_error(2);  /* selector prefix ignored */
                  size = 8;
                  break;
                case MVBANK:
                  mask = 0xff0000;
                  val = (val >> 16) & 0xff;
                  size = 8;
                  break;
                case IMMED8:
                  if (op->flags & OF_PC) {
                    type = REL_PC;
                    add += offs;
                    val += offs;
                    if (op->flags & (OF_LO|OF_HI|OF_BK))
                      cpu_error(2);  /* selector prefix ignored */
                  }
                  else if (op->flags & OF_BK) {
                    mask = 0xff0000;
                    val = (val >> 16) & 0xff;
                  }
                  else if (op->flags & OF_HI) {
                    mask = 0xff00;
                    val = (val >> 8) & 0xff;
                  }
                  else if ((op->flags & OF_LO) || auto_mask) {
                    mask = 0xff;
                    val &= 0xff;
                  }
                  size = 8;
                  break;
                case IMMED16:
                  if (op->flags & OF_PC) {
                    type = REL_PC;
                    val += offs;
                    if (op->flags & (OF_LO|OF_HI|OF_BK))
                      cpu_error(2);  /* selector prefix ignored */
                  }
                  else if (op->flags & OF_BK) {
                    mask = 0xffff0000;
                    val = (val >> 16) & 0xffff;
                  }
                  else if (op->flags & OF_HI) {
                    mask = 0xffff00;
                    val = (val >> 8) & 0xffff;
                  }
                  else if ((op->flags & OF_LO) || auto_mask) {
                    mask = 0xffff;
                    val &= 0xffff;
                  }
                  size = 16;
                  break;
                case REL8:
                  type = REL_PC;
                  size = 8;
                  add -= -1;  /* 6502 addend correction */
                  break;
                case REL16:
                  type = REL_PC;
                  size = 16;
                  add -= -2;  /* 6502 addend correction */
                  break;
                default:
                  ierror(0);
                  break;
              }
              add_extnreloc_masked(&db->relocs,base,add,type,0,size,offs,mask);
            }
          }
        }
        else {
          /* constant/absolute value */
          base = NULL;
          switch (optype) {
            case DPAGE:
            case DPAGEX:
            case DPAGEY:
            case DPAGEZ:
            case DPINDX:
            case DPINDY:
            case DPINDZ:
            case DPIND:
            case LDPINDY:
            case QDPINDZ:
            case LDPIND:
              if (op->flags & OF_LO)
                val &= 0xff;
              else
                val -= dpage;
              if (op->flags & (OF_HI|OF_WA))
                cpu_error(2);  /* selector prefix ignored */
              break;
            case ABS:
            case ABSX:
            case ABSY:
            case ABSZ:
            case INDIR:
            case INDIRX:
            case LINDIR:
            case RELJMP:
              if ((cpu_type & WDC65816) || (op->flags&(OF_HI|OF_WA)))
                val &= 0xffff;  /* ignore the bank, assume DBR is correct */
              else if (op->flags & OF_LO)
                cpu_error(2);  /* selector prefix ignored */
              break;
            case LABS:
            case LABSX:
              val &= 0xffffff;
              if (op->flags & (OF_LO|OF_WA))
                cpu_error(2);  /* selector prefix ignored */
              break;
            case REL8:
              val -= pc + offs + 1;
              break;
            case REL16:
              val -= pc + offs + 2;
              break;
            case MVBANK:
              val = (val >> 16) & 0xff;
            case SR:
            case SRINDY:
              if (op->flags & (OF_LO|OF_WA|OF_HI))
                cpu_error(2);  /* selector prefix ignored */
              break;
            case IMMED8:
              if (op->flags & OF_BK)
                val = (val >> 16) & 0xff;
              else if (op->flags & OF_HI)
                val = (val >> 8) & 0xff;
              else if ((op->flags & OF_LO) || auto_mask)
                val &= 0xff;
              break;
            case IMMED16:
              if (op->flags & OF_BK)
                val = (val >> 16) & 0xffff;
              else if (op->flags & OF_HI)
                val = (val >> 8) & 0xffff;
              else if ((op->flags & OF_LO) || auto_mask)
                val &= 0xffff;
              break;
          }
        }

        rangecheck(base,val,op);

        /* write operand data */
        switch (optype) {
          case ABSX:
          case ABSY:
          case ABSZ:
            if (!*(db->data)) /* STX/STY allow only ZeroPage addressing mode */
              cpu_error(0);
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
	    d[-1] = 0xea;  /* MEGA65 32-bit indirect prefix */
	    d++;
          case DPAGE:
          case DPAGEX:
          case DPAGEY:
          case DPAGEZ:
          case LDPIND:
          case LDPINDY:
          case DPIND:
          case DPINDX:
          case DPINDY:
          case DPINDZ:
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
  rlist *rl = NULL;
  taddr val;

  if (bitsize>32 || (bitsize&7))
    cpu_error(3,bitsize);  /* data size not supported */

  db->size = bitsize >> 3;
  db->data = mymalloc(db->size);

  if (!eval_expr(op->value,&val,sec,pc)) {
    symbol *base;
    int btype = find_base(op->value,&base,sec,pc);

    if (btype==BASE_OK ||
        (btype==BASE_PCREL && !(op->flags&(OF_LO|OF_HI|OF_BK)))) {
      rl = add_extnreloc(&db->relocs,base,val,
                         btype==BASE_PCREL?REL_PC:REL_ABS,0,bitsize,0);
    }
    else
      general_error(38);  /* illegal relocation */
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
      else if (op->flags & OF_BK) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = 0xff0000;
        val = (val >> 16) & 0xff;
      }
      if (val<-0x80 || val>0xff)
        cpu_error(5,8);  /* operand doesn't fit into 8 bits */
      break;

    case 16:
      if ((op->flags & OF_LO) || auto_mask) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = 0xffff;
        val &= 0xffff;
      }
      else if (op->flags & OF_HI) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = 0xffff00;
        val = (val >> 8) & 0xffff;
      }
      else if (op->flags & OF_BK) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = 0xffff0000;
        val = (val >> 16) & 0xffff;
      }
      if (val<-0x8000 || val>0xffff)
        cpu_error(5,16);  /* operand doesn't fit into 16 bits */
      break;

    case 24:
      if ((op->flags & OF_LO) || auto_mask) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = 0xffffff;
        val &= 0xffffff;
      }
      else if (op->flags & OF_HI) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = 0xffffff00;
        val = (val >> 8) & 0xffffff;
      }
      else if (op->flags & OF_BK) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = ~0xffff;  /* @@@ */
        val = (val >> 16) & 0xffffff;
      }
      if (val<-0x800000 || val>0xffffff)
        cpu_error(5,24);  /* operand doesn't fit into 24 bits */
      break;

    case 32:
      /* @@@ does that make sense? */
      if (op->flags & OF_HI) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = ~0xff;
        val >>= 8;
      }
      else if (op->flags & OF_BK) {
        if (rl)
          ((nreloc *)rl->reloc)->mask = ~0xffff;
        val >>= 16;
      }
      break;
  }

  setval(0,db->data,db->size,val);
  return db;
}


operand *new_operand(void)
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


int init_cpu(void)
{
  hashdata data;
  int i;

  for (i=0; i<mnemonic_cnt; i++) {
    if (mnemonics[i].ext.available & cpu_type) {
      if (!strcmp(mnemonics[i].name,"jmp") && mnemonics[i].operand_type[0]==ABS)
        OC_JMPABS = i;
      else if (!strcmp(mnemonics[i].name,"bra"))
        OC_BRA = i;
      else if (!strcmp(mnemonics[i].name,"mvn") && !OC_FIRSTMV)
        OC_FIRSTMV = i;
      else if (!strcmp(mnemonics[i].name,"mvp"))
        OC_LASTMV = i;
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
  else if (!strcmp(p,"-am"))
    auto_mask = 1;
  else if (!strcmp(p,"-dpo"))
    dp_offset = 1;
  else if (!strcmp(p,"-opt-branch"))
    branchopt = 1;
  else if (*p!='-' || !set_cpu_type(p+1))
    return 0;

  return 1;
}
