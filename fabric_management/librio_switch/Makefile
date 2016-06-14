# Copyright (c) 2014, Prodrive Technologies
# Copyright (c) 2014, Integrated Device Technology
#
# SPDX-License-Identifier: BSD-3-Clause

NAME:=switch_api
TARGETS:=lib$(NAME).a
HEADERS:=inc

LOG_LEVEL?=4
DEBUG?=DEBUG
TOP_LEVEL = $(shell pwd)

CROSS_COMPILE ?= /usr/bin/

CXX ?= $(CROSS_COMPILE)g++
AR ?= $(CROSS_COMPILE)ar

CFLAGS+=$(addprefix -W,all extra error) -fPIC
CFLAGS+=-O2 -DNDEBUG -g -DIDT_TSI721_SUPPORT
CFLAGS+=-I./inc -I../inc
LDFLAGS+=-L. -l$(NAME)

OBJECTS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c)) 

all: $(TARGETS)

%.a: $(OBJECTS)
	$(AR) rcs $@ $^

src/%.o: src/%.c $(HEADERS)
	$(CXX) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS) $(OBJECTS)

.PHONY: all clean
