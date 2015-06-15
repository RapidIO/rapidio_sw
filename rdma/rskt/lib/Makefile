# Copyright (c) 2015, Integrated Device Technology
#

NAME:=rskt
TARGETS:=lib$(NAME).a
HEADERS:=inc/lib$(NAME).h inc/lib$(NAME)_private.h src/*.h

CROSS_COMPILE?=/usr/bin/
#INCDIR?=/usr/src/linux/include
INCDIR=$(LINUX_INCLUDE_DIR)

CC:=$(CROSS_COMPILE)gcc
AR:=$(CROSS_COMPILE)ar

CFLAGS+=$(addprefix -W,all extra error) -fPIC
CFLAGS+=-O2 -DDEBUG -g
CFLAGS+=-I../inc -I../../../include 
CFLAGS+=-I../../../common/include
CFLAGS+=-DRDMA_LL=7
CFLAGS+=$(addprefix -I,inc $(INCDIR))
LDFLAGS+=-L. -l$(NAME)

OBJECTS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c))

all: $(TARGETS)

%.a: $(OBJECTS)
	$(AR) rcs $@ $^

src/%.o: src/%.c $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS) $(OBJECTS) inc/*~ src/*~ *~ *.a

.PHONY: all clean
