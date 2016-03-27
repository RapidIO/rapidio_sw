#------------------------------------------------------------------------------------------------------------------------------------------------------*
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
#--------------------------------------------------------------------------------------------------------------------------------------------***

# LOG_LEVEL is set by the master Makefile. If this Makefile is run alone, 
# it can override the loglevel on the command line by specifying 'level=XXX'
# otherwise it defaults to 7
ifndef LOG_LEVEL
level?=7
else
level?=$(LOG_LEVEL)
endif

CC?=$(CROSS_COMPILE)g++
CXX?=$(CROSS_COMPILE)g++

TOP_LEVEL?=..
COMMON_DIR?=$(TOP_LEVEL)/common
INCLUDE_DIR?=$(TOP_LEVEL)/include
LIBS_A_PATH=../common/libs_a
MPORTLIB_INC=../common/include
MPORTLIB=libmport.a
MPORTLIB_HEADER=rapidio_mport_dma.h
LIBLOG_PATH=../common/libs_a
LIB_DIRS = \
	-L$(TOP_LEVEL)/fabric_management/libfmdd \
	-L$(COMMON_DIR)/libs_a \
	-L$(INCLUDE_DIR)/libs_a \
	-L$(COMMON_DIR)/libs_so \
	-L$(TOP_LEVEL)/fabric_management/libriocp_pe \
	-L$(TOP_LEVEL)/fabric_management/libdd

# Common flags for all applications
RDMAAPP_CFLAGS = -Wall -Werror -Wextra -g -ggdb -I../../samples/latency/common/ -Iinc/ -O3

# RDMA Daemon flags & sources
RDMAD_CFLAGS = -Wall -Werror -Wextra -std=c++11 -rdynamic -g -ggdb -O3 \
		-ffunction-sections -fdata-sections -flto \
		-Wl,-gc-sections \
		-DRDMA_LL=$(level) -DBE_STRICT -I$(MPORTLIB_INC) \
		-I../common/libcli/inc \
		-I../include -Idaemon/inc -Idaemon/src -I. -Isrc -Iinc -I../common/include

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
RDMALIB_STATIC_CFLAGS = -Wall -Werror -Wextra -std=c++11 -c -g -ggdb \
		-Idaemon/inc \
		-I$(MPORTLIB_INC) -Iinc -I../include -I../common/include \
		-DRDMA_LL=$(level) -O3 \
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

rsktd: rdmad
	$(MAKE) all -C rskt

FORCE:

# The RDMA static library
librdma.a: $(LIBMPORT) $(RDMALIB_SOURCES) $(MPORTLIB_INC)/$(MPORTLIB_HEADER) \
	lib/src/*.h $(LIBLOG_PATH)/liblog.a
	@echo ---------- Building librdma.a ----------
	$(CXX) $(RDMALIB_STATIC_CFLAGS) $(RDMALIB_SOURCES)
	ar rcs librdma.a $(RDMALIB_OBJS) 
	mkdir -p $(TOP_LEVEL)/include/libs_a
	cp librdma.a $(LIBS_A_PATH)

# The RDMA daemon (the AF_UNIX server)
rdmad: $(LIBS_A_PATH)/$(MPORTLIB) $(RDMAD_SOURCES) daemon/inc/*.h $(COMMON_DIR)/libs_a/* $(INCLUDE_DIR)/libs_a/* \
	daemon/src/*.h $(INCLUDE_DIR)/*.h

	@echo ---------- Building the RDMA Daemon...
	$(CXX) $(LIB_DIRS) \
	-o $@ $(RDMAD_CFLAGS) $(RDMAD_SOURCES) -llist -lmport -lrt -lcli -llog -lfmdd -ldd -lriocp_pe -pthread

# Remote debug
rrdma:  $(REMDBG_SOURCES)
	@echo ---------- Building remote debug CLI ...
	$(CXX) $(RDMAD_CFLAGS) $(LIB_DIRS) \
	-o $@ $(REMDBG_SOURCES) \
	-lcli -lrt -lmport -llog -pthread

clean:
	rm -f rdmad rrdma *.o librdma.so *.i
	$(MAKE) clean -C rskt
	$(MAKE) clean -C samples
	$(MAKE) clean -C test

