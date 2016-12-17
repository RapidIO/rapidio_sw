#----------------------------------------------------------------------------*
#Copyright (c) 2016, Integrated Device Technology Inc.
#Copyright (c) 2016, RapidIO Trade Association
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
#specific prior written permisson.
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
#----------------------------------------------------------------------------*

ifeq (,$(TOPDIR))
$(error ************  ERROR: Please define TOPDIR in your Makefile ************)
endif

CC    =$(CROSS_COMPILE)gcc
CXX   =$(CROSS_COMPILE)g++
AR    =$(CROSS_COMPILE)ar

export CC
export CXX

ifdef KLOKWORK
KDIR=$(TOPDIR)/include/test
RIODIR=$(TOPDIR)/include/test
else
KDIR?=/usr/src/rapidio
RIODIR?=/usr/src/rapidio
endif

ARCH := $(shell arch)

OPTFLAGS = -ggdb -Ofast -fno-strict-overflow

ifeq ($(ARCH), x86_64)
 OPTFLAGS += -march=native -mfpmath=sse -ffast-math
endif
ifeq ($(ARCH), i686)
 OPTFLAGS += -march=native -mfpmath=sse -ffast-math
endif

## by default, debug logging is enabled
LOG_LEVEL?=7

## by default, debug is disabled 
## [export DEBUG_CTL="DEBUG=3" | export DEBUG_CTL=DEBUG]
DEBUG_CTL?=NDEBUG

ifneq ($(DEBUG_CTL), NDEBUG)
 OPTFLAGS = -ggdb -Og
endif

SOVER?=0.6

HERE := $(shell pwd)

$(info Building $(HERE) on $(ARCH) release $(SOVER) with optimisations $(OPTFLAGS))

COMMONDIR=$(TOPDIR)/common
COMMONINC=$(COMMONDIR)/include
COMMONLIB=$(COMMONDIR)/libs_so
COMMONLIBA=$(COMMONDIR)/libs_a

FMDDIR=$(TOPDIR)/fabric_management

STD_FLAGS=$(OPTFLAGS) -pthread -Wall -Wextra -Werror -fPIC
STD_FLAGS+=-I$(TOPDIR)/include -I$(COMMONINC) -I. -I./inc
STD_FLAGS+=-L$(COMMONLIB)
ifdef TEST
STD_FLAGS+=-DUNIT_TESTING
endif

CFLAGS+=$(STD_FLAGS)
CXXFLAGS+=$(STD_FLAGS) -std=c++11

ifdef TEST
TST_LIBS=-lcmocka
TST_INCS=-I$(COMMONDIR)/libcmocka/inc
endif

LIBS_RPATH?=-Wl,-rpath=$(COMMONDIR)/libs_so
