# Unix, using gcc

CC = gcc
TARGET =
TARGETEXTENSION =

CCOUT = -o $(DUMMY)
CFLAGS = -c -std=c90 -O2 -pedantic -Wno-long-long -DUNIX $(OUTFMTS)

LD = $(CC)
LDOUT = $(CCOUT)
LDFLAGS = -lm

RM = rm -f

include make.rules
