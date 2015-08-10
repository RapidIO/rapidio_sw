# Paths to libraries
LIBRDMA_PATH=..
COMMON_LIBS_PATH=../../common/libs_so
TOP_LEVEL=../../
level?=$(LOG_LEVEL)

# Build flags and include paths
RDMAAPP_CFLAGS = --std=gnu++0x -Wall -Werror -Wextra -g -ggdb -DRDMA_LL=$(level) \
	 -I../../include -I../../common/include -I$(KINC) -I. \
	-D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS

# Common dependencies
RDMA_APP_DEPS= $(LIBRDMA_PATH)/librdma.a ../src/* \
	../inc/* ../daemon/src/* ../daemon/inc/*

# Common build strings
# For C apps
BUILD_STRING_C = $(CXX) -L$(LIBRDMA_PATH) -L$(COMMON_LIBS_PATH) \
	-L$(TOP_LEVEL)/fabric_management/libfmdd \
	-L$(TOP_LEVEL)/fabric_management/libriocp_pe \
	-L$(TOP_LEVEL)/fabric_management/libdd \
	-Wl,-rpath=$(COMMON_LIBS_PATH) \
	-o $@ $(RDMAAPP_CFLAGS) \
	src/$@.c -lrt -lcli -llist -llog -lmport -lrdma -pthread -lfmdd -ldd -lriocp_pe

# For C++ apps
BUILD_STRING_CPP = $(CXX) -L$(LIBRDMA_PATH) -L$(COMMON_LIBS_PATH) -L../../common/libs_so \
	-L$(TOP_LEVEL)/fabric_management/libfmdd \
	-L$(TOP_LEVEL)/fabric_management/libriocp_pe \
	-L$(TOP_LEVEL)/fabric_management/libdd \
	-Wl,-rpath=$(COMMON_LIBS_PATH) \
	-o $@ $(RDMAAPP_CFLAGS) \
	src/$@.cpp -lrt -lcli -llist -llog -lmport -lrdma -pthread -lfmdd -ldd -lriocp_pe

all: rdma_server rdma_client rdma_user multi_ms_server multi_ms_client create_and_die

# RDMA Server App
rdma_server: src/rdma_server.c $(RDMA_APP_DEPS)
	@echo ---------- Building rdma_server...
	$(BUILD_STRING_C)

# RDMA User App
rdma_user: src/rdma_user.c $(RDMA_APP_DEPS)
	@echo ---------- Building rdma_user...
	$(BUILD_STRING_C)

# RDMA Client App
rdma_client: src/rdma_client.c $(RDMA_APP_DEPS)
	@echo ---------- Building rdma_client...
	$(BUILD_STRING_C)

# RDMA multi memory space server app
multi_ms_server: src/multi_ms_server.cpp $(RDMA_APP_DEPS)
	@echo ---------- Building multi_ms_server ...
	$(BUILD_STRING_CPP)

# RDMA multi memory space client app
multi_ms_client: src/multi_ms_client.cpp $(RDMA_APP_DEPS)
	@echo ---------- Building multi_ms_client ...
	$(BUILD_STRING_CPP)

# Tests auto-cleanup by the daemon of msos, ms's and msubs created by an app
# that later dies
create_and_die:	src/create_and_die.cpp $(RDMA_APP_DEPS)
	@echo ---------- Building create_and_die
	$(BUILD_STRING_CPP)

clean:
	@rm -Rf *.o *~ rdma_server rdma_client rdma_user multi_ms_server multi_ms_client create_and_die
