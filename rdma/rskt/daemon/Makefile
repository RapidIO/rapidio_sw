#
# Copyright (c) 2015, Integrated Device Technology
#
# Default configurations

#----------------------------------------------------------------------------*
#Copyright (c) 2015, Integrated Device Technology Inc.
#Copyright (c) 2015, RapidIO Trade Association
#All rights reserved.
#
###Redistribution and use in source and binary forms, with or without modification,
#are permitted provided that the following conditions are met:
#
###1. Redistributions of source code must retain the above copyright notice, this
#list of conditions and the following disclaimer.
#
###2. Redistributions in binary form must reproduce the above copyright notice,
#this list of conditions and the following disclaimer in the documentation
#and/or other materials provided with the distribution.
#
###3. Neither the name of the copyright holder nor the names of its contributors
#may be used to endorse or promote products derived from this software without
#specific prior written permission.
#
###THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
#ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
#FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#--------------------------------------------------------------------------***

LOG_LEVEL?=7
DEBUG?=DEBUG
CC?= $(CROSS_COMPILE)g++
CXX?= $(CROSS_COMPILE)g++

TOP_LEVEL ?= ../../..

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb
A_LIB_LKFLAGS = -lnsl -fPIC -g -ggdb

CROSS_COMPILE?=/usr/bin/

COMMON_DIR?=$(TOP_LEVEL)/common
MPORT_DIR?=$(COMMON_DIR)/libmport
RDMA_DIR?=$(TOP_LEVEL)/rdma
RSKT_DIR?=../lib
INCLUDE_DIR?=$(TOP_LEVEL)/include

LIB_DIRS?=-L$(INCLUDE_DIR)/libs_a \
	-L$(COMMON_DIR)/libs_so -L$(TOP_LEVEL)/fabric_management/libriocp_pe \
	-L$(TOP_LEVEL)/fabric_management/libdd \
	#-L$(MPORT_DIR)

LIBS_RPATH?=-Wl,-rpath=$(COMMON_DIR)/libs_so

AR ?= $(CROSS_COMPILE)ar

DAEMON_OBJS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c))

src/%.o: src/%.c
	$(CXX) --std=c++11 -o $@ $< -c -g -ggdb -fPIC -Wall -Werror -O2 \
	-I./src -I./inc -I../inc \
	-I$(COMMON_DIR)/include \
	-I$(COMMON_DIR)/liblist/inc \
	-I../../../include \
	-I../../lib/src \
	-I../../../rdma/inc \
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

RSKT_SERVER_SOURCE = src/rskt_server.c
RSKT_SERVER_OBJECTS = src/rskt_server.o

RSKT_CLIENT_SOURCE = src/rskt_client.c
RSKT_CLIENT_OBJECTS = src/rskt_client.o

REMDBG_SOURCE = src/remdbg.c
REMDBG_OBJECTS = src/remdbg.o

all: rsktd rskts remdbg rskt_client rskt_server

# The RDMA Sockets Daemon
rsktd:  $(RSKTLIB_OBJECTS) $(RSKTD_OBJECTS) src/*.h
	@echo ---------- Building rsktd...
	$(CXX) -pthread $(LIBS_RPATH) \
	-Wl,-rpath=$(RDMA_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKTD_OBJECTS) $(LIB_DIRS) \
	-lrskt -lcli -lrt -ldl -lmport \
	-llist -llog -lrdma -lfmdd -ldd -lriocp_pe

# The RDMA Sockets Test Server
rskts:  $(RSKTLIB_OBJECTS) $(RSKTS_OBJECTS) src/*.h
	@echo ---------- Building rskt test server...
	$(CXX) -L$(MPORT_DIR) -L$(shell pwd) -pthread \
	$(LIBS_RPATH) \
	-Wl,-rpath=$(RDMA_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKTS_OBJECTS) $(LIB_DIRS) \
	-lrskt -lcli -lrt -ldl -lmport \
	-llist -llog -lrdma -lfmdd -ldd -lriocp_pe

rskt_server:  $(RSKTLIB_OBJECTS) $(RSKT_SERVER_SOURCE) src/*.h ../inc/*.h $(COMMON_DIR)/include/*.h
	@echo ---------- Building rskt_server
	$(CXX) --std=gnu++0x -L$(MPORT_DIR) -L$(shell pwd) -pthread \
	-I$(COMMON_DIR)/include \
	-I./src -I./inc -I../inc \
	-I$(COMMON_DIR)/include \
	-I$(TOP_LEVEL)/include \
	-DRDMA_LL=7 \
	$(LIBS_RPATH) \
	-Wl,-rpath=$(RDMA_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKT_SERVER_SOURCE) $(LIB_DIRS) \
	-lrskt -lcli -lrt -ldl -lmport \
	-llist -llog -lrdma -lfmdd -ldd -lriocp_pe

rskt_client:  $(RSKTLIB_OBJECTS) $(RSKT_CLIENT_SOURCE) src/*.h ../inc/*.h $(COMMON_DIR)/include/*.h
	@echo ---------- Building rskt_client
	$(CXX) --std=gnu++0x -L$(MPORT_DIR) -L$(shell pwd) -pthread \
	-I$(COMMON_DIR)/include \
	-I./src -I./inc -I../inc \
	-I$(COMMON_DIR)/include \
	-I$(TOP_LEVEL)/include \
	-DRDMA_LL=7 \
	$(LIBS_RPATH) \
	-Wl,-rpath=$(RDMA_DIR) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKT_CLIENT_SOURCE) $(LIB_DIRS) \
	-lrskt -lcli -lrt -ldl -lmport \
	-llist -llog -lrdma -lfmdd -ldd -lriocp_pe

# The Remote Debug shellsrc/librsktd_cli.c 
remdbg:  $(RSKTLIB_OBJECTS) $(REMDBG_OBJECTS) src/*.h
	@echo ---------- Building remote debug CLI ...
	$(CXX) -L$(shell pwd) -L$(MPORT_DIR) \
	-pthread $(LIBS_RPATH) \
	-o $@ $(RSKTLIB_OBJECTS) $(REMDBG_OBJECTS) $(LIB_DIRS) \
	-lrskt -lcli -lrt -ldl -lmport \
	-llist -llog -lrdma -lfmdd -ldd -lriocp_pe

clean:
	rm -f *.o src/*.o *~ *.exe core rsktd remdbg rskts rskt_server rskt_client
