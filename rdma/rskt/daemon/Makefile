# Copyright (c) 2015, Integrated Device Technology
#
# Default configurations

KDIR ?= /usr/src/linux

LINUX_INCLUDE_DIR ?= $(KDIR)/include

LOG_LEVEL ?= 6

DEBUG ?= DEBUG

TOP_LEVEL ?= ../../..

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb

CROSS_COMPILE?=/usr/bin/
LINUX_INCLUDE_DIR?=/usr/src/linux/include
INCDIR=$(LINUX_INCLUDE_DIR)

COMMON_DIR?=$(TOP_LEVEL)/common
MPORT_DIR?=$(TOP_LEVEL)/libmport

CC:=$(CROSS_COMPILE)gcc
AR:=$(CROSS_COMPILE)ar

DAEMON_OBJS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c))

src/%.o: src/%.c
	g++ -o $@ $< -c -g -ggdb -fPIC -Wall -Werror -O2 \
	-I./src -I./inc -I../inc \
	-I$(COMMON_DIR)/include \
	-I$(COMMON_DIR)/liblist/inc \
	-I../../../include \
	-I$(LINUX_INCLUDE_DIR)

RSKTD_SOURCES = src/librsktd_cli.c src/librsktd_dmn.c src/librsktd_lib.c \
	src/librsktd_msg_proc.c src/librsktd_sn.c src/librsktd_speer.c  \
	src/librsktd_wpeer.c src/rsktd.c

RSKTD_OBJECTS = src/librsktd_cli.o src/librsktd_dmn.o src/librsktd_lib.o \
	src/librsktd_msg_proc.o src/librsktd_sn.o src/librsktd_speer.o  \
	src/librsktd_wpeer.o src/rsktd.o

# The RDMA Sockets Daemon
rsktd:  $(RSKTD_OBJECTS) src/*.h
	@echo ---------- Building rsktd...
	g++ -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(RSKTD_OBJECTS) \
	-L$(MPORT_DIR)/ -lmport \
	-L$(COMMON_DIR)/liblog -llog \
	-L$(COMMON_DIR)/libcli -lcli \
	-L$(COMMON_DIR)/liblist -llist \
	$(SO_LIB_LKFLAGS)


all: rsktd

clean:
	rm -f *.o src/*.o *~ *.exe core rsktd remdbg rskts
