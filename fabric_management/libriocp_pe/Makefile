# Copyright (c) 2014, Prodrive Technologies
#
# SPDX-License-Identifier: BSD-3-Clause

TOPDIR?=$(shell pwd)/../..
include $(TOPDIR)/rules.mk

NAME:=riocp_pe
TARGETS:=lib$(NAME).a
HEADERS:=inc/lib$(NAME).h

LOG_LEVEL?=4
DEBUG?=DEBUG

level?=$(LOG_LEVEL)

CC=$(CXX) # XXX very UGGLY switcheroo

CFLAGS+=$(addprefix -W,all extra) -fPIC
CFLAGS+=-DNDEBUG
CFLAGS+=-I. -I./inc -I./../libdid/inc
CFLAGS+=-I./src
CFLAGS+=-DRDMA_LL=$(level) -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS
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
