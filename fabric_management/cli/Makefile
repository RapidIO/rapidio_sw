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
KDIR=/usr/src/linux

LINUX_INCLUDE_DIR ?= $(KDIR)/include

LOG_LEVEL?=6

DEBUG?=DEBUG

TOP_LEVEL = $(shell pwd)

CXX ?= $(CROSS_COMPILE)g++
AR:=$(CROSS_COMPILE)ar

CCFLAGS = -c -g -ggdb -fPIC -Wall -Werror -I. -I.. \
	-I../librio_switch/inc \
	-I../libriocp_pe/inc \
	-I../librio_maint/inc \
	-I../Linux_Include \
	-O2 -I$(LINUX_INCLUDE_DIR) -rdynamic -DIDT_TSI721_SUPPORT

LKFLAGS= -shared -fPIC -g -ggdb

CLI_DEPS= cli_base_init.h cli_cmd_db.h cli_cmd_line.h cli_parse.h \
	dev_db_cli_cmds.h dev_db_rw_cmds.h dev_db_sm.h

CLI_OBJS= cli_base_init.o cli_cmd_db.o cli_cmd_line.o cli_parse.o \
	dev_db_cli_cmds.o dev_db_rw_cmds.o dev_db_sm.o

%.o: %.c $(CLI_DEPS)
	$(CXX) -o $@ $< -c $(CCFLAGS)

clidb_so: $(CLI_OBJS)
	$(CXX) -o libclidb.so $^  -rdynamic $(LKFLAGS)

libclidb.a: clidb_so
	$(AR) rcs $@ $(CLI_OBJS)

all: clidb_so libclidb.a

clean:
	rm -f *.o *~ *.o *.so *.a *.exe core 
