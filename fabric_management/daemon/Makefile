# Copyright (c) 2015, Integrated Device Technology
#
# Default configurations

LOG_LEVEL ?= 1

DEBUG ?= DEBUG

TOP_LEVEL ?= ../..

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb
A_LIB_LKFLAGS = -lnsl -fPIC -g -ggdb

CROSS_COMPILE?=/usr/bin/

COMMON_DIR=$(TOP_LEVEL)/common/
MPORT_DIR=$(TOP_LEVEL)/libmport
DAEMON=./

LOCAL_LIBRARY_DIR ?= "libs_a"

LIB_DIRS?=-L$(COMMON_DIR)libs_a -L$(COMMON_DIR)libs_so \
	-L../$(LOCAL_LIBRARY_DIR) -L$(MPORT_DIR) 
	

CXX ?= $(CROSS_COMPILE)g++
AR ?= $(CROSS_COMPILE)ar

src/%.o: src/%.c
	$(CXX) -o $@ $< -c -g -ggdb -fPIC -Wall -Werror -O2 \
	-I./src -I./inc -I../inc \
	-I$(COMMON_DIR)/include \
	-I$(TOP_LEVEL)/include \
	-I../ \
	-I../librio_switch/inc \
	-I../libriocp_pe/inc \
	-I../cli \
	-DRDMA_LL=7

FMD_SOURCES = src/fmd_app_mgmt.c src/fmd_cfg.c src/fmd.c src/fmd_cfg_cli.c \
	src/fmd_mgmt_master.c src/fmd_mgmt_slave.c src/fmd_mgmt_cli.c \
	src/fmd_dev_rw_cli.c 

FMD_OBJECTS = src/fmd_app_mgmt.o src/fmd_cfg.o src/fmd.o src/fmd_cfg_cli.o \
	src/fmd_mgmt_master.o src/fmd_mgmt_slave.o src/fmd_mgmt_cli.o \
	src/fmd_dev_rw_cli.o 
all: fmd

# The Fabric Management Daemon
fmd:  $(FMD_OBJECTS) $(FMD_SOURCES) src/*.h 
	@echo ---------- Building fmd...
	$(CXX) -pthread \
	-o $@ $(FMD_OBJECTS) $(LIB_DIRS) \
	-lcli -ldd -llist -lrt -lmport -llog -lriocp_pe \
	-lswitch_api

clean:
	rm -f *.o src/*.o *~ *.exe core fmd
