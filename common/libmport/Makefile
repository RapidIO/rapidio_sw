#----------------------------------------------------------------------------*
#Copyright (c) 2015, Integrated Device Technology Inc.
#Copyright (c) 2015, RapidIO Trade Association
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

TOPDIR?=$(shell pwd)/../..
include $(TOPDIR)/rules.mk

LOG_LEVEL ?= 4
PREFIX?=/usr
LIBDIR=$(DESTDIR)$(PREFIX)/lib
BINDIR=$(DESTDIR)$(PREFIX)/bin
INCDIR=$(COMMONINC)
O?=.

ifdef DEBUG
CXXFLAGS +=-Wall -Wno-sign-compare -fPIC # -DMPORT_DEBUG
else
CXXFLAGS += -Wall -Wno-sign-compare -fPIC -DNDEBUG
endif

CXXFLAGS += -I$(FMDDIR)/librio_switch/inc
CXXFLAGS += -I$(UMDINCDIR)

LIBS=rt	pthread dl
LDFLAGS+= -L$(COMMONLIB) $(LIBS_RPATH)
INCLUDES=$(INCDIR) $(RIO_DIR)

LIBPATHS=$(LIBDIR) $(O) $(COMMONLIB)

SAMPLES_TARGETS=$(addprefix $(O)/, riodp_test_dma riodp_test_misc \
		riodp_test_client riodp_test_server riodp_test_buf \
		riodp_test_db riodp_test_pw riodp_test_obw riodp_test_devs)
LIB_TARGETS=$(addprefix $(O)/, libmport.a libmport.so)

.PHONY: all clean libs samples

all: $(LIB_TARGETS)

libs: $(LIB_TARGETS)

samples: $(SAMPLES_TARGETS)

libmport.so: src/riodp_mport_lib.o src/dmachanshm_dl.o
	$(CXX) -shared -ldl -L$(COMMONLIB) -ltime_utils $(LIBS_RPATH) -Wl,-soname,$@.$(SOVER) -o $@.$(SOVER) $^
	@ln -s $@.$(SOVER) $@ &>/dev/null || true

libmport.a: src/riodp_mport_lib.o src/dmachanshm_dl.o
	$(AR) -r $@ $^

umdmporttest: src/riodp_mport_lib.c src/dmachanshm_dl.c
	$(CXX) -DTEST_UMDD \
		$(addprefix -I, $(INCLUDES)) -I$(FMDDIR)/librio_switch/inc -I$(UMDINCDIR) \
		-Wall -Wno-sign-compare \
		-ldl -L$(COMMONLIBA) -ltime_utils -o $@ $^

$(O)/%: $(O)/samples/%.o libmport.so libmport.a
	$(CXX) $(LDFLAGS) -o $@ $(addprefix -L, $(LIBPATHS)) $< $(addprefix -l, $(LIBS)) $(LIBS_RPATH) -ltime_utils -lmport

$(O)/%: $(O)/src/%.o
	$(CXX) $(LDFLAGS) -o $@ $(addprefix -L, $(LIBPATHS)) $< $(addprefix -l, $(LIBS)) $(LIBS_RPATH) -ltime_utils -lmport

$(O)/%.o: %.c
	$(CXX) -c -o $@ $(addprefix -I, $(INCLUDES)) $(CXXFLAGS) $(LIBS_RPATH) $<

clean:
	rm -rf $(SAMPLES_TARGETS) $(LIB_TARGETS) $(O)/src/*.o $(O)/samples/*.o libmport.so* umdmporttest
