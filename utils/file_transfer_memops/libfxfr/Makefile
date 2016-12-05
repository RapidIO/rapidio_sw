# Copyright (c) 2015, Integrated Device Technology
#

TOPDIR=../../..
include $(TOPDIR)/rules.mk

NAME:=fxfr
TARGETS:=lib$(NAME).a
HEADERS:=inc/lib$(NAME).h

CC=$(CXX)

OBJECTS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c))

all: $(TARGETS)

%.a: $(OBJECTS)
	$(AR) rcs $@ $^

src/%.o: src/%.c $(HEADERS)
	$(CXX) -c $(CFLAGS) $< -o $@ \
	-I$(FMDDIR)/librio_switch/inc -I$(FMDDIR)/libdid/inc -I$(FMDDIR)/libct/inc

clean:
	rm -f $(TARGETS) $(OBJECTS) inc/*~ src/*~ *~ *.a

.PHONY: all clean
