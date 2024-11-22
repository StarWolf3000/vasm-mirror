/*
 * cpu.c HANS cpu description file
 */

#include "vasm.h"

mnemonic mnemonics[] = {
  "add",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(0) },
  "sub",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(1) },
  "mul",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(2) },
  "sqrt",    { TargetReg,SourceReg1 },             { FORMR(3) },
  "div",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(4) },
  "mod",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(5) },
  "sla",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(6) },
  "sra",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(7) },
  "ce",      { TargetReg,SourceReg1,SourceReg2 },  { FORMR(8) },
  "cne",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(9) },
  "cg",      { TargetReg,SourceReg1,SourceReg2 },  { FORMR(10) },
  "cl",      { TargetReg,SourceReg1,SourceReg2 },  { FORMR(11) },
  "cgu",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(12) },
  "clu",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(13) },
  "cgeu",    { TargetReg,SourceReg2,SourceReg1 },  { FORMR(13) },
  "cleu",    { TargetReg,SourceReg2,SourceReg1 },  { FORMR(12) },
  "cge",     { TargetReg,SourceReg2,SourceReg1 },  { FORMR(11) },
  "cle",     { TargetReg,SourceReg2,SourceReg1 },  { FORMR(10) },
  "itof",    { TargetFloatReg,SourceReg1 },        { FORMR(14) },
  "uitof",   { TargetFloatReg,SourceReg1 },        { FORMR(15) },
  "not",     { TargetReg,SourceReg1 },             { FORMR(16) },
  "and",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(17) },
  "or",      { TargetReg,SourceReg1,SourceReg2 },  { FORMR(18) },
  "xor",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(19) },
  "xnor",    { TargetReg,SourceReg1,SourceReg2 },  { FORMR(20) },
  "sll",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(22) },
  "srl",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(23) },
  "add.s",   { TargetFloatReg,SourceFloatReg1,SourceFloatReg2 },  { FORMR(32) },
  "sub.s",   { TargetFloatReg,SourceFloatReg1,SourceFloatReg2 },  { FORMR(33) },
  "mul.s",   { TargetFloatReg,SourceFloatReg1,SourceFloatReg2 },  { FORMR(34) },
  "sqrt.s", { TargetFloatReg,SourceFloatReg1 },                  { FORMR(35) },
  "div.s",   { TargetFloatReg,SourceFloatReg1,SourceFloatReg2 },  { FORMR(36) },
  "ce.s",    { TargetReg,SourceFloatReg1,SourceFloatReg2 },       { FORMR(40) },
  "cne.s",   { TargetReg,SourceFloatReg1,SourceFloatReg2 },       { FORMR(41) },
  "cg.s",    { TargetReg,SourceFloatReg1,SourceFloatReg2 },       { FORMR(42) },
  "cl.s",    { TargetReg,SourceFloatReg1,SourceFloatReg2 },       { FORMR(43) },
  "cge.s",    { TargetReg,SourceFloatReg2,SourceFloatReg1 },       { FORMR(43) },
  "cle.s",    { TargetReg,SourceFloatReg2,SourceFloatReg1 },       { FORMR(42) },
  "ftoi",    { TargetReg,SourceFloatReg1 },                       { FORMR(46) },
  "ftoui",    { TargetReg,SourceFloatReg1 },                       { FORMR(47) },
  "addi",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(0) },
  "subi",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(1) },
  "muli",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(2) },
  "divi",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(4) },
  "modi",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(5) },
  "slai",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(6) },
  "srai",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(7) },
  "cei",     { TargetReg,SourceReg1,Immediate16 },  { FORMI(8) },
  "cnei",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(9) },
  "cgi",     { TargetReg,SourceReg1,Immediate16 },  { FORMI(10) },
  "cli",     { TargetReg,SourceReg1,Immediate16 },  { FORMI(11) },
  "cgei",    { TargetReg,SourceReg1,Immediate16Minus1 },  { FORMI(10) },
  "clei",    { TargetReg,SourceReg1,Immediate16Plus1 },  { FORMI(11) },
  "cgui",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(12) },
  "clui",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(13) },
  "cgeui",   { TargetReg,SourceReg1,Immediate16Minus1 },  { FORMI(12) },
  "cleui",   { TargetReg,SourceReg1,Immediate16Plus1 },  { FORMI(13) },
  "addis",   { TargetReg,SourceReg1,Immediate16 },  { FORMI(16) },
  "andi",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(17) },
  "ori",     { TargetReg,SourceReg1,Immediate16 },  { FORMI(18) },
  "xori",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(19) },
  "xnori",   { TargetReg,SourceReg1,Immediate16 },  { FORMI(20) },
  "slli",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(22) },
  "srli",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(23) },
  "load",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(24) },
  "load.s",  { TargetFloatReg,SourceReg1,Immediate16 },            { FORMI(25) },
  "store",   { SourceReg1,TargetReg,Immediate16 },  { FORMI(26) },
  "store.s", { SourceReg1,TargetFloatReg,Immediate16 },            { FORMI(27) },
  "jreg",    { SourceReg1 },                        { FORMI(28) },
  "bez",     { SourceReg1, Immediate16Label },      { FORMI(29) },
  "bnez",    { SourceReg1, Immediate16Label },      { FORMI(30) },
  "jal",     { TargetReg, Immediate16Label },       { FORMI(31) },
  "jmp",     { Immediate26Label },                  { FORMJ(0) }
};
const int mnemonic_cnt = sizeof(mnemonics) / sizeof(mnemonics[0]);

const char* cpu_copyright = "vasm hans cpu backend 1.1 (c)2024 by Yannik Stamm";
const char* cpuname = "hans";
int bytespertaddr = 1;

operand* new_operand()
{
    operand* new = mymalloc(sizeof(*new));
    return new;
}

/*Returns the number of the register identifier, or a constant*/
static int parse_reg(char** start, int regtype)
{
    char* stringPos = *start;
    regsym* symbol;
    int registerIndex;
    unsigned int identifierLength;

    /*If string is an identifier...*/
    if (ISIDSTART(*stringPos))
    {
        stringPos++;
        while (ISIDCHAR(*stringPos)) stringPos++;

        /*...find that identifier and return the register number it references */
        identifierLength = stringPos - *start;

        if (symbol = find_regsym_nc(*start, identifierLength))
        {
            if (symbol->reg_type == regtype)
            {
                *start = stringPos;
                return symbol->reg_num;
            }
            
            return -1; /*In case register type is not the expected type, die*/
        }
    }

    /*...else if it is a number, check if it */
    /*is inside the valid interval and return it*/
    /* get register number */
    stringPos = *start;
    registerIndex = (int)parse_constexpr(&stringPos);  
    if (registerIndex >= 0 && registerIndex <= 31)
    {
        *start = stringPos;
        return registerIndex;
    }

    return -1;
}

static void resolve_high_low_label_reference
        (operand* operand, char* lastSymbolOfName)
{
    operand->labelType = DefaultLabel;
    if (*lastSymbolOfName != '@') return;

    if (*(lastSymbolOfName + 1) == 'l' || *(lastSymbolOfName + 1) == 'L')
    {
        operand->labelType = LowLabel;
    }
    else if (*(lastSymbolOfName + 1) == 'h' || *(lastSymbolOfName + 1) == 'H')
    {
        if (*(lastSymbolOfName + 2) == 'a' || *(lastSymbolOfName + 2) == 'A')
        {
            operand->labelType = HighAlgebraicLabel;
        }
        else
        {
            operand->labelType = HighLabel;
        }
    }

}

/*Sets operand->reg to the appropriate register and returns, */
/*if it was successful or not (returns PO_MATCH or PO_NOMATCH).*/
/*If the operand is an immediate-label, set operand->expr instead of operand->reg */
int parse_operand(char* start, int len, operand* operand, int requiredOperandType)
{
    /*Skips spaces*/
    start = skip(start);

    switch (requiredOperandType)
    {
    case SourceReg1:
    case SourceReg2:
    case TargetReg:
        if ((operand->reg = parse_reg(&start, RTYPE_R)) >= 0)
            return PO_MATCH;
        break;

    case SourceFloatReg1:
    case SourceFloatReg2:
    case TargetFloatReg:
        if ((operand->reg = parse_reg(&start, RTYPE_F)) >= 0)
            return PO_MATCH;
        break;

    case Immediate16:
        if (*start == '#')
            start = skip(start + 1);  /* skip optional '#' */
    case Data:
    case Immediate16Label:
    case Immediate26Label:
        operand->exp = parse_expr(&start);
        resolve_high_low_label_reference(operand, start);
        return PO_MATCH;
    case Immediate16Plus1:
        if (*start == '#')
            start = skip(start + 1);  /* skip optional '#' */
        operand->exp = make_expr(ADD, parse_expr(&start), number_expr(1));
        return PO_MATCH;
    case Immediate16Minus1:
        if (*start == '#')
            start = skip(start + 1);  /* skip optional '#' */
        operand->exp = make_expr(SUB, parse_expr(&start), number_expr(1));
        return PO_MATCH;
    }
    
    return PO_NOMATCH;
}


char* parse_cpu_special(char* s)
{
    return s;  /* nothing special */
}

/*Instructions are always one byte (= 32 Bit for this CPU),*/
/*so we can always return 1 here*/
size_t instruction_size(instruction* ip, section* sec, taddr pc)
{
    return 1;
}

/*Handles low and high labels*/
static uint32_t add_immediate(uint32_t opCode, taddr immediateValue, operand* operand)
{
    if (operand->labelType == HighLabel)
    {
        opCode |= (immediateValue & 0xffff0000) >> 16;
    }
    else if (operand->labelType == HighAlgebraicLabel)
    {
        uint16_t higherHalf = (immediateValue & 0xffff0000) >> 16;
        uint16_t lowerHalf = immediateValue & 0x0000ffff;
        if (lowerHalf > (1 << 15) - 1)
        {
            higherHalf += 1;
        }
        opCode |= higherHalf;
    }
    else
    {
        opCode |= immediateValue & 0x0000ffff;
    }

    return opCode;
}

int generateAddendum(operand *operand)
{
    int addendum;
    addendum = 0;
    if (operand->exp->type == ADD)
    {
        if (operand->exp->left->type == NUM)
            addendum = operand->exp->left->c.val;
        else if (operand->exp->right->type == NUM)
            addendum = operand->exp->right->c.val;
        else
            general_error(38); /*Illegal relocation*/
    }
    if (operand->exp->type == SUB)
        if (operand->exp->right->type == NUM && operand->exp->left->type == SYM)
            addendum = -operand->exp->right->c.val;
        else
            general_error(38); /*Illegal relocation*/
    return addendum;
}

/*Converts the received instruction into its final binary format*/
dblock* eval_instruction
        (instruction* instruction, section* section, taddr programCounter)
{
    mnemonic* mnemonic = &mnemonics[instruction->code];
    uint32_t opCode = mnemonic->ext.opcode;
    dblock* dataBlock = new_dblock();
    int i;

    dataBlock->size = 1;
    dataBlock->data = mymalloc(OCTETS(dataBlock->size));

    /* assemble all operands into the opcode */
    for (i = 0; i < MAX_OPERANDS; i++)
    {
        operand* operand = instruction->op[i];

        /*"A non-constant expression is based on a (defined or undefined) symbol*/
        /*(baseOfImmediate) and an addend (ImmediateValue)."*/
        symbol* baseOfImmediate = NULL;
        taddr immediateValue; 

        if (operand == NULL)
            break;  /* no more operands */
        if (operand->exp != NULL)
        {
            /*If immediateValue depends on a symbol, find that symbol*/
            if (!eval_expr(operand->exp, &immediateValue, section, programCounter))
            {
                int result = find_base(operand->exp, &baseOfImmediate, section, programCounter);
                if (result != BASE_OK)
                    general_error(38);
            }
        }


        switch (mnemonic->operand_type[i])
        {
        case TargetReg:
        case TargetFloatReg:
            opCode |= (operand->reg & 31) << 21;
            break;
        case SourceReg1:
        case SourceFloatReg1:
            opCode |= (operand->reg & 31) << 16;
            break;
        case SourceReg2:
        case SourceFloatReg2:
            opCode |= (operand->reg & 31) << 11;
            break;
        case Immediate16Minus1:
            if (strcmp(mnemonic->name, "cgei") == 0 && immediateValue < -0x8000 || immediateValue > 0x7FFF)
                cpu_error(4, immediateValue + 1, "[-32767:32768]", mnemonic->name);
            if (strcmp(mnemonic->name, "cgeui") == 0 && immediateValue < 0x0000 || immediateValue > 0xFFFF)
                cpu_error(4, immediateValue + 1, "[1:65536]", mnemonic->name);
        case Immediate16Plus1:
            if (strcmp(mnemonic->name, "clei") == 0 && immediateValue < -0x8000 || immediateValue > 0x7FFF)
                cpu_error(4, immediateValue - 1, "[-32769:32766]", mnemonic->name);
            if (strcmp(mnemonic->name, "cleui") == 0 && immediateValue < -0x0000 || immediateValue > 0xFFFF)
                cpu_error(4, immediateValue - 1, "[-1:65534]", mnemonic->name);
        case Immediate16:
            if (baseOfImmediate != NULL)
            {
                rlist* newReloc;
                int addendum;
                /* external label or label from a different section needs reloc */
                int mask;
                if (operand->labelType == HighLabel)
                {
                    mask = 0xFFFF0000;
                }
                else if (operand->labelType == HighAlgebraicLabel)
                {
                    rlist* reloc;
                    mask = 0xFFFF0000;

                    reloc = add_extnreloc(&dataBlock->relocs,
                        baseOfImmediate, 0, REL_ABS, 16, 16, 0);
                    ((nreloc*)reloc->reloc)->mask = 0x8000;
                }
                else if (operand->labelType == LowLabel)
                {
                    mask = 0xFFFF;
                }
                else
                    mask = 0;

                addendum = generateAddendum(operand);
                newReloc = add_extnreloc(&dataBlock->relocs,
                    baseOfImmediate, addendum, REL_ABS, 16, 16, 0);
                ((nreloc*)newReloc->reloc)->mask = mask;
            }
            /*Only throw warning if immediate out of range of 16 Bit signed/unsigned int*/
            if (immediateValue < -0x8000 || immediateValue > 0xFFFF)
                cpu_error(1, immediateValue);


            opCode = add_immediate(opCode, immediateValue, operand);
            break;
        case Immediate16Label:
            /*If operand contains a label*/
            if (baseOfImmediate != NULL)
            {
                /*If symbol is extern*/
                if (is_pc_reloc(baseOfImmediate, section))
                {
                    int addendum;
                    addendum = generateAddendum(operand);
                    /* external label or label from a different section needs reloc */
                    add_extnreloc(&dataBlock->relocs,
                        baseOfImmediate, addendum, REL_PC, 16, 16, 0);
                }
            }
            immediateValue -= programCounter + 1;

            /*Only throw warning if immediate out of range of 16 Bit signed int*/
            if (immediateValue < -0x8000 || immediateValue > 0x7FFF)
                cpu_error(2, immediateValue);
            /*Should not be used with @l, @h, @ha so no add_immediate*/
            opCode |= immediateValue & 0xffff;
            break;
        case Immediate26Label:
            /*If operand contains a label*/
            if (baseOfImmediate != NULL)
            {
                /*If symbol is extern*/
                if (is_pc_reloc(baseOfImmediate, section))
                {
                    int addendum;
                    addendum = generateAddendum(operand);
                    /* external label or label from a different section needs reloc */
                    add_extnreloc(&dataBlock->relocs,
                        baseOfImmediate, addendum, REL_PC, 6, 26, 0);
                }
            }
            immediateValue -= programCounter + 1;
            if (immediateValue < -0x2000000 || immediateValue > 0x1FFFFFF)
                cpu_error(3, immediateValue);

            /*Should not be used with @l, @h, @ha so no add_immediate*/
            opCode |= immediateValue & 0x3ffffff; 
            break;
        case Data:
            ierror(0);  /* should be handled by eval_data() */
            break;
        }
    }

    /* write opcode - endianness doesn't matter */
    setval(1, dataBlock->data, dataBlock->size, opCode);
    return dataBlock;
}

/*Converts the received data operand into its final binary format*/
dblock* eval_data
    (operand* operand, size_t bitsize, section* section, taddr programCounter)
{
    dblock* dataBlock = new_dblock();
    taddr value;
    if (bitsize % BITSPERBYTE)
        cpu_error(0, bitsize);  /* data size not supported */

    dataBlock->size = bitsize / BITSPERBYTE;
    dataBlock->data = mymalloc(OCTETS(dataBlock->size));

    /* evaluate expression, get baseOfImmediate and type for relocations */
    if (!eval_expr(operand->exp, &value, section, programCounter))
    {
        symbol* baseOfImmediate;
        int baseType;

        baseType = find_base(operand->exp, &baseOfImmediate, section, programCounter);
        if (baseType == BASE_OK || baseType == BASE_PCREL)
        {
            int addendum;
            addendum = generateAddendum(operand);
            add_extnreloc(&dataBlock->relocs, baseOfImmediate, addendum,
                baseType == BASE_PCREL ? REL_PC : REL_ABS, 0, bitsize, 0);
        }
        else
            general_error(38);  /* illegal relocation */
    }

    setval(1, dataBlock->data, dataBlock->size, value);
    return dataBlock;
}


int init_cpu()
{
    char r[4];
    int i;

    /* define register symbols */
    for (i = 0; i < 32; i++)
    {
        sprintf(r, "r%d", i);
        new_regsym(0, 1, r, RTYPE_R, 0, i);
        r[0] = 'f';
        new_regsym(0, 1, r, RTYPE_F, 0, i);
    }

    return 1;
}


int cpu_args(char* p)
{
    return 0;
}
