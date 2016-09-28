#------------------------------------------------------------------------------*
#Copyright (c) 2015, Integrated Device Technology Inc.
#Copyright (c) 2015, RapidIO Trade Association
#All rights reserved.
#
#Redistribution and use in source and binary forms, with or without modification,
#are permitted provided that the following conditions are met:
#
#1. Redistributions of source code must retain the above copyright notice, this
#list of conditions and the following disclaimer.
#
#2. Redistributions in binary form must reproduce the above copyright notice,
#this list of conditions and the following disclaimer in the documentation
#and/or other materials provided with the distribution.
#
#3. Neither the name of the copyright holder nor the names of its contributors
#may be used to endorse or promote products derived from this software without
#specific prior written permission.
#
#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
#ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
#FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#------------------------------------------------------------------------------*

TOPDIR?=$(shell pwd)/..
include $(TOPDIR)/rules.mk

# LOG_LEVEL is set by the master Makefile. If this Makefile is run alone, 
# it can override the loglevel on the command line by specifying 'level=XXX'
# otherwise it defaults to 7
ifndef LOG_LEVEL
level?=7
else
level?=$(LOG_LEVEL)
endif

INCLUDE_DIR?=$(TOPDIR)/include
MPORTLIB_PATH=$(TOPDIR)/common/libs_a
MPORTLIB=libmport.a
MPORTLIB_HEADER=$(COMMONINC)/rapidio_mport_dma.h
LIBLOG_PATH=$(COMMONLIBA)
LIB_DIRS = -L$(FMDDIR)/libfmdd \
	-L$(COMMONLIBA) \
	-L$(COMMONLIB) \
	-L$(FMDDIR)/libriocp_pe \
	-L$(FMDDIR)/libdd

# Common flags for all applications
RDMAAPP_CFLAGS = -Wall -Werror -Wextra -I../../samples/latency/common/ -Iinc $(OPTFLAGS)

# RDMA Daemon flags & sources
RDMAD_CFLAGS = $(CXXFLAGS) -Wall -Wextra \
		-flto \
		-DRDMA_LL=$(level) -DBE_STRICT \
		-I$(COMMONDIR)/libcli/inc \
		-Idaemon/inc -Idaemon/src -I. -Isrc -Iinc

RDMAD_SOURCES = daemon/src/rdmad_main.cpp \
		daemon/src/rdmad_console.c \
		daemon/src/rdmad_ms_owner.cpp \
		daemon/src/rdmad_ms_owners.cpp \
		daemon/src/rdmad_mspace.cpp \
		daemon/src/rdmad_ibwin.cpp \
		daemon/src/rdmad_inbound.cpp \
		daemon/src/rdmad_srvr_threads.cpp \
		daemon/src/rdmad_clnt_threads.cpp \
		daemon/src/rdmad_fm.cpp \
		daemon/src/rdmad_l2d_dispatch.cpp \
		daemon/src/rdmad_d2d_dispatch.cpp \
		daemon/src/rdmad_actions.cpp

REMDBG_SOURCES	= daemon/src/remdbg.c

# RDMA Library flags, sources and objects
RDMALIB_STATIC_CFLAGS = -Wall -Wextra -Werror -std=c++11 -c $(OPTFLAGS) \
		-Idaemon/inc \
		-Iinc -I$(TOPDIR)/include -I$(COMMONINC) \
		-DRDMA_LL=$(level) $(OPTFLAGS) \
		$(LIB_DIRS) \
		-llog -lmport

RDMALIB_SOURCES = lib/src/librdma.cpp \
		lib/src/librdma_db.cpp \
		lib/src/librdma_dispatch.cpp

RDMALIB_OBJS    = librdma.o librdma_db.o librdma_dispatch.o

# ------------------------------!!! All targets ------------------------------!!!!
all: librdma.a rdmad rsktd test samples rrdma

test: rdmad librdma.a
	$(MAKE) all -C test

samples: rdmad librdma.a
	$(MAKE) all -C samples

rsktd: rdmad librdma.a
	$(MAKE) all -C rskt

FORCE:

# The RDMA static library
librdma.a: $(LIBMPORT) $(RDMALIB_SOURCES) $(MPORTLIB_HEADER) \
	lib/src/*.h $(LIBLOG_PATH)/liblog.a
	@echo ---------- Building librdma.a ----------
	$(CXX) $(RDMALIB_STATIC_CFLAGS) $(RDMALIB_SOURCES) -L$(COMMONLIBA)
	ar rcs librdma.a $(RDMALIB_OBJS)
	mkdir -p $(TOPDIR)/include/libs_a
	cp librdma.a $(COMMONLIBA)

# The RDMA daemon
rdmad: $(MPORTLIB_PATH)/$(MPORTLIB) $(RDMAD_SOURCES) daemon/inc/*.h $(COMMONDIR)/libs_a/* \
	daemon/src/*.h $(INCLUDE_DIR)/*.h
	@echo ---------- Building the RDMA Daemon...
	$(CXX) $(LIB_DIRS) $(LIBS_RPATH) \
	-o $@ $(RDMAD_CFLAGS) $(RDMAD_SOURCES) -llist -ldl -lrt \
	-ltime_utils -lmport -lcli -llog -lfmdd -ldd -lriocp_pe -pthread \
	-lrsvdmem

# Remote debug
rrdma:  $(REMDBG_SOURCES)
	@echo ---------- Building remote debug CLI ...
	$(CXX) $(RDMAD_CFLAGS) -L$(MPORT_DIR) $(LIB_DIRS) \
	$(LIBS_RPATH) \
	-o $@ $(REMDBG_SOURCES) \
	-lcli -lrt -ldl -ltime_utils -lmport -llog -pthread

clean:
	rm -f rdmad rrdma *.o librdma.a librdma.so *.i $(INCLUDE_DIR)/libs_a/librdma.a
	$(MAKE) clean -C rskt
	$(MAKE) clean -C samples
	$(MAKE) clean -C test

