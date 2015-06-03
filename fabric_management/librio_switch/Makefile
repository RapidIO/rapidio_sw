# Copyright (c) 2014, Prodrive Technologies
# Copyright (c) 2014, Integrated Device Technology
#
# SPDX-License-Identifier: BSD-3-Clause

NAME:=switch_api
TARGETS:=lib$(NAME).a
HEADERS:=inc

CROSS_COMPILE?=/usr/bin/
INCDIR?=/usr/include/

CC:=$(CROSS_COMPILE)gcc
AR:=$(CROSS_COMPILE)ar

CFLAGS+=$(addprefix -W,all extra error) -fPIC
CFLAGS+=-O2 -DNDEBUG -g -DIDT_TSI721_SUPPORT
CFLAGS+=$(addprefix -I,inc $(INCDIR))
LDFLAGS+=-L. -l$(NAME)

OBJECTS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c)) 

all: $(TARGETS)

%.a: $(OBJECTS)
	$(AR) rcs $@ $^

src/%.o: src/%.c $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS) $(OBJECTS)

.PHONY: all clean
