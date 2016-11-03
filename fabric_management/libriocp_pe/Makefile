# Copyright (c) 2014, Prodrive Technologies
#
# SPDX-License-Identifier: BSD-3-Clause

TOPDIR?=$(shell pwd)/../..
include $(TOPDIR)/rules.mk

NAME:=riocp_pe
TARGETS:=lib$(NAME).a
HEADERS:=inc/lib$(NAME).h

CC=$(CXX) # XXX very UGGLY switcheroo

CFLAGS+=$(addprefix -W,all extra) -fPIC
CFLAGS+=-D$(DEBUG_CTL) -DRDMA_LL=$(LOG_LEVEL)
CFLAGS+=-I../inc
CFLAGS+=-I./inc -I./src
CFLAGS+=-I../libct/inc -I../libdid/inc

LDFLAGS+=-L. -l$(NAME)

OBJECTS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c)) 

src/%.o: src/%.c $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

%.a: $(OBJECTS)
	$(AR) rcs $@ $^

all: $(TARGETS)

clean:
	rm -f $(TARGETS) $(OBJECTS)

.PHONY: all clean
