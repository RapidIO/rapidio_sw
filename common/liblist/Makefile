level?=7
debug?=DEBUG

AR:=$(CROSS_COMPILE)ar

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb

# LIST library flags, sources and objects
LISTLIB_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -c -g -ggdb -fPIC \
		-D$(debug) -DRDMA_LL=$(level) -O2 -rdynamic -lnsl \
		-I./inc \
		-I../include
LISTLIB_OBJS = liblist.o liblist_test.o

all: liblist.so liblist.a

# The list library
liblist.so: 
	@echo ---------- Building liblist.so...
	g++ $(LISTLIB_CFLAGS) src/liblist.c
	g++ $(LISTLIB_CFLAGS) src/liblist_test.c
	g++ -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(LISTLIB_OBJS) $(SO_LIB_LKFLAGS)

%.a: 
	$(AR) rcs $@ $(LISTLIB_OBJS)
clean:
	@rm -f *.o *~ liblist.so liblist.a
