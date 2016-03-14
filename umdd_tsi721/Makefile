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
COMMONDIR=../common
COMMONINC=$(COMMONDIR)/include
COMMONLIB=$(COMMONDIR)/libs_so
REGSDIR=../fabric_management/librio_switch/inc
LOG_LEVEL?=7
export LOG_LEVEL

CCFLAGS = -O3 -Wall -g -I./inc -I$(COMMONINC) -D$(config) -std=gnu++0x \
	-pthread -DRDMA_LL=7 

CCFLAGS+=-DDEBUG_BD

INCLUDES = inc/worker.h inc/goodput.h inc/goodput_cli.h
SOURCES = src/goodput.c src/goodput_cli.c src/worker.c

# User-mode only flags
# FIXME: Work to remove these as time permits
UMD_FLAGS = -I$(REGSDIR) -DUSER_MODE_DRIVER -Wall \
	 -DRIO_ANY_ADDR=0 -ggdb -O0 -DRDMA_LL=$(LOG_LEVEL)

UMD_INCLUDES = inc/debug.h inc/dmadesc.h inc/local_endian.h inc/mapfile.h \
	inc/mport.h inc/pciebar.h inc/psem.h inc/pshm.h inc/rdtsc.h \
	inc/spinlock.h

UMD_SOURCES = src/mapfile.cc src/mport.cc src/dmachan.cc \
	src/crc32.c \
	src/psem.cc src/pshm.cc

UMDDLIB_SOURCES = src/mapfile.cc src/mport.cc src/dmachan.cc \
		src/crc32.c src/psem.cc src/pshm.cc

LIBRARIES =  -lpthread -lcli -lmport -ltime_utils -llog -lrt

all: umdd umddlib

umdd: $(SOURCES) $(INCLUDES) $(UMD_SOURCES) $(UMD_INCLUDES) 
	$(CXX) $(CCFLAGS) $(UMD_FLAGS) $(SOURCES) $(UMD_SOURCES) -o $@ \
	$(LIBRARIES) -L$(COMMONLIB)

umddlib: $(UMDDLIB_SOURCES) 
	$(CXX) -O3 -Wall -ggdb -I./inc -I../common/include \
        -I../fabric_management/librio_switch/inc \
        -DNODEBUG -std=gnu++0x -pthread \
        -DUSER_MODE_DRIVER -Wall -Wno-format -DRIO_ANY_ADDR=0 \
        -ggdb -O0  \
        src/mapfile.cc src/mport.cc src/dmachan.cc src/crc32.c src/psem.cc src/pshm.cc \
        -fPIC   -shared -Wl,-soname,libUMDd.so.1 -o libUMDd.so.1.0 \
        -lrt -L../common/libs_so -lmport -ltime_utils

clean:
	rm -f *.o umdd libUMDd.so.1.0 *~ src/*~ inc/*~ gmon.out 

FORCE:

logs: FORCE
	rm -f logs/*.log logs/*.res

