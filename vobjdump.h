/*
 * vobjdump
 * Views the contents of a VOBJ file.
 * Written by Frank Wille <frank@phoenix.owl.de>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>

/* maximum VOBJ version to support */
#define VOBJ_MAX_VERSION 2

/* symbol types */
#define LABSYM 1
#define IMPORT 2
#define EXPRESSION 3

/* symbol flags */
#define TYPE(sym) ((sym)->flags&7)
#define TYPE_UNKNOWN  0
#define TYPE_OBJECT   1
#define TYPE_FUNCTION 2
#define TYPE_SECTION  3
#define TYPE_FILE     4

#define EXPORT 8
#define INEVAL 16
#define COMMON 32
#define WEAK 64


typedef int64_t taddr;
typedef uint8_t ubyte;

struct vobj_symbol {
  size_t offs;
  const char *name;
  int type,flags,sec,size;
  taddr val;
};

struct vobj_section {
  size_t offs;
  const char *name;
  taddr dsize,fsize;
};

#define STD_REL_TYPE(t) ((t)&0x1f)
#define REL_MOD_S 0x20
#define REL_MOD_U 0x40
#define FIRST_CPU_RELOC 0x80
#define makemask(x) (((x)>=sizeof(unsigned long long)*CHAR_BIT)?(~(unsigned long long)0):((((unsigned long long)1)<<(x))-1u))
