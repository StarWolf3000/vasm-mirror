# Unix, using gcc

CC = gcc
TARGET =
TARGETEXTENSION =
OUTFMTS = -DOUTAOUT -DOUTBIN -DOUTELF -DOUTHUNK -DOUTSREC -DOUTTOS -DOUTVOBJ \
          -DOUTXFIL -DOUTIHEX

CCOUT = -o
COPTS = -c -std=c99 -O2 -Wpedantic -DUNIX $(OUTFMTS)

LD = $(CC)
LDOUT = $(CCOUT)
LDFLAGS = -lm

RM = rm -f

include make.rules
