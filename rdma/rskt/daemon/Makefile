# Copyright (c) 2015, Integrated Device Technology
#
# Default configurations

LOG_LEVEL ?= 1

DEBUG ?= DEBUG

TOP_LEVEL ?= ../../..

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb
A_LIB_LKFLAGS = -lnsl -fPIC -g -ggdb

CROSS_COMPILE?=/usr/bin/

COMMON_DIR?=$(TOP_LEVEL)/common
MPORT_DIR?=$(TOP_LEVEL)/libmport
RDMA_DIR?=$(TOP_LEVEL)/rdma
RSKT_DIR?=../lib
INCLUDE_DIR?=$(TOP_LEVEL)/include

LIB_DIRS?=-L$(COMMON_DIR)/libs_a -L$(INCLUDE_DIR)/libs_a \
	-L$(COMMON_DIR)/libs_so -L$(TOP_LEVEL)/fabric_management/libriocp_pe \
	-L$(TOP_LEVEL)/fabric_management/libdd \
	-L$(MPORT_DIR)

CXX ?= $(CROSS_COMPILE)g++
AR ?= $(CROSS_COMPILE)ar

DAEMON_OBJS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c))

src/%.o: src/%.c
	$(CXX) -o $@ $< -c -g -ggdb -fPIC -Wall -Werror -O2 \
	-I./src -I./inc -I../inc \
	-I$(COMMON_DIR)/include \
	-I$(COMMON_DIR)/liblist/inc \
	-I../../../include \
	-DRDMA_LL=7

RSKTLIB_SOURCES = src/librsktd_dmn.c src/librsktd_lib.c \
	src/librsktd_msg_proc.c src/librsktd_sn.c src/librsktd_speer.c  \
	src/librsktd_wpeer.c

RSKTLIB_OBJECTS = src/librsktd_dmn.o src/librsktd_lib.o \
	src/librsktd_msg_proc.o src/librsktd_sn.o src/librsktd_speer.o  \
	src/librsktd_wpeer.o src/librsktd_fm.o

RSKTD_SOURCE = src/rsktd.c src/librsktd_cli.c 
RSKTD_OBJECTS = src/rsktd.o src/librsktd_cli.o 

RSKTS_SOURCE = src/rskts.c
RSKTS_OBJECTS = src/rskts.o

RSKTS_TEST_SOURCE = src/rskts_test.cpp
RSKTS_TEST_OBJECTS = src/rskts_test.o

RSKTC_TEST_SOURCE = src/rsktc_test.cpp
RSKTC_TEST_OBJECTS = src/rsktc_test.o

REMDBG_SOURCE = src/remdbg.c
REMDBG_OBJECTS = src/remdbg.o

all: rsktd rskts remdbg rskts_test rsktc_test

# The RDMA Sockets Daemon
rsktd:  $(RSKTLIB_OBJECTS) $(RSKTD_OBJECTS) src/*.h
	@echo ---------- Building rsktd...
	$(CXX) -pthread -Wl,-rpath=$(MPORT_DIR) \
	-L$(MPORT_DIR) \
	-Wl,-rpath=$(RDMA_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKTD_OBJECTS) $(LIB_DIRS) \
	-lrskt -lcli -lrt -lmport \
	-llist -llog -lrdma -lfmdd -ldd -lriocp_pe

# The RDMA Sockets Test Server
rskts:  $(RSKTLIB_OBJECTS) $(RSKTS_OBJECTS) src/*.h
	@echo ---------- Building rskt test server...
	$(CXX) -L$(MPORT_DIR) -L$(shell pwd) -pthread \
	-Wl,-rpath=$(MPORT_DIR) \
	-Wl,-rpath=$(RDMA_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKTS_OBJECTS) $(LIB_DIRS) \
	-lrskt -lcli -lrt -lmport \
	-llist -llog -lrdma -lfmdd -ldd -lriocp_pe

rskts_test:  $(RSKTLIB_OBJECTS) $(RSKTS_TEST_SOURCE) src/*.h
	@echo ---------- Building rskts_test
	$(CXX) --std=gnu++0x -L$(MPORT_DIR) -L$(shell pwd) -pthread \
	-I$(COMMON_DIR)/include \
	-I./src -I./inc -I../inc \
	-I$(COMMON_DIR)/include \
	-I$(TOP_LEVEL)/include \
	-DRDMA_LL=7 \
	-Wl,-rpath=$(MPORT_DIR) \
	-Wl,-rpath=$(RDMA_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKTS_TEST_SOURCE) $(LIB_DIRS) \
	-lrskt -lcli -lrt -lmport \
	-llist -llog -lrdma -lfmdd -ldd -lriocp_pe

rsktc_test:  $(RSKTLIB_OBJECTS) $(RSKTC_TEST_OBJECTS) src/*.h
	@echo ---------- Building rsktc_test
	$(CXX) -L$(MPORT_DIR) -L$(shell pwd) -pthread \
	-Wl,-rpath=$(MPORT_DIR) \
	-Wl,-rpath=$(RDMA_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKTC_TEST_OBJECTS) $(LIB_DIRS) \
	-lrskt -lcli -lrt -lmport \
	-llist -llog -lrdma -lfmdd -ldd -lriocp_pe


# The Remote Debug shellsrc/librsktd_cli.c 
remdbg:  $(RSKTLIB_OBJECTS) $(REMDBG_OBJECTS) src/*.h
	@echo ---------- Building remote debug CLI ...
	$(CXX) -L$(shell pwd) -L$(MPORT_DIR) \
	-pthread -Wl,-rpath=$(MPORT_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(REMDBG_OBJECTS) $(LIB_DIRS) \
	-lrskt -lcli -lrt -lmport \
	-llist -llog -lrdma -lfmdd -ldd -lriocp_pe

clean:
	rm -f *.o src/*.o *~ *.exe core rsktd remdbg rskts rskts_test rsktc_test
