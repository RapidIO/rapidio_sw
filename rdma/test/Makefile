LIBRDMA_PATH=..
LIBMPORT_PATH=../../libmport
COMMON_LIBS_PATH=../../common/libs_a
KDIR?=/usr/src/linux
KINC=$(KDIR)/include

# Common flags for all applications
RDMAAPP_CFLAGS = -Wall -Werror -Wextra -g -ggdb -DRDMA_LL=7 -I../../include -I../../common/include -I$(KINC) -O3

BAT_DEPS = $(LIBMPORT_PATH)/libmport.so $(LIBRDMA_PATH)/librdma.a ../src/* ../inc/* ../daemon/src/* ../daemon/inc/*

BAT_BUILD_STRING = $(CXX) --std=gnu++0x \
	-L$(LIBRDMA_PATH) -L$(LIBMPORT_PATH) -L$(COMMON_LIBS_PATH) \
	-Wl,-rpath=$(LIBMPORT_PATH) \
	-o $@ -I$(KERNINC) $(RDMAAPP_CFLAGS) \
	src/$@.cpp -lrt -llog -lmport -lrdma

all: bat_server bat_client

# Batch Automated Test (BAT)
bat_server: $(BAT_DEPS) src/bat_server.cpp
	@echo ---------- Building bat_server...
	$(BAT_BUILD_STRING)

bat_client: $(BAT_DEPS) src/bat_client.cpp
	@echo ---------- Building bat_client...
	$(BAT_BUILD_STRING)

clean:
	@rm -f bat_client bat_server *.o *~
