# Copyright (c) 2015, Integrated Device Technology
#

TOPDIR=../../..
include $(TOPDIR)/rules.mk

NAME:=fxfr
TARGETS:=lib$(NAME).a
HEADERS:=inc/lib$(NAME).h

CFLAGS+=-fPIC -DNDEBUG -I./inc
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
