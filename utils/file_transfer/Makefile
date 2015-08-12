# Copyright (c) 2015, Integrated Device Technology
#

export LOCAL_LIBRARY_DIR=libs_a
COMMON_LIBS_DIR?=../../common/libs_a

CC= gcc
CCFLAGS= -c -g -ggdb -fPIC -Wall -Werror -O2 -I. \
	-I../../common/include -I./file_xfer_lib/inc -I./libcdev/inc \

REMDBG_DEPS=

REMDBG_OBJS= remdbg.o

CLIENT_DEPS=

CLIENT_OBJS= client.o

SERVER_DEPS=

SERVER_OBJS= server.o

LIB_DEPS= libfxfr
.PHONY: $(LIB_DEPS)


%.o: %.c $(SERVER_DEPS) $(LIB_DEPS)
	$(CC) -o $@ $< $(CCFLAGS) -I./libfxfr/inc

libfxfr:
	$(MAKE) all -C libfxfr
	mkdir -p $(LOCAL_LIBRARY_DIR)
	cp libfxfr/libfxfr.a $(LOCAL_LIBRARY_DIR)
	

server: $(SERVER_OBJS) 
	gcc -o server $^ -pthread -L$(LOCAL_LIBRARY_DIR) -L$(COMMON_LIBS_DIR) \
	-lfxfr -lcli -lmport -lrt -D__CLI_LINUX__ -I./libfxfr/inc

client: $(CLIENT_OBJS) 
	gcc -o rftp $^ -pthread -L$(LOCAL_LIBRARY_DIR) -L$(COMMON_LIBS_DIR) \
	-lfxfr -lcli -lmport -lrt -D__CLI_LINUX__ -I./libfxfr/inc

remdbg: $(REMDBG_OBJS)
	gcc -o remdbg $^ -pthread -L$(LOCAL_LIBRARY_DIR) -L$(COMMON_LIBS_DIR) \
	-lfxfr -lcli -lrt -D__CLI_LINUX__ 

all: $(LIB_DEPS) server client remdbg

clean:
	rm -f *.o *~ *.exe core $(LOCAL_LIBRARY_DIR)/*.a fxfr rftp server remdbg
	$(MAKE) clean -C libfxfr
