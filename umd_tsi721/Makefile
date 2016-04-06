#----------------------------------------------------------------------------*
#Copyright (c) 2016, Integrated Device Technology Inc.
#Copyright (c) 2016, RapidIO Trade Association
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
CXX?= $(CROSS_COMPILE)g++
AR?=$(CROSS_COMPILE)ar

config?=NODEBUG
COMMONDIR=../common
LIBMPINCDIR=$(COMMONDIR)/libmport/src
COMMONINC=$(COMMONDIR)/include
COMMONLIB=$(COMMONDIR)/libs_so
REGSDIR=../fabric_management/librio_switch/inc

export LOG_LEVEL

LIBS_RPATH?=-Wl,-rpath=$(COMMONDIR)/libs_so

INCLUDES = inc/debug.h inc/dmachan.h inc/dmadesc.h inc/local_endian.h inc/lockfile.h \
		inc/mapfile.h inc/mboxchan.h inc/mhz.h inc/mport.h inc/pciebar.h \
		inc/psem.h inc/pshm.h inc/rdtsc.h
SOURCES = src/crc32.c src/dmachan.cc src/mapfile.cc src/mboxchan.cc \
	src/mport.cc src/psem.cc src/pshm.cc src/tsi721_dma.c

CXXFLAGS = -I./inc -I$(REGSDIR) -I$(COMMONINC) \
	 -std=gnu++0x -D$(DEBUG) \
	 -Werror -pthread

CXXFLAGS += -ggdb -Ofast -march=native -mfpmath=sse -ffast-math
#CXXFLAGS += -DDEBUG_BD

LIBFLAGS = -shared -fPIC

#CXXFLAGS+=-pg
#CXXFLAGS+=-DDEBUG=2

SYSLIBS = -lrt -lpthread -ldl 
LIBRARIES = -lmport -ltime_utils -lcli

all: libumd.a libumd_nolog.a client server

libumd.a: $(SOURCES) $(INCLUDES)
	@rm -f *.o
	$(CXX) $(CXXFLAGS) -DRDMA_LL=$(LOG_LEVEL) $(SOURCES) \
		-c $(SYSLIBS) -L$(COMMONLIB) $(LIBRARIES) -llog
	$(AR) rcs $@ *.o
	@rm -f *.o

libumd_nolog.a: $(SOURCES) $(INCLUDES)
	@rm -f *.o
	$(CXX) $(CXXFLAGS) $(LIBFLAGS) $(SOURCES) \
		-c $(SYSLIBS) -L$(COMMONLIB) $(LIBRARIES) 
	$(AR) rcs $@ *.o
	@rm -f *.o

client: src/mbox_example.c
	$(CXX) $(CXXFLAGS) \
		-I$(COMMONINC) -Iinc -I$(REGSDIR) -I../include \
		-DRDMA_LL=$(LOG_LEVEL) -DMBOX_CLIENT \
		src/mbox_example.c -o $@ \
		$(SYSLIBS) -L$(COMMONLIB) $(LIBRARIES) $(LIBS_RPATH) -llog -L. -lumd
server: src/mbox_example.c
	$(CXX) $(CXXFLAGS) \
		-I$(COMMONINC) -Iinc -I$(REGSDIR) -I../include \
		-DRDMA_LL=$(LOG_LEVEL) \
		src/mbox_example.c -o $@ \
		$(SYSLIBS) -L$(COMMONLIB) $(LIBRARIES) $(LIBS_RPATH) -llog -L. -lumd


clean:
	rm -f *.o *.a *~ src/*~ inc/*~ gmon.out client server

FORCE:

