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
LOGLIB_SOURCES = src/rdma_logger.cpp
LOGLIB_OBJS = rdma_logger.o

all: liblog.so liblog.a

# The logger library
liblog.so: $(LOGLIB_SOURCES) ../include/rdma_logger.h
	@echo ---------- Building liblog.so...
	$(CXX) $(LOGLIB_CFLAGS) $(LOGLIB_SOURCES)
	$(CXX) -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(LOGLIB_OBJS) $(SO_LIB_LKFLAGS)

%.a: 
	$(AR) rcs $@ $(LOGLIB_OBJS)

clean:
	@rm -f *.o *~ *.so *.a
