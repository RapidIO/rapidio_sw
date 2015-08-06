LIBRDMA_PATH=..
LIBMPORT_PATH=../../common/libs_so
COMMON_LIBS_PATH=../../common/libs_a

# LOG_LEVEL is set by the master Makefile. If this Makefile is run alone, 
# it can override the loglevel on the command line by specifying 'level=XXX'
# otherwise it defaults to 7
ifndef LOG_LEVEL
level?=7
else
level?=$(LOG_LEVEL)
endif

# Common flags for all applications
RDMAAPP_CFLAGS = --std=gnu++0x -Wall -Werror -Wextra -g -ggdb -DRDMA_LL=$(level) \
		-I../../include -I../../common/include -I$(KINC) \
		-D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS

BAT_DEPS = $(LIBMPORT_PATH)/libmport.so $(LIBRDMA_PATH)/librdma.a ../src/* \
	../inc/* ../daemon/src/* ../daemon/inc/*

BAT_BUILD_STRING = $(CXX) -L$(LIBRDMA_PATH) -L$(LIBMPORT_PATH) -L$(COMMON_LIBS_PATH) \
	-Wl,-rpath=$(LIBMPORT_PATH) \
	-o $@ $(RDMAAPP_CFLAGS) \
	src/$@.cpp -lrt -llog -lmport -lrdma -lcli -pthread

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
