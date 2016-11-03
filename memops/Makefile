#----------------------------------------------------------------------------*
#Copyright (c) 2016, Integrated Device Technology Inc.
#Copyright (c) 2016, RapidIO Trade Association
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

TOPDIR=..
include $(TOPDIR)/rules.mk

.PHONY: all clean

INCLUDES=-Iinc -I$(TOPDIR)/include -I$(COMMONINC)
INCLUDES+=-I$(UMDINCDIR) -I$(UMDDINCDIR) 
INCLUDES+=-I$(FMDDIR)/libdid/inc -I$(FMDDIR)/libct/inc
INCLUDES+=-I$(FMDDIR)/librio_switch/inc

SOURCES = src/memops_factory.cc src/memops_mport.cc src/dmachanshm_dl.c src/memops_umdd.cc src/memops_umd.cc src/dmachanmgr.cc

all: libmemops_intf.so libmemops_intf.a memops_demo memops_demo_log memops_test_cf

LFLAGS = -std=gnu++0x -DDMACHAN_TICKETED $(STD_FLAGS)

libmemops_intf.a: $(SOURCES)
	$(CXX) $(LFLAGS) -c $(INCLUDES) -I$(UMDDINCDIR) $^ 
	$(AR) rcs $@ *.o

libmemops_intf.so: $(SOURCES)
	$(CXX) $(LFLAGS) $(INCLUDES) -shared -fPIC $^  -o $@.$(SOVER) \
		-Wl,-soname,$@.$(SOVER) \
		\
		-L$(COMMONLIB) $(LIBS_RPATH) -ltime_utils -lmport \
		$(COMMONLIBA)/libumd_tsi721_nolog_tik.a -lrt -ldl 
	@ln -s $@.$(SOVER) $@ &>/dev/null || true

## libmemops_intf.so is the version without logging and debugging.
## The .so library must be bound into memops_demo.
memops_demo: $(SOURCES) libmemops_intf.so samples/memops_demo.cc
	$(CXX) $(LFLAGS) $(INCLUDES) samples/memops_demo.cc \
		-o $@ \
		-lpthread -lrt -ldl \
		-L$(COMMONLIB) $(LIBS_RPATH) -L. -Wl,-rpath=. \
		-ltime_utils -lmport -lmemops_intf -lrt -llog -lcli

## libmemops_intf.a is the version without logging and debugging.
## The .a library must be bound into memops_demo_log.
memops_demo_log: $(SOURCES) libmemops_intf.a samples/memops_demo.cc
	$(CXX) $(LFLAGS) $(INCLUDES) samples/memops_demo.cc \
 		-o $@ \
		-DRDMA_LL=$(LOG_LEVEL) \
		-L$(COMMONLIB) -L. $(LIBS_RPATH) -Wl,-rpath=. \
		-L$(COMMONLIBA) libmemops_intf.a -lumd_tsi721_tik \
		-ltime_utils -lmport -llog -lcli \
		-lpthread -lrt -ldl 

memops_test_cf: $(SOURCES) libmemops_intf.so src/memops_test_cf.cc
	$(CXX) $(LFLAGS) src/memops_test_cf.cc $(INCLUDES) \
		-o $@ \
		-L$(COMMONLIBA) -lmemops_intf -L. -Wl,-rpath=. \
		-L$(COMMONLIB) $(LIBS_RPATH) \
		-ltime_utils -lmport -llog -lcli  \
		-lpthread -lrt -ldl

clean:
	rm -f *.[ao] *.so* a.out memops_demo*
