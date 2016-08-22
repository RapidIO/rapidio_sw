#----------------------------------------------------------------------------*
#Copyright (c) 2015, Integrated Device Technology Inc.
#Copyright (c) 2015, RapidIO Trade Association
#All rights reserved.
#
####Redistribution and use in source and binary forms, with or without modification,
#are permitted provided that the following conditions are met:
#
####1. Redistributions of source code must retain the above copyright notice, this
#list of conditions and the following disclaimer.
#
####2. Redistributions in binary form must reproduce the above copyright notice,
#this list of conditions and the following disclaimer in the documentation
#and/or other materials provided with the distribution.
#
####3. Neither the name of the copyright holder nor the names of its contributors
#may be used to endorse or promote products derived from this software without
#specific prior written permission.
#
####THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
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

TOPDIR?=$(shell pwd)/../..
include $(TOPDIR)/rules.mk

TARGETS:=$(patsubst src/%.cpp,%,$(wildcard src/*.cpp))

LIBMPORT_PATH=../libmport
LIBTU_PATH=../libtime_utils
LIBLOG_PATH=../liblog
LIBCLI_PATH=../libcli
CXXFLAGS+=-D$(DEBUG_CTL) -DRDMA_LL=$(LOG_LEVEL)

CM_BUILD_STRING = $(CXX) $(CXXFLAGS) \
	$(LIBS_RPATH) \
	-o $@ \
	src/$@.cpp \
	-ltime_utils -lmport -llog -lcli

UNIX_BUILD_STRING = $(CXX) $(CXXFLAGS) \
	-I$(TOPDIR)/rdma/inc \
	-L$(LIBLOG_PATH) -L$(LIBCLI_PATH) \
	$(LIBS_RPATH) \
	-o $@ \
	src/$@.cpp \
	-llog -lcli -lrt -pthread

all: $(TARGETS)

# Test apps for CM socket wrapper class
cm_sock_server: src/cm_sock_server.cpp $(COMMONINC)/*.h
	@echo ---------- Building $@
	$(CM_BUILD_STRING)

cm_sock_client: $(LIBMPORT_PATH)/libmport.so $(LIBLOG_PATH)/liblog.so src/cm_sock_client.cpp $(COMMONINC)/*.h
	@echo ---------- Building $@
	$(CM_BUILD_STRING)

# Test apps for UNIX socket wrapper class
unix_sock_server: src/unix_sock_server.cpp $(COMMONINC)/*.h
	@echo ---------- Building $@
	$(UNIX_BUILD_STRING)

unix_sock_client: src/unix_sock_client.cpp $(COMMONINC)/*.h
	@echo ---------- Building $@
	$(UNIX_BUILD_STRING)

msg_q1:	src/msg_q1.cpp
	@echo ---------- Building $@
	$(UNIX_BUILD_STRING)

msg_q2:	src/msg_q2.cpp
	@echo ---------- Building $@
	$(UNIX_BUILD_STRING)

clean:
	rm -f $(TARGETS) *.o *~
