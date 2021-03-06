# $Id: Makefile 5 2006-01-12 21:41:36Z matled $

CC=gcc
LD=gcc
STRIP=strip
RM=rm -f --

-include Makefile.local

OBJECTS+=$(patsubst %.c,%.o,$(shell echo *.c))
HEADERS+=$(shell echo *.h)
TARGET?=serialclient

LDFLAGS+=
CFLAGS+=-std=c99 -pedantic-errors -Wall -W

ifneq ($(DEBUG),) # {{{
	CFLAGS+=-ggdb3
	CFLAGS+=-Wchar-subscripts -Wmissing-prototypes
	CFLAGS+=-Wmissing-declarations -Wredundant-decls
	CFLAGS+=-Wstrict-prototypes -Wshadow -Wbad-function-cast
	CFLAGS+=-Winline -Wpointer-arith -Wsign-compare
	CFLAGS+=-Wunreachable-code -Wdisabled-optimization
	CFLAGS+=-Wcast-align -Wwrite-strings -Wnested-externs -Wundef
	CFLAGS+=-DDEBUG
	LDFLAGS+=
else
	CFLAGS+=-O2
	LDFLAGS+=
endif
# }}}
ifneq ($(PROFILING),) # {{{
	CFLAGS+=-pg -DPROFILING
	LDFLAGS+=-pg
endif
# }}}

all: $(TARGET) $(TARGET).stripped
	ls -l $^

clean:
	$(RM) $(TARGET) $(TARGET).stripped $(OBJECTS)

.PHONY: clean all

%.o: $(HEADERS)

$(TARGET): $(OBJECTS) $(TARGET).o
	$(LD) -o $@ $^ $(LDFLAGS)

%.stripped: %
	$(STRIP) -s -o $@ $^
