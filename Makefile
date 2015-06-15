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

ifndef KDIR
KDIR=/usr/src/linux
export KDIR
endif

LINUX_INCLUDE_DIR ?= $(KDIR)/include
export LINUX_INCLUDE_DIR

LOG_LEVEL?=4
export LOG_LEVEL

DEBUG?=DEBUG
export DEBUG

CC = $(CROSS_COMPILE)gcc
export CC

CXX = $(CROSS_COMPILE)g++
export CXX

TOP_LEVEL = $(shell pwd)
export TOP_LEVEL

TARGETS = common libmport rdma fabric_management

all: $(TARGETS)

fabric_management: common FORCE
	$(MAKE) all -C fabric_management
		
rdma: common FORCE
	$(MAKE) all -C rdma
		
common: libmport FORCE
	$(MAKE) all -C common
		
libmport:  FORCE
	$(MAKE) all LDFLAGS= -C libmport

FORCE:

clean: FORCE
	$(MAKE) clean -C common
	$(MAKE) clean -C libmport
	$(MAKE) clean -C rdma
	$(MAKE) clean -C fabric_management
