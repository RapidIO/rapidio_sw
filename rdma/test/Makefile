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

LIBMPORT_PATH=../../common/libs_a
COMMON_LIBS_PATH=../../common/libs_a
TOP_LEVEL=../..
# LOG_LEVEL is set by the master Makefile. If this Makefile is run alone, 
# it can override the loglevel on the command line by specifying 'level=XXX'
# otherwise it defaults to 7
ifndef LOG_LEVEL
level?=7
else
level?=$(LOG_LEVEL)
endif

# Common flags for all applications
RDMAAPP_CFLAGS = --std=c++11 -Wall -Werror -Wextra -g -ggdb -DRDMA_LL=$(level) \
		-I$(TOP_LEVEL)/include \
		-I$(TOP_LEVEL)/common/include \
		-I$(TOP_LEVEL)/rdma/lib/src \
		-I$(TOP_LEVEL)/rdma/inc \
		-I$(TOP_LEVEL)/fabric_management/librio_switch/inc \
		-I$(TOP_LEVEL)/fabric_management/libdid/inc \
		-I$(TOP_LEVEL)/fabric_management/libct/inc

BAT_DEPS = $(COMMON_LIBS_PATH)/libmport.a $(COMMON_LIBS_PATH)/librdma.a \
	../inc/* ../daemon/src/* ../daemon/inc/*

BAT_BUILD_STRING = $(CXX) -L$(COMMON_LIBS_PATH) \
	-L$(TOP_LEVEL)/fabric_management/libfmdd \
	-L$(TOP_LEVEL)/fabric_management/libriocp_pe \
	-L$(TOP_LEVEL)/fabric_management/libdd \
	-o $@ $(RDMAAPP_CFLAGS) \
	$($@_SRC) -lrt -lmport -llist -ldl -llog -lrdma -lcli -pthread -lfmdd -ldd -lriocp_pe -llist

bat_server_SRC = src/bat_server.cpp

bat_client_SRC = src/bat_client.cpp src/bat_client_rdma_api.cpp src/bat_client_test_cases.cpp

bat_child_SRC = src/bat_child.cpp


all: bat_server bat_client

# Batch Automated Test (BAT)
bat_server: $(BAT_DEPS) $(bat_server_SRC)
	@echo ---------- Building bat_server...
	$(BAT_BUILD_STRING)

bat_client: $(BAT_DEPS) $(bat_client_SRC)
	@echo ---------- Building bat_client...
	$(BAT_BUILD_STRING)

bat_child: $(BAT_DEPS) $(bat_child_SRC)
	@echo ---------- Building bat_child...
	$(BAT_BUILD_STRING)

clean:
	@rm -f bat_client bat_server bat_child *.o *~
