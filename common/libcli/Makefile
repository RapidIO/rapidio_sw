# Default configurations
level?=6
debug?=DEBUG

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb

# CLI library flags, sources and objects
CLILIB_CFLAGS = -Wall -std=gnu++0x -c -g -ggdb -fPIC \
		-Iinc/ -Isrc/ -I../../rdma/daemon/inc \
		-D$(debug) -DRDMA_LL=$(level) -O2 -rdynamic -lnsl

CLILIB_SOURCES = src/cli_base_init.c src/cli_cmd_db.c src/cli_cmd_line.c\
		src/cli_parse.c src/cli_console.c

CLILIB_OBJS = cli_base_init.o cli_cmd_db.o cli_cmd_line.o\
		cli_parse.o cli_console.o

# The CLI library
libcli.so: $(CLILIB_SOURCES) src/*.h ../../rdma/daemon/inc/rdmad_console.h
	@echo ---------- Building libcli.so...
	g++ $(CLILIB_CFLAGS) $(CLILIB_SOURCES)
	g++ -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(CLILIB_OBJS) $(SO_LIB_LKFLAGS)

clean:
	@rm -f *.o libcli.so *~
