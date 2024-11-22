#include "vasm.h"

#ifdef OUTPAP

static char *copyright = "vasm MOS paper tape output module 0.1 (c) 2024 Dimitri Theulings";

static uint8_t *buffer;       /* output buffer for data records */
static uint8_t buffer_s = 24; /* default buffer size */
static uint8_t buffer_i = 0;  /* current index in buffer */

static uint32_t addr = 0;     /* current output address */
static uint32_t lines = 0;    /* number of lines in output file */

static int strict = 0;	      /* default not strict */

static void write_newline(FILE *f)
{
  uint8_t i;

  fw8(f,'\r');
  fw8(f,'\n');

  if(strict) /* pad file  with 6 NULL chars */
    for (i=0; i<6; i++)
      fw8(f,0);

  lines++;
}

static void write_eof_record(FILE *f)
{
  uint16_t csum = (lines>>8 & 0xFF) + (lines & 0xFF);

  fprintf(f, ";00%04X%04X", lines, csum);
  write_newline(f);

  if(strict) /* end file with XOFF */
      fw8(f,19);
}

static void write_data_record(FILE *f)
{
  uint32_t csum;
  uint8_t i;
  uint32_t start;

  /* pre-flight checks */
  if (buffer_i == 0)
    return;
  /* addr points to the next inserted byte
     addr-1 is the last one written */
  if (addr-1 > UINT16_MAX)
    output_error(11, addr-1);

  start = addr - buffer_i;
  start &= 0xFFFF;

  /* write data record */
  fprintf(f, ";%02X%04X", buffer_i, start);
  csum = buffer_i + (start>>8 & 0xFF) + (start &0xFF);
  for (i = 0; i < buffer_i; i++) {
    csum = (csum + buffer[i]) & 0xFFFF;
    fprintf(f, "%02X", buffer[i]);
  }
  fprintf(f, "%04X", csum);
  write_newline(f);

  /* reset the buffer index */
  buffer_i = 0;
}

static void buffer_data(FILE *f, uint8_t b)
{
  buffer[buffer_i] = b;
  buffer_i++;
  addr++;

  if (buffer_i == buffer_s)
    write_data_record(f);
}

static void write_output(FILE *f, section *sec, symbol *sym)
{
  uint32_t i, j;
  section *s;
  atom *a;

  if (!sec)
    return;

  for (; sym; sym = sym->next)
    if (sym->type == IMPORT)
      output_error(6, sym->name); /* undefined symbol (sym->name) */

  chk_sec_overlap(sec); /* fail on overlapping sections */

  buffer = mymalloc(sizeof(uint8_t) * buffer_s);

  for (s = sec; s; s = s->next) {
    addr = s->org;
    for (a = s->first; a; a = a->next) {
      if (a->type == DATA) {
        for (i = 0; i < a->content.db->size; i++) {
          buffer_data(f, a->content.db->data[i]);
        }
      } else if (a->type == SPACE) {
        for (i = 0; i < a->content.sb->space; i++) {
          for (j = 0; j < a->content.sb->size; j++) {
            buffer_data(f, a->content.sb->fill[j]);
          }
        }
      }
    }
    /* flush buffer before moving on to next section */
    write_data_record(f);
  }

  write_eof_record(f);
  myfree(buffer);
}

static int parse_args(char *arg)
{
  int size;
  long long val;

  if (!strcmp(arg, "-strict")) {
    strict = 1;
    return 1;
  }
  else if (!strncmp(arg, "-record-size=", 13)) {
    size = atoi(arg + 13);
    /* a PAP record (incl. header) cannot be longer than 0xFF bytes */
    if (size < 1 || size > 249)
      return 0;
    buffer_s = size;
    return 1;
  }
  else if (!strncmp(arg,"-start=",7)) {
    sscanf(arg+7,"%lli",&val);
    defsectorg = val;  /* set start of default section */
    return 1;
  }
  return 0;
}

int init_output_pap(char **cp, void (**wo)(FILE *, section *, symbol *), int (**oa)(char *))
{
  if (sizeof(utaddr) > sizeof(uint32_t)) {
    output_error(1, cpuname); /* output module doesn't support (cpuname) */
    return 0;
  }

  *cp = copyright;
  *wo = write_output;
  *oa = parse_args;
  defsecttype = emptystr;  /* default section is "org 0" */
  return 1;
}

#else
int init_output_pap(char **cp, void (**wo)(FILE *, section *, symbol *), int (**oa)(char *))
{
  return 0;
}
#endif
