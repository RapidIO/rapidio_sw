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

TOPDIR=..
include $(TOPDIR)/rules.mk

DEBUG?=DEBUG
config?=NODEBUG

LIBMPINCDIR=$(COMMONDIR)/libmport/src
REGSDIR=$(FMDDIR)/librio_switch/inc

INCLUDES = inc/debug.h inc/dmachan.h inc/dmadesc.h inc/local_endian.h inc/lockfile.h \
		inc/mapfile.h inc/mboxchan.h inc/mhz.h inc/mport.h inc/pciebar.h \
		inc/psem.h inc/pshm.h inc/rdtsc.h
SOURCES = src/crc32.c src/dmachan.cc src/mapfile.cc src/mboxchan.cc \
	src/mport.cc src/psem.cc src/pshm.cc src/tsi721_dma.c \
	src/chanlock.cc

CXXFLAGS += -I./inc -I$(REGSDIR) -I$(COMMONINC) \
	 -D$(DEBUG) \
	 -pthread

#CXXFLAGS += -DDEBUG_BD

LIBFLAGS = -shared -fPIC

#CXXFLAGS+=-pg
#CXXFLAGS+=-DDEBUG=2

SYSLIBS = -lrt -lpthread -ldl 
LIBRARIES = -lmport -ltime_utils -lcli

all: libumd_tsi721.a libumd_tsi721_nolog.a libumd_tsi721_nolog_tik.a mbox_client mbox_server cmsock_ex

libumd_tsi721.a: $(SOURCES) $(INCLUDES)
	@rm -f *.o
	$(CXX) $(CXXFLAGS) -DRDMA_LL=$(LOG_LEVEL) $(SOURCES) \
		-c $(SYSLIBS) $(LIBRARIES) -llog
	$(AR) rcs $@ *.o
	cp -v $@ $(COMMONDIR)/libs_a
	@rm -f *.o

libumd_tsi721_nolog.a: $(SOURCES) $(INCLUDES)
	@rm -f *.o
	$(CXX) $(CXXFLAGS) $(LIBFLAGS) $(SOURCES) \
		-c $(SYSLIBS) $(LIBRARIES) 
	$(AR) rcs $@ *.o
	cp -v $@ $(COMMONDIR)/libs_a
	@rm -f *.o

libumd_tsi721_nolog_tik.a: $(SOURCES) $(INCLUDES)
	@rm -f *.o
	$(CXX) $(CXXFLAGS) -DDHACHAN_TICKETED -I$(UMDDINCDIR) $(LIBFLAGS) $(SOURCES) \
		-c $(SYSLIBS) $(LIBRARIES) 
	$(AR) rcs $@ *.o
	cp -v $@ $(COMMONDIR)/libs_a
	@rm -f *.o

mbox_client: test/mbox_example.c libumd_tsi721.a
	$(CXX) $(CXXFLAGS) \
		-Iinc -I$(REGSDIR) -I../include \
		-DRDMA_LL=$(LOG_LEVEL) -DMBOX_CLIENT \
		$^ -o $@ \
		$(SYSLIBS) $(LIBRARIES) $(LIBS_RPATH) -llog -L. -lumd_tsi721

mbox_server: test/mbox_example.c libumd_tsi721.a
	$(CXX) $(CXXFLAGS) \
		-Iinc -I$(REGSDIR) -I../include \
		-DRDMA_LL=$(LOG_LEVEL) \
		$^ -o $@ \
		$(SYSLIBS) $(LIBRARIES) $(LIBS_RPATH) -llog -L. -lumd_tsi721

cmsock_ex: test/cmsock_example.cc
	$(CXX) $(CXXFLAGS) \
		-I../include \
		$^ -o $@ \
		$(SYSLIBS) $(LIBRARIES) $(LIBS_RPATH) -llog 

clean:
	rm -f *.o *.a *~ src/*~ inc/*~ gmon.out mbox_client mbox_server cmsock_ex

FORCE:

