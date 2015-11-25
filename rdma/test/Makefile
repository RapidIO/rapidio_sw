LIBRDMA_PATH=..
LIBMPORT_PATH=../../common/libs_so
COMMON_LIBS_PATH=../../common/libs_a
TOP_LEVEL=../../
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
	-L$(TOP_LEVEL)/fabric_management/libfmdd \
	-L$(TOP_LEVEL)/fabric_management/libriocp_pe \
	-L$(TOP_LEVEL)/fabric_management/libdd \
	-Wl,-rpath=$(LIBMPORT_PATH) \
	-o $@ $(RDMAAPP_CFLAGS) \
	$($@_SRC) -lrt -llist -llog -lmport -lrdma -lcli -pthread -lfmdd -ldd -lriocp_pe

bat_server_SRC = src/bat_server.cpp

bat_client_SRC = src/bat_client.cpp src/bat_client_rdma_api.cpp src/bat_client_test_cases.cpp

bat_child_SRC = src/bat_child.cpp


all: bat_server bat_client bat_child

# Batch Automated Test (BAT)
bat_server: $(BAT_DEPS) $(bat_server_SRC)
	@echo ---------- Building bat_server...
	$(BAT_BUILD_STRING)

bat_client: $(BAT_DEPS) $(bat_client_SRC)
	@echo ---------- Building bat_client...
	$(BAT_BUILD_STRING)

bat_child: $(BAT_DEPS) $(bat_child_SRC)
	@echo ---------- Building bat_child...
	$(BAT_BUILD_STRING)

clean:
	@rm -f bat_client bat_server bat_child *.o *~
