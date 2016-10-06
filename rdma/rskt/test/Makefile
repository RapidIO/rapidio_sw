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

TOPDIR?=$(shell pwd)/../../..
include $(TOPDIR)/rules.mk

DEBUG?=DEBUG

DAEMON_SRC = ../daemon/src
RDMA_SRC = fake_librdma
MPORT_SRC = fake_libmport
FAKE_DMN = fake_daemon
FAKE_FMDD = fake_libfmdd

level=$(LOG_LEVEL)

NAME:=rskt
TEST_TARGET:=$(NAME)_test

CXXFLAGS+=-D$(DEBUG_CTL) -DRDMA_LL=$(level)
CXXFLAGS+=-Isrc
CXXFLAGS+=-I$(FMDDIR)/librio_switch/inc
CXXFLAGS+=-I$(MPORT_SRC)
CXXFLAGS+=-I$(FAKE_FMDD)
CXXFLAGS+=-I../daemon/src -I../inc -I/../lib/src
LDFLAGS+=-L. -l$(NAME)

OBJECTS:=$(DAEMON_SRC)/librsktd_lib.o $(DAEMON_SRC)/librsktd_cli.o
OBJECTS+=$(DAEMON_SRC)/librsktd_dmn.o $(DAEMON_SRC)/librsktd_msg_proc.o 
OBJECTS+=$(DAEMON_SRC)/librsktd_sn.o  $(DAEMON_SRC)/librsktd_fm.o 
OBJECTS+=$(DAEMON_SRC)/librsktd_speer.o $(DAEMON_SRC)/librsktd_wpeer.o
HEADERS:=$(wildcard ../daemon/src/*.h) $(TOPDIR)/include/librskt.h 

TEST_HEADERS:=$(wildcard src/*.h) $(wildcard inc/*.h)
TEST_HEADERS+=$(wildcard $(MPORT_SRC)/*.h) $(wildcard $(FAKE_FMDD)/*.h)
TEST_OBJECTS+=$(patsubst $(RDMA_SRC)/%.c,$(RDMA_SRC)/%.o,$(wildcard $(RDMA_SRC)/*.c))
TEST_OBJECTS+=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c))
TEST_OBJECTS+=$(patsubst $(MPORT_SRC)/%.c,$(MPORT_SRC)/%.o,$(wildcard $(MPORT_SRC)/*.c))
TEST_OBJECTS+=$(patsubst $(FAKE_DMN)/%.c,$(FAKE_DMN)/%.o,$(wildcard $(FAKE_DMN)/*.c))
TEST_OBJECTS+=$(patsubst $(FAKE_FMDD)/%.c,$(FAKE_FMDD)/%.o,$(wildcard $(FAKE_FMDD)/*.c))

all: $(TEST_TARGET)

%.o: %.c $(HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(DAEMON_SRC)/%.o: $(DAEMON_SRC)/%.c $(HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@


test: $(TEST_TARGET) 

$(TEST_TARGET) : $(TEST_OBJECTS) $(OBJECTS) $(HEADERS) $(TEST_HEADERS)
	$(CXX) $(OBJECTS) $(TEST_OBJECTS) -o $(TEST_TARGET) \
	-llog -lunit_test -ltime_utils -lcli -llist -lrskt -pthread \
	-L. -L$(COMMONLIBA) -L$(TOPDIR)/include/libs_a

clean:
	rm -f $(TARGETS) $(TEST_TARGET) $(OBJECTS) $(TEST_OBJECTS)
	rm -f inc/*~ src/*~ *~ *.a

.PHONY: all clean
