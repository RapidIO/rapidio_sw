# Copyright (c) 2014, Prodrive Technologies
# Copyright (c) 2014, Integrated Device Technology
#
# SPDX-License-Identifier: BSD-3-Clause

TOPDIR?=$(shell pwd)/../..
include $(TOPDIR)/rules.mk

NAME:=switch_api
TARGETS:=lib$(NAME).a
HEADERS:=inc
TEST_TARGETS:=$(NAME)_test

LOG_LEVEL?=4
DEBUG?=DEBUG

CFLAGS+=$(addprefix -W,all extra) -fPIC
CFLAGS+=-DNDEBUG -DIDT_TSI721_SUPPORT
CFLAGS+=-I./inc
CFLAGS+=-I$(FMDDIR)/librio_switch/inc
LDFLAGS+= -Wl,--wrap=DARRegRead -Wl,--wrap=DARRegWrite
LDFLAGS+=-L. -L$(COMMONLIB) -L$(FMDDIR)/libs_a $(LIBS_RPATH) -l$(NAME)
LDFLAGS+=-lcmocka


OBJECTS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c)) 
TEST_OBJECTS:=$(patsubst test/%.c,test/%.o,$(wildcard test/*.c))

.PHONY: all clean test


ifdef TEST
all: $(TARGETS) $(TEST_TARGETS)
else
all: $(TARGETS)
endif

test: $(TEST_TARGETS)

%.a: $(OBJECTS)
	@echo ---------- Building $@
	$(AR) rcs $@ $^

src/%.o: src/%.c
	@echo ---------- Building $@
	$(CXX) -c $(CFLAGS) $< -o $@ \
        $(TST_INCS)

test/%.o: test/%.c
	@echo ---------- Building $@
	$(CXX) -c $(CFLAGS) $< -o $@ \
	$(TST_INCS)

$(TEST_TARGETS): $(TEST_OBJECTS)
	@echo ---------- Building $@
	$(CXX) -o $@ $< $(LDFLAGS) \
	$(TST_LIBS)

clean:
	rm -f $(TARGETS) $(OBJECTS) \
        $(TEST_TARGETS) $(TEST_OBJECTS) \
	inc/*~ src/*~ test/*~ *~


