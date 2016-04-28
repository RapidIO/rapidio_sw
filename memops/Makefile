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

INCLUDES = -Iinc -I$(TOPDIR)/include -I$(COMMONINC) -I$(UMDINCDIR) -I$(FMDDIR)/librio_switch/inc

SOURCES = src/memops_factory.cc src/memops_mport.cc src/dmachanshm_dl.c src/memops_umdd.cc src/dmachanmgr.cc

all: libmemops_intf.so libmemops_intf.a

libmemops_intf.a: $(SOURCES)
	$(CXX) -c $(INCLUDES) -I$(UMDDINCDIR) $^ -L$(COMMONLIB) -lmport $(LIBS_RPATH)
	$(AR) rcs $@ *.o

libmemops_intf.so: $(SOURCES)
	$(CXX) $(INCLUDES) $^ \
		-shared -fPIC -ldl -L$(COMMONLIB) -lmport $(LIBS_RPATH) -Wl,-soname,$@.$(SOVER) -o $@.$(SOVER)
	@ln -s $@.$(SOVER) $@ &>/dev/null || true

clean:
	rm -f *.[ao] *.so*
