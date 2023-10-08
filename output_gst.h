/* gst.h header file for Atari GST format */
/* (c) in 2023 by Frank Wille */

#define GSTESC 0xfb      /* Escape code for GST directives in the stream */

/* GST directives */
#define GST_SOURCE  0x01 /* string - source file name */
#define GST_COMMENT 0x02 /* string - comment to be ignored */
#define GST_ORG     0x03 /* long - start addr. of ORG section */
#define GST_SECTION 0x04 /* id - switch to section */
#define GST_OFFSET  0x05 /* long - move to absolute offset in section */
#define GST_XDEF    0x06 /* string long id - define symbol with value */
#define GST_XREF    0x07 /* long trunc-rule [op id] 0xfb - relocs/xrefs */
#define GST_DEFINE  0x10 /* id string - define section/symbol names */
#define GST_COMMON  0x12 /* id - switch to section for common symbol */
#define GST_END     0x13 /* end of current object module */

#define GSTOUT(f,d) fw16(f,(GSTESC<<8)|(d),1)

/* GST_XREF-directive trunc-rule bits */
#define GSTR_BYTE     0x01
#define GSTR_WORD     0x02
#define GSTR_LONG     0x04
#define GSTR_SIGNED   0x08
#define GSTR_UNSIGNED 0x10
#define GSTR_PCREL    0x20
#define GSTR_ABS      0x40
