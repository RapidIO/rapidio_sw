level?=7
debug?=DEBUG

AR:=$(CROSS_COMPILE)ar

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb

# LOG library flags, sources and objects
LOGLIB_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -c -g -ggdb -fPIC \
		-D$(debug) -DRDMA_LL=$(level) -O2 -rdynamic -lnsl \
		-I../include \
		-I../../rdma/inc
LOGLIB_SOURCES = src/liblog.cpp src/liblog_cli.c
LOGLIB_OBJS = liblog.o liblog_cli.o

all: liblog.so liblog.a

# The logger library
liblog.so: $(LOGLIB_SOURCES) ../include/liblog.h
	@echo ---------- Building liblog.so...
	$(CXX) $(LOGLIB_CFLAGS) $(LOGLIB_SOURCES)
	$(CXX) -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(LOGLIB_OBJS) $(SO_LIB_LKFLAGS)

%.a: liblog.so
	$(AR) rcs $@ $(LOGLIB_OBJS)

clean:
	@rm -f *.o *~ *.so *.a
