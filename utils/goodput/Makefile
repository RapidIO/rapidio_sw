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

LOG_LEVEL?=7
DEBUG?=DEBUG
CC?= $(CROSS_COMPILE)g++
CXX?= $(CROSS_COMPILE)g++

ARCH := $(shell arch)

config?=NODEBUG
COMMONDIR=../../common
UMDDIR=../../umd_tsi721
LIBMPINCDIR=$(COMMONDIR)/libmport/src
COMMONINC=$(COMMONDIR)/include
COMMONLIB=-Wl,-rpath=$(COMMONDIR)/libs_so -L$(COMMONDIR)/libs_so
REGSDIR=../../fabric_management/librio_switch/inc
LOG_LEVEL?=7
export LOG_LEVEL


# Kernel-mode flags
CCFLAGS = -I./inc  -I$(UMDDIR)/inc -I$(COMMONINC) -D$(config) \
	-std=gnu++0x \
	-Wall -Wno-sign-compare \
	-pthread -DRDMA_LL=7 -I$(LIBMPINCDIR) -I$(REGSDIR)

#CCFLAGS+=-pg
#CCFLAGS+=-DDEBUG_BD
#CCFLAGS+=-DDEBUG=2
#CCFLAGS+=-DMBOX_TUN_DEBUG
#CCFLAGS+=-DUDMA_TUN_DEBUG
#CCFLAGS+=-DUDMA_TUN_DEBUG_IB
#CCFLAGS+=-DUDMA_TUN_DEBUG_IB_BD
#CCFLAGS+=-DUDMA_TUN_DEBUG_NREAD
#CCFLAGS+=-DUDMA_TUN_DEBUG_IN
#CCFLAGS+=-DUDMA_TUN_DEBUG_SPLOCK
#CCFLAGS+=-DUDMA_TUN_DEBUG_NWRITE_CH2
CCFLAGS+=-DUDMA_TUN_DEBUG_HISTO
#CCFLAGS+=-rdynamic # use with mutrace

CCFLAGS+=-ggdb -Ofast

ifeq ($(ARCH), x86_64)
 CXXFLAGS += -march=native -mfpmath=sse -ffast-math
endif
ifeq ($(ARCH), i686)
 CXXFLAGS += -march=native -mfpmath=sse -ffast-math
endif

INCLUDES = inc/worker.h inc/goodput.h inc/goodput_cli.h
SOURCES = src/goodput.c src/goodput_cli.c src/worker.c

# User-mode only flags
UMD_FLAGS = -I$(REGSDIR) -DUSER_MODE_DRIVER \
	 -Wall -Wno-format -Wno-sign-compare -Wno-write-strings \
	 -ggdb -DRDMA_LL=$(LOG_LEVEL)
	 #-DRIO_ANY_ADDR=0


UMD_SOURCES = src/udma_tun.cc src/tun_ipv4.c src/umbox_afu.cc src/rdmaopsumd.cc src/httpsrv.cc

LIBRARIES =  -lpthread -ldl -lmport -lcli -ltime_utils -llog -lrt -lumd

all: goodput ugoodput

ugoodput: $(SOURCES) $(INCLUDES) $(UMD_SOURCES) 
	$(CXX) $(CCFLAGS) $(UMD_FLAGS) $(SOURCES) $(UMD_SOURCES) -o $@ \
	$(COMMONLIB) -L$(UMDDIR) $(LIBRARIES) 
	@ln -s scripts mport0 &>/dev/null || true

goodput: $(SOURCES) $(INCLUDES)
	$(CXX) $(CCFLAGS) $(SOURCES) -o $@ $(COMMONLIB) -L$(UMDDIR) $(LIBRARIES)

clean:
	@rm -f *.o goodput ugoodput *~ src/*~ inc/*~ gmon.out *.log

FORCE:

logs: FORCE
	rm -f logs/*.log logs/*.res

