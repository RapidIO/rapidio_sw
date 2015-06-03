# Copyright (c) 2014, Prodrive Technologies
#
# SPDX-License-Identifier: BSD-3-Clause

NAME:=riocp_pe
TARGETS:=lib$(NAME).a
HEADERS:=inc/lib$(NAME).h

CROSS_COMPILE?=/usr/bin/
INCDIR?=/usr/src/linux/include

CC:=$(CROSS_COMPILE)gcc
AR:=$(CROSS_COMPILE)ar

CFLAGS+=$(addprefix -W,all extra error) -fPIC
CFLAGS+=-O2 -DNDEBUG -g
CFLAGS+=$(addprefix -I,inc $(INCDIR))
CFLAGS+= -I $(LINUX_INCLUDE_DIR)
CFLAGS+=-I.
CFLAGS+=-I../Linux_Include
CFLAGS+=-I../librio_maint/inc
CFLAGS+=-I./src
LDFLAGS+=-L. -l$(NAME)

OBJECTS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c)) $(patsubst src/switches/%.c,src/switches/%.o,$(wildcard src/switches/*.c))

all: $(TARGETS)

%.a: $(OBJECTS)
	$(AR) rcs $@ $^

src/%.o: src/%.c $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

src/switches/%.o: src/switches/%.c $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS) $(OBJECTS)

.PHONY: all clean
