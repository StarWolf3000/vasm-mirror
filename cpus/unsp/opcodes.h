  /* ALU instructions */
  /* Each instruction comes in 3 forms:
   * - The basic 2-operand version where one of the source registers is also the destination
   * - A 3-operand form where the 2 sources are one register and one 16-bit (immediate or address)
   * - Another 3-operand form where the 2 sources are registers, and the destination is a 16 bit address
   */
  "add",  {MREG, MREG, M16BIT}, {0000000, ALU},
  "add",  {MREG, MANY},         {0000000, ALU},
  "add",  {MA16, MREG, MREG},   {0000000, ALUSTORE},

  "adc",  {MREG, MREG, M16BIT}, {0010000, ALU},
  "adc",  {MREG, MANY},         {0010000, ALU},
  "adc",  {MA16, MREG, MREG},   {0010000, ALUSTORE},

  "sub",  {MREG, MREG, M16BIT}, {0020000, ALU},
  "sub",  {MREG, MANY},         {0020000, ALU},
  "sub",  {MA16, MREG, MREG},   {0020000, ALUSTORE},

  "sbc",  {MREG, MREG, M16BIT}, {0030000, ALU},
  "sbc",  {MREG, MANY},         {0030000, ALU},
  "sbc",  {MA16, MREG, MREG},   {0030000, ALUSTORE},

  "cmp",  {MREG, MANY},         {0040000, ALU},

  "neg",  {MREG, MANY},         {0060000, ALU},
  "neg",  {MA16, MREG},         {0060000, ALUSTORE},

  "xor",  {MREG, MREG, M16BIT}, {0100000, ALU},
  "xor",  {MREG, MANY},         {0100000, ALU},
  "xor",  {MA16, MREG, MREG},   {0100000, ALUSTORE},

  "ld",   {MREG, MANY},         {0110000, ALU},

  "or",   {MREG, MREG, M16BIT}, {0120000, ALU},
  "or",   {MREG, MANY},         {0120000, ALU},
  "or",   {MA16, MREG, MREG},   {0120000, ALUSTORE},

  "and",  {MREG, MREG, M16BIT}, {0130000, ALU},
  "and",  {MREG, MANY},         {0130000, ALU},
  "and",  {MA16, MREG, MREG},   {0130000, ALUSTORE},

  "test", {MREG, MANY},         {0140000, ALU},

  "st",   {MREG, MMEMORY},      {0150000, STORE},

  /* Stack instructions */
  "pop",   {MREG, MREG, MREGA}, {0110200, POP},
  "pop",   {MREG, MREG},        {0110200, POP}, /* short version: pop/push from SP register */
  "push",  {MREG, MREG, MREGA}, {0150200, PUSH},
  "push",  {MREG, MREG},        {0150200, PUSH},
  "retf",  {0, 0},              {0115220, NO},
  "reti",  {0, 0},              {0115230, NO},

  /* Conditional jump instructions */
  "jcc",   {MPC6, 0},           {0007000, SO},
  "ljcc",  {MA22, 0},           {0017002, XJMP}, /* pseudo instruction for JCS PC+2; LJMP IMM16 */
  "jb",    {MPC6, 0},           {0007000, SO},
  "ljb",   {MA22, 0},           {0017002, XJMP},
  "jnae",  {MPC6, 0},           {0007000, SO},
  "ljnae", {MA22, 0},           {0017002, XJMP},

  "jcs",   {MPC6, 0},           {0017000, SO},
  "ljcs",  {MA22, 0},           {0007002, XJMP},
  "jnb",   {MPC6, 0},           {0017000, SO},
  "ljnb",  {MA22, 0},           {0007002, XJMP},
  "jae",   {MPC6, 0},           {0017000, SO},
  "ljae",  {MA22, 0},           {0007002, XJMP},

  "jsc",   {MPC6, 0},           {0027000, SO},
  "ljsc",  {MA22, 0},           {0037002, XJMP},
  "jge",   {MPC6, 0},           {0027000, SO},
  "ljge",  {MA22, 0},           {0037002, XJMP},
  "jnl",   {MPC6, 0},           {0027000, SO},
  "ljnl",  {MA22, 0},           {0037002, XJMP},

  "jss",   {MPC6, 0},           {0037000, SO},
  "ljss",  {MA22, 0},           {0027002, XJMP},
  "jnge",  {MPC6, 0},           {0037000, SO},
  "ljnge", {MA22, 0},           {0027002, XJMP},
  "jl",    {MPC6, 0},           {0037000, SO},
  "ljl",   {MA22, 0},           {0027002, XJMP},

  "jne",   {MPC6, 0},           {0047000, SO},
  "ljne",  {MA22, 0},           {0057002, XJMP},
  "jnz",   {MPC6, 0},           {0047000, SO},
  "ljnz",  {MA22, 0},           {0057002, XJMP},

  "jz",    {MPC6, 0},           {0057000, SO},
  "ljz",   {MA22, 0},           {0047002, XJMP},
  "je",    {MPC6, 0},           {0057000, SO},
  "lje",   {MA22, 0},           {0047002, XJMP},

  "jpl",   {MPC6, 0},           {0067000, SO},
  "ljpl",  {MA22, 0},           {0077002, XJMP},

  "jmi",   {MPC6, 0},           {0077000, SO},
  "ljmi",  {MA22, 0},           {0067002, XJMP},

  "jbe",   {MPC6, 0},           {0107000, SO},
  "ljbe",  {MA22, 0},           {0117002, XJMP},
  "jna",   {MPC6, 0},           {0107000, SO},
  "ljna",  {MA22, 0},           {0117002, XJMP},

  "jnbe",  {MPC6, 0},           {0117000, SO},
  "ljnbe", {MA22, 0},           {0107002, XJMP},
  "ja",    {MPC6, 0},           {0117000, SO},
  "lja",   {MA22, 0},           {0107002, XJMP},

  "jle",   {MPC6, 0},           {0127000, SO},
  "ljle",  {MA22, 0},           {0137002, XJMP},
  "jng",   {MPC6, 0},           {0127000, SO},
  "ljng",  {MA22, 0},           {0137002, XJMP},

  "jnle",  {MPC6, 0},           {0137000, SO},
  "ljnle", {MA22, 0},           {0127002, XJMP},
  "jg",    {MPC6, 0},           {0137000, SO},
  "ljg",   {MA22, 0},           {0127002, XJMP},

  "jvc",   {MPC6, 0},           {0147000, SO},
  "ljvc",  {MA22, 0},           {0157002, XJMP},

  "jvs",   {MPC6, 0},           {0157000, SO},
  "ljvs",  {MA22, 0},           {0147002, XJMP},

  "jmp",   {MPC6, 0},           {0167000, SO},
  "ljmp",  {MIMM16, 0},         {0117417, SO}, /* actually a pseudo-op for LD PC,#Imm16 */
      /* TODO that's actually the least useful thing you can do with PC as a target register,
       * since it is very similar to GOTO, takes as many cycles but can't jump to other segments.
       * Maybe we should allow LJMP with other type of arguments as well? Registers, indirect
       * jumps, ... Other operations with PC as a target or source are also possible. */

  /* Other instructions */
  "mul",     {MREG, MREG},        {0170410, MUL1},
  "mul.ss",  {MREG, MREG},        {0170410, MUL1}, /* alternate syntax for the above */
  "mul.us",  {MREG, MREG},        {0170010, MUL1},
  "call",    {MA22, 0},           {0170100, SO},
  "goto",    {MA22, 0},           {0177200, SO},
  "mac",     {MREG, MREG, MIMM6}, {0170600, MAC},
  "mac.ss",  {MREG, MREG, MIMM6}, {0170600, MAC},
  "mac.us",  {MREG, MREG, MIMM6}, {0170200, MAC},
  "int",     {MONOFF, 0},         {0170500, SO},
  "int",     {MONOFF, MONOFF, 0}, {0170500, SO},
  "fir_mov", {MONOFF, 0},         {0170504, FIRMOV},
  "irq",     {MONOFF, 0},         {0170510, SO},
  "fiq",     {MONOFF, 0},         {0170514, FIQ},
  "break",   {0, 0},              {0170520, NO},
  "nop",     {0, 0},              {0170545, NO},
