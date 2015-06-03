# Copyright (c) 2015, Integrated Device Technology
#

NAME:=rio_fmd
TARGETS:=lib$(NAME).a
HEADERS:=inc/lib$(NAME).h

CROSS_COMPILE?=/usr/bin/
INCDIR?=/usr/src/linux/include

CC:=$(CROSS_COMPILE)gcc
AR:=$(CROSS_COMPILE)ar

CFLAGS+=$(addprefix -W,all extra error) -fPIC
CFLAGS+=-O2 -DNDEBUG -g
CFLAGS+=$(addprefix -I,inc $(INCDIR))
CFLAGS+=$(addprefix -I,inc $(LINUX_INCLUDE_DIR))
CFLAGS+=-I../cli
CFLAGS+=-I../librio_switch/inc
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
