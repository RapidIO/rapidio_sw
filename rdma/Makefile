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

# Default configurations
level?=6
debug?=DEBUG
KDIR?=/usr/src/linux
KERNINC=$(KDIR)/include
MPORTLIB_PATH=../libmport
MPORTLIB_INC=../include
MPORTLIB=libmport.so
MPORTLIB_HEADER=riodp_mport_lib.h
LIBCLI_PATH=../common/libs_a
LIBLOG_PATH=../common/libs_a


# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb

# Common flags for all applications
RDMAAPP_CFLAGS = -Wall -Werror -Wextra -g -ggdb -D$(debug) -I../../samples/latency/common/ -Iinc/ -O3

# RDMA Daemon flags & sources
RDMAD_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -g -ggdb -O3 \
		-DRDMA_LL=$(level) -D$(debug) -I$(KERNINC) -I$(MPORTLIB_INC) \
		-I../common/libcli/inc \
		-I../include -Idaemon/inc -Idaemon/src -I. -Isrc -Iinc -I../common/include

RDMAD_SOURCES = rdmad_rpc.c rdmad_xdr.c \
		src/rdma_mq_msg.c \
		daemon/src/rdmad_svc.cpp \
		daemon/src/rdmad_main.cpp \
		daemon/src/rdmad_console.c \
		daemon/src/rdmad_ms_owner.cpp \
		daemon/src/rdmad_mspace.cpp \
		daemon/src/rdmad_inbound.cpp \
		daemon/src/rdmad_srvr_threads.cpp \
		daemon/src/rdmad_clnt_threads.cpp

# RDMA Library flags, sources and objects
RDMALIB_SHARED_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -c -g -ggdb -fPIC \
		-I$(MPORTLIB_INC) -I$(KERNINC) -Iinc -I../include -I../common/include \
		-D$(debug) -DRDMA_LL=$(level) -O3 -rdynamic -lnsl

RDMALIB_STATIC_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -c -g -ggdb \
		-I$(MPORTLIB_INC) -I$(KERNINC) -Iinc -I../include -I../common/include \
		-D$(debug) -DRDMA_LL=$(level) -O3

RDMALIB_SOURCES = rdmad_xdr.c rdmalib_rpc.c \
		lib/src/librdma.cpp \
		lib/src/librdma_db.cpp \
		src/rdma_mq_msg.c

RDMALIB_OBJS    = rdmad_xdr.o rdmalib_rpc.o librdma.o librdma_db.o rdma_mq_msg.o

# ------------------------------!!! All targets ------------------------------!!!!
all: librdma.so librdma.a rdmad rsktd samples test

test: rdmad librdma.so
	$(MAKE) all -C test

samples: rdmad librdma.so
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
	-Wl,-rpath=$(LIBLOG_PATH) -o $@ $(RDMALIB_OBJS) $(SO_LIB_LKFLAGS) -lmport -lrt -llog -pthread

# The RDMA static library
librdma.a: $(LIBMPORT) $(RDMALIB_SOURCES) $(MPORTLIB_INC)/$(MPORTLIB_HEADER) \
	lib/src/*.h $(LIBLOG_PATH)/liblog.a librdma.so
	@echo ---------- Building librdma.a ----------
	$(CXX) $(RDMALIB_STATIC_CFLAGS) $(RDMALIB_SOURCES)
	ar rcs librdma.a $(RDMALIB_OBJS)

# The RDMA daemon (the RPC server)
rdmad: $(MPORTLIB_PATH)/$(MPORTLIB) $(RDMAD_SOURCES) daemon/inc/*.h \
	daemon/src/*.h $(LIBCLI_PATH)/libcli.a $(LIBLOG_PATH)/liblog.a ../common/include

	@echo ---------- Building the RDMA Daemon...
	$(CXX) -L$(MPORTLIB_PATH) -L$(LIBCLI_PATH) -L$(LIBLOG_PATH) \
	-Wl,-rpath=$(MPORTLIB_PATH) -Wl,-rpath=$(LIBCLI_PATH) -Wl,-rpath=$(LIBLOG_PATH) \
	-o $@ $(RDMAD_CFLAGS) $(RDMAD_SOURCES) -lmport -lrt -lcli -llog -pthread

# RPC-generated files
rdmalib_rpc.c: rdmad.x rdmad.h
	rpcgen rdmad.x -l > rdmalib_rpc.c
rdmad_rpc.c: rdmad.x rdmad.h
	rpcgen rdmad.x -m > rdmad_rpc.c
rdmad.h: rdmad.x
	rpcgen rdmad.x -h > rdmad.h
rdmad_xdr.c: rdmad.x rdmad.h
	rpcgen rdmad.x -c > rdmad_xdr.c
	rm -f rdmad_xdr.b
	mv rdmad_xdr.c rdmad_xdr.b
	perl remove_buf.pl

clean:
	rm -f rdmalib_rpc.c rdmad_rpc.c rdmad_xdr.c rdmad.h rdmad *.o rdmad_xdr.b librdma.so
	$(MAKE) clean -C rskt
	$(MAKE) clean -C samples
	$(MAKE) clean -C test

