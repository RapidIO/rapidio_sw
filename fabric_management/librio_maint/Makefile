# Copyright (c) 2014, Prodrive Technologies
#
# SPDX-License-Identifier: BSD-3-Clause

NAME:=rio_maint
TARGETS:=lib$(NAME).a $(NAME)
HEADERS:=inc/lib$(NAME).h

CROSS_COMPILE?=/usr/bin/
INCDIR?=/usr/src/linux/include

CC:=$(CROSS_COMPILE)gcc
AR:=$(CROSS_COMPILE)ar

CFLAGS+=$(addprefix -W,all extra error) -fPIC
CFLAGS+=-O2 -DNDEBUG -g
CFLAGS+=$(addprefix -I,inc $(INCDIR))
CFLAGS+= -I $(LINUX_INCLUDE_DIR)
LDFLAGS+=-L. -l$(NAME)

OBJECTS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c))

all: $(TARGETS)

%.a: $(OBJECTS)
	$(AR) rcs $@ $^

$(NAME): lib$(NAME).a
	$(CC) $(CFLAGS) -o $@ tests/*.c $(LDFLAGS)

src/%.o: src/%.c $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS) $(OBJECTS)

.PHONY: all clean
