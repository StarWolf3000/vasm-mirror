/* NOTE: Errors without NOLINE have to be printed by output_atom_error() */
  "sections <%s>:%llx-%llx and <%s>:%llx-%llx must not overlap",FATAL|ERROR|NOLINE,
  "output module doesn't support cpu %s",ERROR|NOLINE,
  "write error",FATAL|ERROR|NOLINE,
  "section attributes <%s> not supported",ERROR|NOLINE,
  "reloc type %d, size %u, mask %#lx (symbol %s + %#lx) not supported",ERROR,
  "reloc type %d not supported",ERROR,                              /* 05 */
  "undefined symbol <%s>",ERROR|NOLINE,
  "output module doesn't allow multiple sections of the same type (%s)",FATAL|ERROR|NOLINE,
  "undefined symbol <%s> at %s+%#lx, reloc type %d",ERROR,
  "section <%s>: alignment padding (%lu) not a multiple of %lu at %#llx",WARNING|NOLINE,
  "weak symbol <%s> not supported by output format, treating as global",WARNING|NOLINE, /* 10 */
  "address %#llx out of range for selected format",ERROR|NOLINE,
  "reloc type %d, mask %#lx to symbol %s + %#lx does not fit into %u bits",ERROR,
  "data definition following a databss space directive",WARNING,
  "file option %d max size exceeded: %lu",WARNING|NOLINE,
  "absolute file path exceeds maximum size of %d characters",FATAL|ERROR|NOLINE, /* 15 */
  "converting NONE relocation <%s> to 8-bit ABS with zero addend",WARNING|NOLINE,
  "no additional space in section to convert NONE relocation <%s> to ABS",ERROR|NOLINE,
  "section <%s>: kickstart 1.x cannot initialize bss sections >256k to zero",WARNING|NOLINE,
  "too many symbols for selected output file format",ERROR|NOLINE,
  "all sections are absolute, nothing to relocate",WARNING|NOLINE,  /* 20 */
  "expression type of symbol %s not supported",ERROR|NOLINE,
  "unaligned relocation offset at %s+%#lx",WARNING,
  "section <%s>: maximum size of 0x%llx bytes exceeded (0x%llx)",FATAL|ERROR|NOLINE,
