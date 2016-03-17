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

TOP_LEVEL?=..
COMMONDIR?=$(TOP_LEVEL)/common
UMDDIR?=$(TOP_LEVEL)/umd_tsi721
COMMONINC=$(COMMONDIR)/include
COMMONLIB=$(COMMONDIR)/libs_so
REGSDIR=../fabric_management/librio_switch/inc
LOG_LEVEL?=7
export LOG_LEVEL

SOVER?=0.42

LIBS_RPATH?=-Wl,-rpath=$(COMMONDIR)/libs_so

CXXFLAGS = -O3 -Wall -g -I./inc -I$(UMDDIR)/inc -I$(COMMONINC) -D$(config) -std=gnu++0x \
	-pthread -DRDMA_LL=7 

CXXFLAGS+=-DDEBUG_BD

INCLUDES = inc/worker.h inc/goodput.h inc/goodput_cli.h
SOURCES = src/goodput.c src/goodput_cli.c src/worker.c

# User-mode only flags
# FIXME: Work to remove these as time permits
UMD_FLAGS = -I$(REGSDIR) -DUSER_MODE_DRIVER -Wall \
	 -DRIO_ANY_ADDR=0 -ggdb -O0 -DRDMA_LL=$(LOG_LEVEL)

UMD_INCLUDES = inc/dmachanshm.h

UMD_SOURCES = src/dmachanshm.cc

UMDDLIB_SOURCES = src/dmachanshm.cc

LIBRARIES =  -lpthread -lcli -lmport -ltime_utils -llog -lrt -lumd

all: umdd umddlib

umdd: $(SOURCES) $(INCLUDES) $(UMD_SOURCES) $(UMD_INCLUDES) 
	$(CXX) $(CXXFLAGS) $(UMD_FLAGS) $(SOURCES) $(UMD_SOURCES) -o $@ \
	$(LIBRARIES) -L$(COMMONLIB) -L$(UMDDIR) $(LIBS_RPATH)

umddlib: $(UMDDLIB_SOURCES) 
	$(CXX) -O3 -Wall -ggdb -pthread -std=gnu++0x \
	-Iinc -I$(COMMONINC)  -I$(UMDDIR)/inc \
	-I$(TOP_LEVEL)/fabric_management/librio_switch/inc \
        -DNODEBUG -DUSER_MODE_DRIVER -Wall -Wno-format -DRIO_ANY_ADDR=0 \
        $(UMDDLIB_SOURCES) \
        -fPIC -shared -Wl,-soname,libUMDd.so.$(SOVER) -o libUMDd.so.$(SOVER) \
        -lrt -L$(COMMONLIB) -L$(UMDDIR) $(LIBS_RPATH) -lmport -ltime_utils -lumd_nolog

clean:
	rm -f *.o umdd libUMDd.so* *~ src/*~ inc/*~ gmon.out 

FORCE:

logs: FORCE
	rm -f logs/*.log logs/*.res

