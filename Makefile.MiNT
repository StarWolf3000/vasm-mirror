# Atari TOS/MiNT

TARGET = _MiNT
TARGETEXTENSION =

CC = vc +mint
CCOUT = -o=
CFLAGS = -c -merge-strings -cpu=68020 -O1 $(OUTFMTS)

LD = $(CC)
LDOUT = $(CCOUT)
LDFLAGS = -lm

RM = rm -f

include make.rules
