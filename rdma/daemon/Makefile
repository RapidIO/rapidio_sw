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
MPORTLIB_PATH=../../samples/mcd_base
MPORTLIB_INC=../../samples/mcd_base/test
MPORTLIB=libmport.so
MPORTLIB_HEADER=riodp_mport_lib.h

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb

# Common flags for all applications
RDMAAPP_CFLAGS = -Wall -Werror -Wextra -g -ggdb -D$(debug) -I../../samples/latency/common/ -Iinc/ -O3

# RDMA Daemon flags & sources
RDMAD_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -g -ggdb -O3 -DRDMA_LL=$(level) -D$(debug)\
		-I$(KERNINC) -I$(MPORTLIB_INC) -I. -Isrc -Iinc 

RDMAD_SOURCES = src/rdmad_main.cpp rdmad_rpc.c rdmad_xdr.c src/rdmad_svc.cpp \
		src/rdma_mq_msg.c src/rdmad_console.c \
		src/rdmad_ms_owner.cpp \
		src/rdmad_mspace.cpp src/rdmad_inbound.cpp \
		src/rdmad_srvr_threads.cpp src/rdmad_clnt_threads.cpp

# RDMA Library flags, sources and objects
RDMALIB_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -c -g -ggdb -fPIC \
		-I$(MPORTLIB_INC) -I$(KERNINC) -Iinc -I.\
		-D$(debug) -DRDMA_LL=$(level) -O3 -rdynamic -lnsl

RDMALIB_SOURCES = rdmad_xdr.c rdmalib_rpc.c src/librdma.cpp\
		src/librdma_db.cpp src/rdma_mq_msg.c

RDMALIB_OBJS    = rdmad_xdr.o rdmalib_rpc.o librdma.o librdma_db.o rdma_mq_msg.o

# CLI library flags, sources and objects
CLILIB_CFLAGS = -Wall -std=gnu++0x -c -g -ggdb -fPIC \
		 -Iinc/ -Isrc/ -I$(KERNINC) \
		-D$(debug) -DRDMA_LL=$(level) -O2 -rdynamic -lnsl

CLILIB_SOURCES = src/cli_base_init.c src/cli_cmd_db.c src/cli_cmd_line.c\
		src/cli_parse.c src/cli_console.c

CLILIB_OBJS = cli_base_init.o cli_cmd_db.o cli_cmd_line.o\
		cli_parse.o cli_console.o

# LOG library flags, sources and objects
LOGLIB_CFLAGS = $(CLILIB_CFLAGS)
LOGLIB_SOURCES = src/rdma_logger.cpp
LOGLIB_OBJS = rdma_logger.o

# ------------------------------!!! All targets ------------------------------!!!!
all: rdmad rdma_server rdma_user rdma_client libcli.so liblog.so \
	cm_sock_server cm_sock_client bat_client bat_server

# The RDMA library (the RPC client)
librdma.so: $(LIBMPORT) $(RDMALIB_SOURCES) $(MPORTLIB_INC)/$(MPORTLIB_HEADER) inc/*.h src/*.h liblog.so
	@echo ---------- Building librdma.so...
	g++ $(RDMALIB_CFLAGS) $(RDMALIB_SOURCES)
	g++ -L$(MPORTLIB_PATH) -L$(shell pwd) -Wl,-rpath=$(MPORTLIB_PATH) \
	-Wl,-rpath=$(shell pwd) -o $@ $(RDMALIB_OBJS) $(SO_LIB_LKFLAGS) -lmport -lrt -llog

# The CLI library
libcli.so: $(CLILIB_SOURCES) src/*.h
	@echo ---------- Building libcli.so...
	g++ $(CLILIB_CFLAGS) $(CLILIB_SOURCES)
	g++ -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(CLILIB_OBJS) $(SO_LIB_LKFLAGS)

# The logger library
liblog.so: $(LOGLIB_SOURCES) src/*.h inc/rdma_logger.h
	@echo ---------- Building liblog.so...
	g++ $(LOGLIB_CFLAGS) $(LOGLIB_SOURCES)
	g++ -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(LOGLIB_OBJS) $(SO_LIB_LKFLAGS)

# The RDMA daemon (the RPC server)
rdmad: $(MPORTLIB_PATH)/$(MPORTLIB) $(RDMAD_SOURCES) inc/*.h src/*.h libcli.so liblog.so
	@echo ---------- Building the RDMA Daemon...
	g++ -L$(MPORTLIB_PATH) -L$(shell pwd) -Wl,-rpath=$(MPORTLIB_PATH) \
	 -Wl,-rpath=$(shell pwd) -o $@ $(RDMAD_CFLAGS) $(RDMAD_SOURCES) \
	-lmport -lrt -lcli -llog

# RDMA Server App (NOTE: currently expects shared library in same directory
rdma_server: librdma.so ../test_app/rdma_server.c inc/*.h ../test_app/test_macros.h
	@echo ---------- Building rdma_server...
	gcc -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(RDMAAPP_CFLAGS) ../test_app/$@.c -lrdma

# RDMA User App (NOTE: currently expects shared library in same directory
rdma_user: librdma.so ../test_app/rdma_user.c inc/*.h ../test_app/test_macros.h
	@echo ---------- Building rdma_user...
	gcc -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(RDMAAPP_CFLAGS) ../test_app/$@.c -lrdma

# RDMA Client App (NOTE: currently expects shared library in same directory
rdma_client: librdma.so ../test_app/rdma_client.c inc/*.h ../test_app/test_macros.h
	@echo ---------- Building rdma_client...
	gcc -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(RDMAAPP_CFLAGS) ../test_app/$@.c -lrdma

# Batch Automated Test (BAT)
bat_server: librdma.so ../test_app/bat_server.cpp inc/*.h ../test_app/*.h src/*.h
	@echo ---------- Building bat_server...
	g++ --std=gnu++0x -L$(MPORTLIB_PATH) -L$(shell pwd) -Wl,-rpath=$(shell pwd) \
	-Wl,-rpath=$(MPORTLIB_PATH) -o $@ \
	-I$(MPORTLIB_INC) -Isrc -I$(KERNINC) $(RDMAAPP_CFLAGS) \
	../test_app/$@.cpp -lrdma -lmport -llog

bat_client: $(MPORTLIB_PATH)/$(MPORTLIB) ../test_app/bat_client.cpp inc/*.h ../test_app/*.h src/*.h
	@echo ---------- Building bat_client...
	g++ --std=gnu++0x -L$(MPORTLIB_PATH) -L$(shell pwd) -Wl,-rpath=$(shell pwd) \
	-Wl,-rpath=$(MPORTLIB_PATH) -o $@ \
	-I$(MPORTLIB_INC) -Isrc -I$(KERNINC) $(RDMAAPP_CFLAGS) \
	../test_app/$@.cpp -lrdma -lmport -llog

# Test app for CM socket wrapper class
cm_sock_server: $(MPORTLIB_PATH)/$(MPORTLIB) ../test_app/cm_sock_server.cpp inc/*.h src/*.h
	@echo ---------- Building cm_sock_server ...
	g++ -L$(shell pwd) -L$(MPORTLIB_PATH) -Wl,-rpath=$(shell pwd) \
	-Wl,-rpath=$(MPORTLIB_PATH) -o $@ $(RDMAD_CFLAGS) ../test_app/$@.cpp -lmport -lrt -llog

cm_sock_client: $(MPORTLIB_PATH)/$(MPORT_LIB) ../test_app/cm_sock_client.cpp inc/*.h src/*.h
	@echo ---------- Building cm_sock_client ...
	g++ -L$(shell pwd) -L$(MPORTLIB_PATH) -Wl,-rpath=$(shell pwd) \
	-Wl,-rpath=$(MPORTLIB_PATH) -o $@ $(RDMAD_CFLAGS) ../test_app/$@.cpp -lmport -lrt -llog

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
	./remove_buf.pl

clean:
	rm -f rdmalib_rpc.c rdmad_rpc.c rdmad_xdr.c rdmad.h rdmad rdma_server \
	rdma_client rdma_user libcli.so liblog.so librdma.so cm_sock_server cm_sock_client *.o
