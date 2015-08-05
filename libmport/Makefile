CXX ?= $(CROSS_COMPILE)g++
AR=$(CROSS_COMPILE)ar
KDIR?=/home/dems0n16/rio/linux-rapidio
LINUX_INCLUDE_DIR ?= $(KDIR)/include
LOG_LEVEL ?= 4
PREFIX?=/usr
LIBDIR=$(DESTDIR)$(PREFIX)/lib
BINDIR=$(DESTDIR)$(PREFIX)/bin
INCDIR=../include
O?=.

ifdef DEBUG
CFLAGS=-Wall -Wsign-compare -fPIC -DDEBUG -g
else
CFLAGS=-O2 -Wall -Wsign-compare -fPIC -DNDEBUG
endif

ifdef LIBMPORT_SIMULATOR
CFLAGS+=-DLIBMPORT_SIMULATOR
endif

ifdef LIBMPORT_TRACE
CFLAGS+=-DLIBMPORT_TRACE
endif

LIBS=rt	pthread
LDFLAGS?=-static
INCLUDES=$(INCDIR) $(LINUX_INCLUDE_DIR)
LIBPATHS=$(LIBDIR) $(O)

SAMPLES_TARGETS=$(addprefix $(O)/, riodp_test_dma riodp_test_misc \
		riodp_test_client riodp_test_server riodp_test_buf \
		riodp_test_db riodp_test_pw riodp_test_obw riodp_test_devs)
LIB_TARGETS=$(addprefix $(O)/, libmport.a libmport.so)

.PHONY: all clean libs samples

libs: $(LIB_TARGETS)

all: $(LIB_TARGETS) $(SAMPLES_TARGETS)

samples: $(SAMPLES_TARGETS)

libmport.so: src/riodp_mport_lib.o
	$(CXX) -shared -o $@ $^

libmport.a: src/riodp_mport_lib.o
	$(AR) -r $@ $^

$(O)/%: $(O)/samples/%.o libmport.so libmport.a
	$(CXX) $(LDFLAGS) -o $@ $(addprefix -L, $(LIBPATHS)) $< $(addprefix -l, $(LIBS)) libmport.a

$(O)/%: $(O)/src/%.o
	$(CXX) $(LDFLAGS) -o $@ $(addprefix -L, $(LIBPATHS)) $< $(addprefix -l, $(LIBS))

$(O)/%.o: %.c
	$(CXX) -c -o $@ $(addprefix -I, $(INCLUDES)) $(CFLAGS) $<

clean:
	rm -rf $(SAMPLES_TARGETS) $(LIB_TARGETS) $(O)/src/*.o $(O)/samples/*.o
