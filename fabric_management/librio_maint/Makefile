# Copyright (c) 2014, Prodrive Technologies
#
# SPDX-License-Identifier: BSD-3-Clause

NAME:=rio_maint
TARGETS:=lib$(NAME).a $(NAME)
HEADERS:=inc/lib$(NAME).h

KDIR=/usr/src/linux

LINUX_INCLUDE_DIR ?= $(KDIR)/include

LOG_LEVEL?=6

DEBUG?=DEBUG

TOP_LEVEL = $(shell pwd)

CROSS_COMPILE?=/usr/bin/

CXX ?= $(CROSS_COMPILE)g++
AR?=$(CROSS_COMPILE)ar

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
	$(CXX) $(CFLAGS) -o $@ tests/*.c $(LDFLAGS)

src/%.o: src/%.c $(HEADERS)
	$(CXX) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS) $(OBJECTS)

.PHONY: all clean
