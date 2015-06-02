level?=7
debug?=DEBUG

# Common link flags for all shared libraries
SO_LIB_LKFLAGS = -rdynamic -shared -lnsl -fPIC -g -ggdb

# LIST library flags, sources and objects
LISTLIB_CFLAGS = -Wall -Werror -Wextra -std=gnu++0x -c -g -ggdb -fPIC \
		-D$(debug) -DRDMA_LL=$(level) -O2 -rdynamic -lnsl \
		-I./inc 
LISTLIB_SOURCES = src/liblist.c
LISTLIB_OBJS = liblist.o

# The listger library
liblist.so: $(LISTLIB_SOURCES) 
	@echo ---------- Building liblist.so...
	g++ $(LISTLIB_CFLAGS) $(LISTLIB_SOURCES)
	g++ -L$(shell pwd) -Wl,-rpath=$(shell pwd) -o $@ $(LISTLIB_OBJS) $(SO_LIB_LKFLAGS)

clean:
	@rm -f *.o *~ liblist.so
