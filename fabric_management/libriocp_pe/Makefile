# Copyright (c) 2014, Prodrive Technologies
#
# SPDX-License-Identifier: BSD-3-Clause

NAME:=riocp_pe
TARGETS:=lib$(NAME).a
HEADERS:=inc/lib$(NAME).h

CROSS_COMPILE?=/usr/bin/
KDIR=/usr/src/linux
LINUX_INCLUDE_DIR ?= $(KDIR)/include
LOG_LEVEL?=4
DEBUG?=DEBUG
TOP_LEVEL?=../..

level?=$(LOG_LEVEL)

CXX = $(CROSS_COMPILE)g++
## Dirty kludge to enforce use of g++ for builtin compile rule
CC = $(CROSS_COMPILE)g++
AR = $(CROSS_COMPILE)ar

CFLAGS+=$(addprefix -W,all extra error) -fPIC
CFLAGS+=-O2 -DNDEBUG -g
CFLAGS+= -I $(LINUX_INCLUDE_DIR)
CFLAGS+=-I. -I./inc
CFLAGS+=-I./src
CFLAGS+=-I$(TOP_LEVEL)/include
CFLAGS+=-I../../common/include
CFLAGS+=-DRDMA_LL=$(level) -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS
LDFLAGS+=-L. -l$(NAME)

OBJECTS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c)) $(patsubst src/switches/%.c,src/switches/%.o,$(wildcard src/switches/*.c))

src/%.o: src/%.c $(HEADERS)
	echo "real rule"
	g++ -c $(CFLAGS) $< -o $@

src/switches/%.o: src/switches/%.c $(HEADERS)
	echo "real rule"
	g++ -c $(CFLAGS) $< -o $@

%.a: $(OBJECTS)
	$(AR) rcs $@ $^

all: $(TARGETS)

clean:
	rm -f $(TARGETS) $(OBJECTS)

.PHONY: all clean
