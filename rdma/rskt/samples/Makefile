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

%.o: %.c
	$(CXX) $(CXXFLAGS) -o $@ -c $< -Wall \
	-I.-I../inc \
	-I$(COMMONDIR)/liblist/inc \
	-I../../lib/src \
	-I$(TOPDIR)/rdma/inc \
	-DRDMA_LL=$(LOG_LEVEL)


RSKT_SERVER_SOURCE = rskt_server.c
RSKT_SERVER_OBJECTS = rskt_server.o

RSKT_CLIENT_SOURCE = rskt_client.c
RSKT_CLIENT_OBJECTS = rskt_client.o

LIBRARIES = -lrskt -lcli -lrt -ldl -lmport \
	-llist -llog -lrdma -lfmdd -ldd -lriocp_pe

all: rskt_client rskt_server


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

clean:
	rm -f *.o *.o *~ *.exe core rskt_server rskt_client 
