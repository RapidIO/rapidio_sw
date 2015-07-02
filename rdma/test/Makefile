LIBRDMA_PATH=..
LIBMPORT_PATH=../../libmport
COMMON_LIBS_PATH=../../common/libs_a
KDIR?=/usr/src/linux
KINC=$(KDIR)/include

level?=$(LOG_LEVEL)

# Common flags for all applications
RDMAAPP_CFLAGS = --std=gnu++0x -Wall -Werror -Wextra -g -ggdb -DRDMA_LL=$(level) \
		-I../../include -I../../common/include -I$(KINC) \
		-D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS

BAT_DEPS = $(LIBMPORT_PATH)/libmport.so $(LIBRDMA_PATH)/librdma.a ../src/* \
	../inc/* ../daemon/src/* ../daemon/inc/*

BAT_BUILD_STRING = $(CXX) -L$(LIBRDMA_PATH) -L$(LIBMPORT_PATH) -L$(COMMON_LIBS_PATH) \
	-Wl,-rpath=$(LIBMPORT_PATH) \
	-o $@ $(RDMAAPP_CFLAGS) \
	src/$@.cpp -lrt -llog -lmport -lrdma -pthread

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
