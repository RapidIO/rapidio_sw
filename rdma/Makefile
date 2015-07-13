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
level?=$(LOG_LEVEL)

KDIR?=/usr/src/linux
KERNINC=$(KDIR)/include
TOP_LEVEL?=..
COMMON_DIR?=$(TOP_LEVEL)/common
INCLUDE_DIR?=$(TOP_LEVEL)/include
MPORTLIB_PATH=../libmport
MPORTLIB_INC=../include
MPORTLIB=libmport.so
MPORTLIB_HEADER=rapidio_mport_lib.h
#LIBCLI_PATH=../common/libs_a
LIBLOG_PATH=../common/libs_a
LIB_DIRS = \
	-L$(TOP_LEVEL)/fabric_management/libfmdd \
	-L$(COMMON_DIR)/libs_a \
	-L$(INCLUDE_DIR)/libs_a \
	-L$(COMMON_DIR)/libs_so \
	-L$(TOP_LEVEL)/fabric_management/libriocp_pe \
	-L$(TOP_LEVEL)/fabric_management/libdd \
	-L$(TOP_LEVEL)/fabric_management/librio_maint 

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb

# Common flags for all applications
RDMAAPP_CFLAGS = -Wall -Werror -Wextra -g -ggdb -I../../samples/latency/common/ -Iinc/ -O3

# RDMA Daemon flags & sources
RDMAD_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -g -ggdb -O3 \
		-DRDMA_LL=$(level) -I$(KERNINC) -I$(MPORTLIB_INC) \
		-I../common/libcli/inc \
		-I../include -Idaemon/inc -Idaemon/src -I. -Isrc -Iinc -I../common/include

RDMAD_SOURCES = src/rdma_mq_msg.c \
		daemon/src/rdmad_svc.cpp \
		daemon/src/rdmad_main.cpp \
		daemon/src/rdmad_console.c \
		daemon/src/rdmad_ms_owner.cpp \
		daemon/src/rdmad_mspace.cpp \
		daemon/src/rdmad_inbound.cpp \
		daemon/src/rdmad_srvr_threads.cpp \
		daemon/src/rdmad_clnt_threads.cpp \
		daemon/src/rdmad_fm.cpp

# RDMA Library flags, sources and objects
RDMALIB_SHARED_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -c -g -ggdb -fPIC \
		-Idaemon/inc \
		-I$(MPORTLIB_INC) -I$(KERNINC) -Iinc -I../include -I../common/include \
		-DRDMA_LL=$(level) -O3 -rdynamic -lnsl

RDMALIB_STATIC_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -c -g -ggdb \
		-Idaemon/inc \
		-I$(MPORTLIB_INC) -I$(KERNINC) -Iinc -I../include -I../common/include \
		-DRDMA_LL=$(level) -O3

RDMALIB_SOURCES = lib/src/librdma.cpp \
		lib/src/librdma_db.cpp \
		src/rdma_mq_msg.c

RDMALIB_OBJS    = librdma.o librdma_db.o rdma_mq_msg.o

# ------------------------------!!! All targets ------------------------------!!!!
all: librdma.a rdmad rsktd test samples

test: rdmad librdma.a
	$(MAKE) all -C test

samples: rdmad librdma.a
	$(MAKE) all -C samples

rsktd: rdmad
	$(MAKE) all -C rskt

FORCE:

# The RDMA shared library (the RPC client)
librdma.so: $(LIBMPORT) $(RDMALIB_SOURCES) $(MPORTLIB_INC)/$(MPORTLIB_HEADER) \
	lib/src/*.h $(LIBLOG_PATH)/liblog.a
	@echo ---------- Building librdma.so ----------
	$(CXX) $(RDMALIB_SHARED_CFLAGS) $(RDMALIB_SOURCES)
	$(CXX) -L$(MPORTLIB_PATH) -L$(LIBLOG_PATH) -Wl,-rpath=$(MPORTLIB_PATH) \
	-o $@ $(RDMALIB_OBJS) $(SO_LIB_LKFLAGS) -lmport -lrt -llog -pthread

# The RDMA static library
librdma.a: $(LIBMPORT) $(RDMALIB_SOURCES) $(MPORTLIB_INC)/$(MPORTLIB_HEADER) \
	lib/src/*.h $(LIBLOG_PATH)/liblog.a
	@echo ---------- Building librdma.a ----------
	$(CXX) $(RDMALIB_STATIC_CFLAGS) $(RDMALIB_SOURCES)
	ar rcs librdma.a $(RDMALIB_OBJS)
	mkdir -p $(TOP_LEVEL)/include/libs_a
	cp librdma.a $(TOP_LEVEL)/include/libs_a
#Generate preprocessor output
	$(CXX) $(RDMALIB_STATIC_CFLAGS) -I$(KERNINC) -I../common/include -I$(MPORTLIB_INC) -E lib/src/librdma.cpp > librdma.i

# The RDMA daemon (the RPC server)
rdmad: $(MPORTLIB_PATH)/$(MPORTLIB) $(RDMAD_SOURCES) daemon/inc/*.h $(COMMON_DIR)/libs_a/* $(INCLUDE_DIR)/libs_a/* \
	daemon/src/*.h $(INCLUDE_DIR)/*.h

	@echo ---------- Building the RDMA Daemon...
	$(CXX) -L../common/liblist/ $(LIB_DIRS) -L$(MPORTLIB_PATH) -Wl,-rpath=$(MPORTLIB_PATH) \
	-Wl,-rpath=$(TOP_LEVEL)/common/liblist \
	-o $@ $(RDMAD_CFLAGS) $(RDMAD_SOURCES) -llist -lmport -lrt -lcli -llog -lfmdd -ldd -lriocp_pe -pthread
# Building preprocessor output for select files.
	$(CXX) $(RDMAD_CFLAGS) -E daemon/src/rdmad_main.cpp > rdmad_main.i
	$(CXX) $(RDMAD_CFLAGS) -E daemon/src/rdmad_svc.cpp > rdmad_svc.i
	$(CXX) $(RDMAD_CFLAGS) -E daemon/src/rdmad_srvr_threads.cpp > rdmad_srvr_threads.i
	$(CXX) $(RDMAD_CFLAGS) -E daemon/src/rdmad_clnt_threads.cpp > rdmad_clnt_threads.i

clean:
	rm -f rdmad *.o librdma.so *.i
	$(MAKE) clean -C rskt
	$(MAKE) clean -C samples
	$(MAKE) clean -C test

