level?=7
debug?=DEBUG

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb

# LOG library flags, sources and objects
LOGLIB_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -c -g -ggdb -fPIC \
		-D$(debug) -DRDMA_LL=$(level) -O2 -rdynamic -lnsl \
		-I../include \
		-I../../rdma/inc
LOGLIB_SOURCES = src/rdma_logger.cpp
LOGLIB_OBJS = rdma_logger.o

# The logger library
liblog.so: $(LOGLIB_SOURCES) ../../rdma/inc/rdma_logger.h
	@echo ---------- Building liblog.so...
	g++ $(LOGLIB_CFLAGS) $(LOGLIB_SOURCES)
	g++ -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(LOGLIB_OBJS) $(SO_LIB_LKFLAGS)

all: liblog.so
clean:
	@rm -f *.o *~ liblog.so
