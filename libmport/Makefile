CC=$(CROSS_COMPILE)gcc
AR=$(CROSS_COMPILE)ar
KDIR?=/usr/src/linux
PREFIX?=/usr
LIBDIR=$(DESTDIR)$(PREFIX)/lib
BINDIR=$(DESTDIR)$(PREFIX)/bin
INCDIR=$(DESTDIR)$(KDIR)/include
O?=.

ifdef debug
CFLAGS=-Wall -Wsign-compare -fPIC -DDEBUG -g
else
CFLAGS=-O2 -Wall -Wsign-compare -fPIC -DNDEBUG
endif

LIBS=rt	mport pthread
LDFLAGS?=-static
INCLUDES=$(INCDIR)
LIBPATHS=$(LIBDIR) $(O)

TEST_TARGETS=$(addprefix $(O)/, riodp_test_dma riodp_test_misc \
		riodp_test_client riodp_test_server riodp_test_buf \
		riodp_test_db riodp_test_pw riodp_test_obw riodp_test_devs)
LIB_TARGETS=$(addprefix $(O)/, libmport.a libmport.so)

.PHONY: all clean
all: $(LIB_TARGETS) $(TEST_TARGETS)

$(O)/libmport.so: $(O)/test/riodp_mport_lib.o
	$(CC) -shared -o $@ $^

$(O)/libmport.a: $(O)/test/riodp_mport_lib.o
	$(AR) -r $@ $^


$(O)/%: $(O)/test/%.o
	$(CC) $(LDFLAGS) -o $@ $(addprefix -L, $(LIBPATHS)) $< $(addprefix -l, $(LIBS))

$(O)/%.o: %.c
	$(CC) -c -o $@ $(addprefix -I, $(INCLUDES)) $(CFLAGS) $<

clean:
	rm -rf $(TEST_TARGETS) $(LIB_TARGETS) $(O)/test/*.o
