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

CXX ?= $(CROSS_COMPILE)g++
config?=NODEBUG
COMMONDIR=../../common
COMMONINC=$(COMMONDIR)/include
HACKINC=../../utils/latency/common
COMMONLIB=$(COMMONDIR)/libs_a
REGSDIR=../../fabric_management/librio_switch/inc
LOG_LEVEL?=7
export LOG_LEVEL


# Kernel-mode flags
CCFLAGS = -O3 -Wall -g -I./inc -I$(COMMONINC) -D$(config) -std=gnu++0x \
	-pthread -DRDMA_LL=7

#CCFLAGS+=-pg
CCFLAGS+=-DDEBUG_BD
#CCFLAGS+=-DDEBUG=2
#CCFLAGS+=-DMBOX_TUN_DEBUG
#CCFLAGS+=-DUDMA_TUN_DEBUG
#CCFLAGS+=-DUDMA_TUN_DEBUG_IB
#CCFLAGS+=-DUDMA_TUN_DEBUG_IB_BD
#CCFLAGS+=-DUDMA_TUN_DEBUG_NREAD
#CCFLAGS+=-DUDMA_TUN_DEBUG_IN
CCFLAGS+=-DUDMA_SIM_DEBUG

INCLUDES = inc/time_utils.h inc/worker.h inc/goodput.h inc/goodput_cli.h
SOURCES = src/goodput.c src/goodput_cli.c src/time_utils.c src/worker.c

# User-mode only flags
# FIXME: Work to remove these as time permits
UMD_FLAGS = -I$(REGSDIR) -DUSER_MODE_DRIVER \
	 -Wall -Wno-format -Wno-sign-compare -Wno-write-strings \
	 -DRIO_ANY_ADDR=0 -ggdb -O0 -DRDMA_LL=$(LOG_LEVEL)

UMD_INCLUDES = inc/debug.h inc/dmadesc.h inc/local_endian.h inc/mapfile.h \
	inc/mport.h inc/pciebar.h inc/psem.h inc/pshm.h inc/rdtsc.h \
	inc/spinlock.h

UMD_SOURCES = src/mapfile.cc src/mport.cc src/dmachan.cc src/mboxchan.cc \
	src/crc32.c \
	src/hash.cc src/psem.cc src/pshm.cc \
	src/udma_tun.cc src/tun_ipv4.c \
	src/umbox_afu.cc

LIBRARIES =  -lpthread -lcli -lmport -llog -lrt

all: goodput ugoodput

ugoodput: $(SOURCES) $(INCLUDES) $(UMD_SOURCES) $(UMD_INCLUDES) 
	$(CXX) $(CCFLAGS) -I$(HACKINC) $(UMD_FLAGS) $(SOURCES) $(UMD_SOURCES) -o $@ \
	$(LIBRARIES) -L$(COMMONLIB)

goodput: $(SOURCES) $(INCLUDES)
	$(CXX) $(CCFLAGS) $(SOURCES) -o $@ $(LIBRARIES) -L$(COMMONLIB)

clean:
	rm -f *.o goodput ugoodput *~ src/*~ inc/*~ gmon.out 

FORCE:

logs: FORCE
	rm -f logs/*.log logs/*.res

