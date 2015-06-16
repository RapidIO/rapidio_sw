# Read values from ones exported in master Makefile. Or, override on the command
# line if building separately.
level?=$(LOG_LEVEL)
debug?=$(DEBUG)

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
