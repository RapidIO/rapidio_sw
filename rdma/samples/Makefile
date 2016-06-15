#----------------------------------------------------------------------------*
#Copyright (c) 2015, Integrated Device Technology Inc.
#Copyright (c) 2015, RapidIO Trade Association
#All rights reserved.
#
##Redistribution and use in source and binary forms, with or without modification,
#are permitted provided that the following conditions are met:
#
##1. Redistributions of source code must retain the above copyright notice, this
#list of conditions and the following disclaimer.
#
##2. Redistributions in binary form must reproduce the above copyright notice,
#this list of conditions and the following disclaimer in the documentation
#and/or other materials provided with the distribution.
#
##3. Neither the name of the copyright holder nor the names of its contributors
#may be used to endorse or promote products derived from this software without
#specific prior written permission.
#
##THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
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
# Paths to libraries
# 
TOPDIR?=$(shell pwd)/../..
include $(TOPDIR)/rules.mk

LOG_LEVEL?=0
DEBUG?=DEBUG

LIBRDMA_PATH=..

level?=$(LOG_LEVEL)

# Build flags and include paths
RDMAAPP_CFLAGS = $(CXXFLAGS) -Wall -Wextra -DRDMA_LL=$(level) \
	-I$(KINC) -I. \
	-D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS

# Common dependencies
RDMA_APP_DEPS= $(LIBRDMA_PATH)/librdma.a \
	../inc/* ../daemon/src/* ../daemon/inc/*

LIBRARIES=-lrdma -lriocp_pe -lfmdd -ldd -lmport -llist -llog -lcli -pthread -lrt

# Common build strings
# For C apps
BUILD_STRING_C = $(CXX) $(CXXFLAGS) -L$(LIBRDMA_PATH) -L$(COMMONLIB) \
	-L$(FMDDIR)/libfmdd \
	-L$(FMDDIR)/libriocp_pe \
	-L$(FMDDIR)/libdd \
	$(LIBS_RPATH) \
	-o $@ $(RDMAAPP_CFLAGS) \
	src/$@.c $(LIBRARIES)

# For C++ apps
BUILD_STRING_CPP = $(CXX) -L$(LIBRDMA_PATH) -L$(COMMONLIB) \
	-L$(FMDDIR)/libfmdd \
	-L$(FMDDIR)/libriocp_pe \
	-L$(FMDDIR)/libdd \
	$(LIBS_RPATH) \
	-o $@ $(RDMAAPP_CFLAGS) \
	src/$@.cpp $(LIBRARIES)

all: rdma_server rdma_client rdma_user multi_ms_server multi_ms_client create_and_die

# RDMA Server App
rdma_server: src/rdma_server.c $(RDMA_APP_DEPS)
	@echo ---------- Building rdma_server...
	$(BUILD_STRING_C)

# RDMA User App
rdma_user: src/rdma_user.c $(RDMA_APP_DEPS)
	@echo ---------- Building rdma_user...
	$(BUILD_STRING_C)

# RDMA Client App
rdma_client: src/rdma_client.c $(RDMA_APP_DEPS)
	@echo ---------- Building rdma_client...
	$(BUILD_STRING_C)

# RDMA multi memory space server app
multi_ms_server: src/multi_ms_server.cpp $(RDMA_APP_DEPS)
	@echo ---------- Building multi_ms_server ...
	$(BUILD_STRING_CPP)

# RDMA multi memory space client app
multi_ms_client: src/multi_ms_client.cpp $(RDMA_APP_DEPS)
	@echo ---------- Building multi_ms_client ...
	$(BUILD_STRING_CPP)

# Tests auto-cleanup by the daemon of msos, ms's and msubs created by an app
# that later dies
create_and_die:	src/create_and_die.cpp $(RDMA_APP_DEPS)
	@echo ---------- Building create_and_die
	$(BUILD_STRING_CPP)

clean:
	@rm -Rf *.o *~ rdma_server rdma_client rdma_user multi_ms_server multi_ms_client create_and_die
