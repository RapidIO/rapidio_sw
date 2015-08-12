# Copyright (c) 2015, Integrated Device Technology
#

NAME:=fxfr
TARGETS:=lib$(NAME).a
HEADERS:=inc/lib$(NAME).h

CROSS_COMPILE?=/usr/bin/
COMMON_INCLUDE_DIR?=../../../common/include

CC:=$(CROSS_COMPILE)gcc
AR:=$(CROSS_COMPILE)ar

CFLAGS+=-fPIC
CFLAGS+=-O2 -DNDEBUG -g -I./inc
CFLAGS+= -I$(COMMON_INCLUDE_DIR)
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
