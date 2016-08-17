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

RSKT_DIR=../lib
INCLUDE_DIR?=$(TOPDIR)/include

LIB_DIRS?=-L$(COMMONLIB) -L$(COMMONLIBA) -L$(FMDDIR)/libriocp_pe

%.o: %.c
	$(CXX) $(CXXFLAGS) -o $@ -c $< -Wall \
	-DRDMA_LL=$(LOG_LEVEL)


RSKT_SERVER_SOURCE = rskt_server.c
RSKT_SERVER_OBJECTS = rskt_server.o

RSKT_CLIENT_SOURCE = rskt_client.c
RSKT_CLIENT_OBJECTS = rskt_client.o

LIBRARIES = -lrskt -lrdma -lriocp_pe -lmport -lmemops_intf -lrsvdmem \
        -llist -llog -lfmdd -ldd -lcli -ltime_utils -lrt -ldl

all: rskt_client rskt_server rskt_tun rskt_shim.so rskt_wwwserver rskt_wwwclient rskt_wwwclient1

rskt_server:  $(RSKT_SERVER_SOURCE) $(COMMONDIR)/include/*.h
	@echo ---------- Building rskt_server
	$(CXX) $(CXXFLAGS) -L$(COMMONLIB) \
	-I./src -I./inc -I../inc \
	-DRDMA_LL=$(LOG_LEVEL) \
	$(LIBS_RPATH) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKT_SERVER_SOURCE) $(LIB_DIRS) $(LIBRARIES)

rskt_client:  $(RSKT_CLIENT_SOURCE) $(COMMONDIR)/include/*.h
	@echo ---------- Building rskt_client
	$(CXX) $(CXXFLAGS) -L$(COMMONLIB) \
	-I./inc -I../inc \
	-DRDMA_LL=$(LOG_LEVEL) \
	$(LIBS_RPATH) \
	-o $@ $(RSKTLIB_OBJECTS) $(RSKT_CLIENT_SOURCE) $(LIB_DIRS) $(LIBRARIES)

rskt_tun: rskt_tun.c $(COMMONDIR)/include/*.h
	@echo ---------- Building rskt_tun
	$(CXX) $(CXXFLAGS) -L$(COMMONLIB) \
	-I./inc -I../inc \
	-I$(TOPDIR)/utils/goodput/inc \
	-DRDMA_LL=$(LOG_LEVEL) \
	$(RSKTLIB_OBJECTS) rskt_tun.c $(TOPDIR)/utils/goodput/src/tun_ipv4.c \
	$(LIB_DIRS) -lrskt -lrdma -lfmdd -ldd -llist -lcli -llog -lmport -lmemops_intf -lrsvdmem -ltime_utils -lrt -lpthread \
	$(LIBS_RPATH) \
	-o $@

rskt_wwwserver: rskt_wwwserver.cc $(COMMONDIR)/include/*.h
	@echo ---------- Building rskt_wwwserver
	$(CXX) $(CXXFLAGS) -L$(COMMONLIB) \
	-I./inc -I../inc \
	-DRDMA_LL=$(LOG_LEVEL) \
	$(RSKTLIB_OBJECTS) rskt_wwwserver.cc \
	$(LIB_DIRS) -lrskt -lrdma -lfmdd -ldd -llist -lcli -llog -lmport -lmemops_intf -lrsvdmem -ltime_utils -lrt -lpthread \
	$(LIBS_RPATH) \
	-o $@

rskt_wwwclient: rskt_wwwclient.cc $(COMMONDIR)/include/*.h
	@echo ---------- Building rskt_wwwclient
	$(CXX) $(CXXFLAGS) -L$(COMMONLIB) \
	-I./inc -I../inc \
	-DRDMA_LL=$(LOG_LEVEL) \
	$(RSKTLIB_OBJECTS) rskt_wwwclient.cc \
	$(LIB_DIRS) -lrskt -lrdma -lfmdd -ldd -llist -lcli -llog -lmport -lmemops_intf -lrsvdmem -ltime_utils -lrt -lpthread \
	$(LIBS_RPATH) \
	-o $@

rskt_wwwclient1: rskt_wwwclient1.cc $(COMMONDIR)/include/*.h
	@echo ---------- Building rskt_wwwclient
	$(CXX) $(CXXFLAGS) -L$(COMMONLIB) \
	-I./inc -I../inc \
	-DRDMA_LL=$(LOG_LEVEL) \
	$(RSKTLIB_OBJECTS) rskt_wwwclient1.cc \
	$(LIB_DIRS) -lrskt -lrdma -lfmdd -ldd -llist -lcli -llog -lmport -lmemops_intf -lrsvdmem -ltime_utils -lrt -lpthread \
	$(LIBS_RPATH) \
	-o $@

rskt_shim.so: rskt_shim.cc mport_shim.so rskt_shim2.so
	$(CXX) $^ -shared -fPIC -pthread -ldl -lpthread -o $@ -O0 -ggdb

rskt_shim2.so: rskt_shim2.cc $(COMMONDIR)/include/*.h
	@echo ---------- Building rskt_shim2
	$(CXX) $(CXXFLAGS) -L$(COMMONLIB) \
	-I./inc -I../inc \
	-DRDMA_LL=$(LOG_LEVEL) \
	$(RSKTLIB_OBJECTS) rskt_shim2.cc \
	$(LIB_DIRS) -lrskt -lrdma -lfmdd -ldd -llist -lcli -llog -lmport -lmemops_intf -ltime_utils -lrt -lpthread \
	$(LIBS_RPATH) \
	-shared -fPIC -ldl \
	-O0 -ggdb \
	-o $@

mport_shim.so: mport_shim.cc $(COMMONDIR)/include/*.h
	@echo ---------- Building mport_shim.so
	$(CXX) $(CXXFLAGS) -L$(COMMONLIB) \
	-I./inc -I../inc \
	-DRDMA_LL=$(LOG_LEVEL) \
	$(RSKTLIB_OBJECTS) mport_shim.cc \
	$(LIB_DIRS) -lmport -ltime_utils -lrt -lpthread \
	$(LIBS_RPATH) \
	-shared -fPIC \
	-o $@

clean:
	rm -f *.o *.o *~ *.exe core rskt_server rskt_client rskt_tun rskt_wwwserver rskt_wwwclient* rskt_shim*.so mport_shim.so
