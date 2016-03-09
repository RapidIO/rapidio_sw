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
#
#
# Default configurations

LOG_LEVEL ?= 1
DEBUG?=DEBUG
CC = $(CROSS_COMPILE)g++
CXX = $(CROSS_COMPILE)g++

TOP_LEVEL ?= ../..

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb
A_LIB_LKFLAGS = -lnsl -fPIC -g -ggdb

CROSS_COMPILE?=/usr/bin/

COMMON_DIR=$(TOP_LEVEL)/common
MPORT_DIR=$(COMMON_DIR)/libmport
TU_DIR=$(COMMON_DIR)/libtime_utils
DAEMON=./

LOCAL_LIBRARY_DIR ?= libs_a

LIB_DIRS?= -L$(COMMON_DIR)/libs_so -L../$(LOCAL_LIBRARY_DIR)
	

CXX ?= $(CROSS_COMPILE)g++
AR ?= $(CROSS_COMPILE)ar

src/%.o: src/%.c
	$(CXX) -o $@ $< -c -g -ggdb -fPIC -Wall -Werror -O2 \
	-I./src -I./inc -I../inc \
	-I$(COMMON_DIR)/include \
	-I$(TOP_LEVEL)/include \
	-I../ \
	-I../librio_switch/inc \
	-I../libriocp_pe/inc \
        -I../libpe_mpdrv/inc \
        -I../libcfg/inc \
	-I../cli \
	-DRDMA_LL=7 -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS

FMD_SOURCES = src/fmd_app_mgmt.c src/fmd.c \
	src/fmd_mgmt_master.c src/fmd_mgmt_slave.c src/fmd_mgmt_cli.c \
	src/fmd_dev_rw_cli.c src/fmd_opts.c 

FMD_OBJECTS = src/fmd_app_mgmt.o src/fmd.o \
	src/fmd_mgmt_master.o src/fmd_mgmt_slave.o src/fmd_mgmt_cli.o \
	src/fmd_dev_rw_cli.o src/fmd_opts.o 
all: fmd

# The Fabric Management Daemon
fmd:  $(FMD_OBJECTS) $(FMD_SOURCES) src/*.h 
	@echo ---------- Building fmd...
	$(CXX) -pthread \
	-o $@ $(FMD_OBJECTS) $(LIB_DIRS) \
	-lcli -ldd -llist -lrt -ldl -ltime_utils -lmport -llog -lriocp_pe \
	-lswitch_api -lcfg -lpe_mpdrv

clean:
	rm -f *.o src/*.o *~ *.exe core fmd
