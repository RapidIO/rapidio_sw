# Paths to libraries
LIBRDMA_PATH=..
LIBMPORT_PATH=../../libmport
COMMON_LIBS_PATH=../../common/libs_so

level?=$(LOG_LEVEL)

# Build flags and include paths
RDMAAPP_CFLAGS = --std=gnu++0x -Wall -Werror -Wextra -g -ggdb -DRDMA_LL=$(level) \
	 -I../../include -I../../common/include -I$(KINC) \
	-D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS

# Common dependencies
RDMA_APP_DEPS= $(LIBMPORT_PATH)/libmport.so $(LIBRDMA_PATH)/librdma.a ../src/* \
	../inc/* ../daemon/src/* ../daemon/inc/*

# Common build string for all apps
BUILD_STRING = $(CXX) -L$(LIBRDMA_PATH) -L$(LIBMPORT_PATH) -L$(COMMON_LIBS_PATH) -L../../common/libs_so \
	-Wl,-rpath=$(LIBMPORT_PATH) \
	-Wl,-rpath=$(COMMON_LIBS_PATH) \
	-o $@ $(RDMAAPP_CFLAGS) \
	src/$@.c -lrt -lcli -llist -llog -lmport -lrdma -pthread

all: rdma_server rdma_client rdma_user multi_ms_server

# RDMA Server App
rdma_server: src/rdma_server.c $(RDMA_APP_DEPS)
	@echo ---------- Building rdma_server...
	$(BUILD_STRING)

# RDMA User App
rdma_user: src/rdma_user.c $(RDMA_APP_DEPS)
	@echo ---------- Building rdma_user...
	$(BUILD_STRING)

# RDMA Client App
rdma_client: src/rdma_client.c $(RDMA_APP_DEPS)
	@echo ---------- Building rdma_client...
	$(BUILD_STRING)

# RDMA multi memory space server app
multi_ms_server: src/multi_ms_server.c $(RDMA_APP_DEPS)
	@echo ---------- Building multi_ms_server ...
	$(BUILD_STRING)

# RDMA multi memory space client app
multi_ms_client: src/multi_ms_client.c $(RDMA_APP_DEPS)
	@echo ---------- Building multi_ms_client ...
	$(BUILD_STRING)

clean:
	@rm -Rf *.o *~ rdma_server rdma_client rdma_user multi_ms_server
