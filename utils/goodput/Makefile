#----------------------------------------------------------------------------*
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
#--------------------------------------------------------------------------***

TOPDIR?=$(shell pwd)/../..
include $(TOPDIR)/rules.mk

DEBUG?=DEBUG

config?=NODEBUG
LIBMPINCDIR=$(COMMONDIR)/libmport/src
REGSDIR=$(FMDDIR)/librio_switch/inc

# Kernel-mode flags
CXXFLAGS += -I./inc  -I$(UMDDIR)/inc -D$(config) \
	-Wall -Wno-sign-compare \
	-pthread -DRDMA_LL=$(LOG_LEVEL) -I$(LIBMPINCDIR) -I$(REGSDIR)

#CXXFLAGS+=-pg
#CXXFLAGS+=-DDEBUG_BD
#CXXFLAGS+=-DDEBUG=2
#CXXFLAGS+=-DMBOX_TUN_DEBUG
#CXXFLAGS+=-DUDMA_TUN_DEBUG
#CXXFLAGS+=-DUDMA_TUN_DEBUG_IB
#CXXFLAGS+=-DUDMA_TUN_DEBUG_IB_BD
#CXXFLAGS+=-DUDMA_TUN_DEBUG_NREAD
#CXXFLAGS+=-DUDMA_TUN_DEBUG_IN
#CXXFLAGS+=-DUDMA_TUN_DEBUG_SPLOCK
#CXXFLAGS+=-DUDMA_TUN_DEBUG_NWRITE_CH2
CXXFLAGS+=-DUDMA_TUN_DEBUG_HISTO
#CXXFLAGS+=-rdynamic # use with mutrace

INCLUDES = inc/worker.h inc/goodput.h inc/goodput_cli.h
SOURCES = src/goodput.c src/goodput_cli.c src/worker.c

# User-mode only flags
UMD_FLAGS = -I$(REGSDIR) -DUSER_MODE_DRIVER \
	 -Wall -Wno-format -Wno-sign-compare -Wno-write-strings \
	 -DRDMA_LL=$(LOG_LEVEL)

UMD_SOURCES = src/udma_tun.cc src/tun_ipv4.c src/umbox_afu.cc src/rdmaopsumd.cc src/httpsrv.cc

LIBRARIES =  -lumd_tsi721 -lmport -ltime_utils -llog -lcli -lpthread -ldl -lrt 

all: goodput ugoodput

scripts:
	@ln -s scripts mport0 &>/dev/null || true

ugoodput: $(SOURCES) $(INCLUDES) $(UMD_SOURCES) 
	$(CXX) $(CXXFLAGS) $(UMD_FLAGS) $(SOURCES) $(UMD_SOURCES) -o $@ \
	-L$(UMDDIR) $(LIBS_RPATH) $(LIBRARIES) 

goodput: $(SOURCES) $(INCLUDES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@ -L$(UMDDIR) $(LIBS_RPATH) $(LIBRARIES)

clean:
	@rm -f *.o goodput ugoodput *~ src/*~ inc/*~ gmon.out *.log

FORCE:

logs: FORCE
	rm -f logs/*.log logs/*.res

