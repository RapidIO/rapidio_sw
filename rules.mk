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
#----------------------------------------------------------------------------*

ifeq (,$(TOPDIR))
$(error ************  ERROR: Please define TOPDIR in your Makefile ************)
endif

CC    =$(CROSS_COMPILE)gcc
CXX   =$(CROSS_COMPILE)g++
AR    =$(CROSS_COMPILE)ar
RANLIB=$(CROSS_COMPILE)ranlib
STRIP =$(CROSS_COMPILE)strip

export CC
export CXX

KERNEL_VERSION := $(shell uname -r)
KERNELDIR = /lib/modules/$(KERNEL_VERSION)/build

ifdef KLOKWORK
KDIR=$(TOPDIR)/include/test
else
KDIR?=/usr/src/linux
endif

ifdef KLOKWORK
RIODIR=$(TOPDIR)/include/test
else
RIO_DIR?=/usr/src/rapidio
endif

DRV_INCLUDE_DIR ?= $(KERNELDIR)/include $(RIO_DIR) $(KDIR)/include

ARCH := $(shell arch)

OPTFLAGS = -ggdb -Ofast -fno-strict-overflow

ifeq ($(ARCH), x86_64)
 OPTFLAGS += -march=native -mfpmath=sse -ffast-math
endif
ifeq ($(ARCH), i686)
 OPTFLAGS += -march=native -mfpmath=sse -ffast-math
endif

ifeq ($(DEBUG), y)
 OPTFLAGS = -ggdb -O0
endif

SOVER?=0.4

HERE := $(shell pwd)

$(info Building $(HERE) on $(ARCH) release $(SOVER) with optimisations $(OPTFLAGS))

LOG_LEVEL?=7
export LOG_LEVEL

COMMONDIR=$(TOPDIR)/common
COMMONINC=$(COMMONDIR)/include
COMMONLIB=$(COMMONDIR)/libs_so
COMMONLIBA=$(COMMONDIR)/libs_a

FMDDIR=$(TOPDIR)/fabric_management

UMDDIR?=$(TOPDIR)/umd_tsi721
UMDINCDIR=$(UMDDIR)/inc

UMDDDIR?=$(TOPDIR)/umdd_tsi721
UMDDINCDIR=$(UMDDDIR)/inc

COMMONFLAGS=$(OPTFLAGS) -pthread -Wall -Werror -I$(TOPDIR)/include -I$(COMMONINC) -L$(COMMONLIB)

CFLAGS+=$(COMMONFLAGS)
CXXFLAGS+=$(COMMONFLAGS) -std=c++11

LIBS_RPATH?=-Wl,-rpath=$(COMMONDIR)/libs_so
