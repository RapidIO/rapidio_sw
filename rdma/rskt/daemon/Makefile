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
A_LIB_LKFLAGS = -lnsl -fPIC -g -ggdb

CROSS_COMPILE?=/usr/bin/
LINUX_INCLUDE_DIR?=/usr/src/linux/include
INCDIR=$(LINUX_INCLUDE_DIR)

COMMON_DIR?=$(TOP_LEVEL)/common
MPORT_DIR?=$(TOP_LEVEL)/libmport
RDMA_DIR?=$(TOP_LEVEL)/rdma
RSKT_DIR?=../lib

LIB_DIRS?=-L$(COMMON_DIR)/libs_a -L$(COMMON_DIR)/libs_so \
	-L$(RSKT_DIR) -L$(RDMA_DIR) -L$(MPORT_DIR) 
	

CC:=$(CROSS_COMPILE)gcc
AR:=$(CROSS_COMPILE)ar

DAEMON_OBJS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c))

src/%.o: src/%.c
	$(CXX) -o $@ $< -c -g -ggdb -fPIC -Wall -Werror -O2 \
	-I./src -I./inc -I../inc \
	-I$(COMMON_DIR)/include \
	-I$(COMMON_DIR)/liblist/inc \
	-I../../../include \
	-I$(LINUX_INCLUDE_DIR)

RSKTLIB_SOURCES = src/librsktd_dmn.c src/librsktd_lib.c \
	src/librsktd_msg_proc.c src/librsktd_sn.c src/librsktd_speer.c  \
	src/librsktd_wpeer.c

RSKTLIB_OBJECTS = src/librsktd_dmn.o src/librsktd_lib.o \
	src/librsktd_msg_proc.o src/librsktd_sn.o src/librsktd_speer.o  \
	src/librsktd_wpeer.o 

RSKTD_SOURCE = src/rsktd.c src/librsktd_cli.c 
RSKTD_OBJECTS = src/rsktd.o src/librsktd_cli.o 

RSKTS_SOURCE = src/rskts.c
RSKTS_OBJECTS = src/rskts.o

REMDBG_SOURCE = src/remdbg.c
REMDBG_OBJECTS = src/remdbg.o

all: rsktd rskts remdbg

# The RDMA Sockets Daemon
rsktd:  $(RSKTLIB_OBJECTS) $(RSKTD_OBJECTS) src/*.h
	@echo ---------- Building rsktd...
	$(CXX) -pthread -Wl,-rpath=$(MPORT_DIR) \
	-Wl,-rpath=$(RDMA_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKTD_OBJECTS) $(LIB_DIRS) \
	-lrskt -lcli -lrt -lmport \
	-llist -llog -lrdma 

# The RDMA Sockets Test Server
rskts:  $(RSKTLIB_OBJECTS) $(RSKTS_OBJECTS) src/*.h
	@echo ---------- Building rskt test server...
	$(CXX) -L$(shell pwd) -pthread -Wl,-rpath=$(MPORT_DIR) \
	-Wl,-rpath=$(RDMA_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKTS_OBJECTS) $(LIB_DIRS) \
	-lrskt -lcli -lrt -lmport \
	-llist -llog -lrdma 

# The Remote Debug shellsrc/librsktd_cli.c 
remdbg:  $(RSKTLIB_OBJECTS) $(REMDBG_OBJECTS) src/*.h
	@echo ---------- Building rskt test server...
	$(CXX) -L$(shell pwd) -pthread -Wl,-rpath=$(MPORT_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(REMDBG_OBJECTS) $(LIB_DIRS) \
	-lrskt -lcli -lrt -lmport \
	-llist -llog -lrdma 

clean:
	rm -f *.o src/*.o *~ *.exe core rsktd remdbg rskts
