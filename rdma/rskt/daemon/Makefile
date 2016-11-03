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

TOPDIR?=$(shell pwd)/../../..
include $(TOPDIR)/rules.mk

LOG_LEVEL?=7
DEBUG?=DEBUG

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC
A_LIB_LKFLAGS = -lnsl -fPIC

RSKT_DIR=../lib
INCLUDE_DIR?=$(TOPDIR)/include

LIB_DIRS?=-L$(COMMONLIBA) -L$(COMMONLIB) \
	-L$(FMDDIR)/libriocp_pe \
	-L$(FMDDIR)/libdd \
	-L$(FMDDIR)/libfmdd \
	-L$(TOPDIR)/rdma

DAEMON_OBJS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c))

src/%.o: src/%.c
	$(CXX) $(CXXFLAGS) -o $@ -c $< -Wall \
	-I./src -I./inc -I../inc \
	-I$(FMDDIR)/libdid/inc \
	-I$(FMDDIR)/libct/inc \
	-I$(COMMONDIR)/liblist/inc \
	-I../../lib/src \
	-I$(TOPDIR)/rdma/inc \
	-DRDMA_LL=$(LOG_LEVEL)

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

REMDBG_SOURCE = src/remdbg.c
REMDBG_OBJECTS = src/remdbg.o

LIBRARIES = -lrskt -lrdma -lriocp_pe -lmport \
	-llist -llog -lfmdd -ldd -lcli -lrt -ldl 

all: rsktd rskts remdbg 

# The RDMA Sockets Daemon
rsktd:  $(RSKTLIB_OBJECTS) $(RSKTD_OBJECTS) src/*.h
	@echo ---------- Building rsktd...
	$(CXX) $(CXXFLAGS) $(LIBS_RPATH) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKTD_OBJECTS) \
	$(LIB_DIRS) $(LIBRARIES) $(TST_LIBS)

# The RDMA Sockets Test Server
rskts:  $(RSKTLIB_OBJECTS) $(RSKTS_OBJECTS) src/*.h
	@echo ---------- Building rskt test server...
	$(CXX) $(CXXFLAGS) -L$(COMMONLIB) \
	$(LIBS_RPATH) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKTS_OBJECTS) \
	$(LIB_DIRS) $(LIBRARIES) $(TST_LIBS)

# The Remote Debug shellsrc/librsktd_cli.c 
remdbg:  $(RSKTLIB_OBJECTS) $(REMDBG_OBJECTS) src/*.h
	@echo ---------- Building remote debug CLI ...
	$(CXX) $(CXXFLAGS) -L$(COMMONLIB) \
	$(LIBS_RPATH) \
	-o $@ $(RSKTLIB_OBJECTS) $(REMDBG_OBJECTS) \
	$(LIB_DIRS) $(LIBRARIES) $(TST_LIBS)

clean:
	rm -f *.o src/*.o *~ *.exe core rsktd remdbg rskts rskt_server rskt_client
