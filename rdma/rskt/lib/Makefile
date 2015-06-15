# Copyright (c) 2015, Integrated Device Technology
#

NAME:=rskt
TARGETS:=lib$(NAME).a
HEADERS:=inc/lib$(NAME).h inc/lib$(NAME)_private.h src/*.h

CROSS_COMPILE?=/usr/bin/
INCDIR=$(LINUX_INCLUDE_DIR)

CXX=$(CROSS_COMPILE)g++
CC=$(CROSS_COMPILE)g++
AR=$(CROSS_COMPILE)ar

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
	$(CXX) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS) $(OBJECTS) inc/*~ src/*~ *~ *.a

.PHONY: all clean
