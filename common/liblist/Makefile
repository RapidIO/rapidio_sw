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

level?=7
debug?=DEBUG

NAME:=list

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -Wl,-soname,lib$(NAME).so.$(SOVER)

# LIST library flags, sources and objects
LISTLIB_CFLAGS = $(CXXFLAGS) -Wall -Wextra -fPIC \
		-D$(debug) -DRDMA_LL=$(level) -rdynamic -lnsl \
		-I./inc 

LISTLIB_OBJS = liblist.o liblist_test.o

all: liblist.so liblist.a

# The list library
liblist.so: 
	@echo ---------- Building liblist.so...
	$(CXX) -c $(LISTLIB_CFLAGS) src/liblist.c
	$(CXX) -c $(LISTLIB_CFLAGS) src/liblist_test.c
	$(CXX) -o $@.$(SOVER) $(LISTLIB_OBJS) $(SO_LIB_LKFLAGS)
	@ln -s $@.$(SOVER) $@ &>/dev/null

%.a: liblist.so
	$(AR) rcs $@ $(LISTLIB_OBJS)
clean:
	@rm -f *.o *~ liblist.so* liblist.a
