# #All rights reserved.
# #
# ##Redistribution and use in source and binary forms, with or without modification,
# #are permitted provided that the following conditions are met:
# #
# ##1. Redistributions of source code must retain the above copyright notice, this
# #list of conditions and the following disclaimer.
# #
# ##2. Redistributions in binary form must reproduce the above copyright notice,
# #this list of conditions and the following disclaimer in the documentation
# #and/or other materials provided with the distribution.
# #
# ##3. Neither the name of the copyright holder nor the names of its contributors
# #may be used to endorse or promote products derived from this software without
# #specific prior written permission.
# #
# ##THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# #ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# #WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# #DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# #FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# #DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# #SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# #CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# #OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# #OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# #--------------------------------------------------------------------------***
# Read values from ones exported in master Makefile. Or, override on the command
# line if building separately.
level?=$(LOG_LEVEL)
debug?=$(DEBUG)

CC?=$(CROSS_COMPILE)g++
CXX?=$(CROSS_COMPILE)g++

AR:=$(CROSS_COMPILE)ar

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb

# CLI library flags, sources and objects
CLILIB_CFLAGS = -Wall -std=gnu++0x -c -g -ggdb -fPIC \
		-I../include -Isrc/ -I../../rdma/daemon/inc \
		-D$(debug) -DRDMA_LL=$(level) -O2 -rdynamic -lnsl

CLILIB_SOURCES = src/cli_base_init.c src/cli_cmd_db.c src/cli_cmd_line.c\
		src/cli_parse.c src/cli_console.c

CLILIB_OBJS = cli_base_init.o cli_cmd_db.o cli_cmd_line.o\
		cli_parse.o cli_console.o

all: libcli.so libcli.a

# The CLI library
libcli.so: $(CLILIB_SOURCES) src/*.h ../../rdma/daemon/inc/*_console.h
	@echo ---------- Building libcli.so...
	$(CXX) $(CLILIB_CFLAGS) $(CLILIB_SOURCES)
	$(CXX) -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(CLILIB_OBJS) $(SO_LIB_LKFLAGS)

%.a: libcli.so
	$(AR) rcs $@ $(CLILIB_OBJS)

clean:
	@rm -f *.o libcli.so *~ libcli.a
