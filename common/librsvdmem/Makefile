#----------------------------------------------------------------------------*
#Copyright (c) 2015, Integrated Device Technology Inc.
#Copyright (c) 2015, RapidIO Trade Association
#All rights reserved.
#
##Redistribution and use in source and binary forms, with or without modification,
#are permitted provided that the following conditions are met:
#
##1. Redistributions of source code must retain the above copyright notice, this
#list of conditions and the following disclaimer.
#
##2. Redistributions in binary form must reproduce the above copyright notice,
#this list of conditions and the following disclaimer in the documentation
#and/or other materials provided with the distribution.
#
##3. Neither the name of the copyright holder nor the names of its contributors
#may be used to endorse or promote products derived from this software without
#specific prior written permission.
#
##THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
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

level=$(LOG_LEVEL)

NAME:=rsvdmem

TARGETS:=lib$(NAME).a lib$(NAME).so
TEST_TARGET:=$(NAME)_test
HEADERS:=inc/librsvdmem_private.h $(COMMONINC)/librsvdmem.h
TEST_HEADERS:=

CXXFLAGS+=$(addprefix -W,all extra) -fPIC
CXXFLAGS+=-D$(DEBUG)
CXXFLAGS+=-I. -Iinc -Isrc
CXXFLAGS+=-DRDMA_LL=$(level)

SO_LINK_FLAGS+= -rdynamic -shared -Wl,-soname,lib$(NAME).so.$(SOVER) -lnsl -fPIC -g -ggdb

OBJECTS:=$(patsubst src/%.c,src/%.o,$(wildcard src/*.c))

TEST_OBJECTS:=$(patsubst test/%.c,test/%.o,$(wildcard test/*.c))

all: $(TARGETS) $(TEST_TARGET)

%.a: $(OBJECTS)
	$(AR) rcs $@ $^

%.so: $(OBJECTS)
	$(CXX) -o $@.$(SOVER) $(OBJECTS) $(SO_LINK_FLAGS)
	@ln -s $@.$(SOVER) $@ &>/dev/null

src/%.o: src/%.c $(HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

test/%.o: test/%.c $(HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(TEST_TARGET) :  $(HEADERS) $(TEST_HEADERS) $(OBJECTS) $(TEST_OBJECTS)
	$(CXX) $(OBJECTS) $(TEST_OBJECTS) -o $(TEST_TARGET) \
	-L. -L$(COMMONLIBA) \
	-pthread -lcli 

clean:
	rm -f $(TARGETS) $(TEST_TARGET) $(OBJECTS) $(TEST_OBJECTS) \
	 inc/*~ src/*~ *~ *.a *.so*

.PHONY: all clean
