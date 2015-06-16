# Paths to libraries
LIBRDMA_PATH=..
LIBMPORT_PATH=../../libmport
COMMON_LIBS_PATH=../../common/libs_a
KDIR?=/usr/src/linux
KINC=$(KDIR)/include

# Build flags and include paths
RDMAAPP_CFLAGS = --std=gnu++0x -Wall -Werror -Wextra -g -ggdb -DRDMA_LL=7 -O3 \
	 -I../../include -I../../common/include -I$(KINC)

# Common dependencies
RDMA_APP_DEPS=$(LIBRDMA_PATH)/librdma.a $(COMMON_LIBS_PATH)/liblog.a \
	$(LIBMPORT_PATH)/libmport.a ../../include/*.h

# Common build string for all apps
BUILD_STRING = $(CXX) -L$(LIBRDMA_PATH) -L$(LIBMPORT_PATH) -L$(COMMON_LIBS_PATH) \
	-Wl,-rpath=$(LIBMPORT_PATH) \
	-o $@ $(RDMAAPP_CFLAGS) src/$@.c -lrt -llog -lmport -lrdma

all: rdma_server rdma_client rdma_user

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

clean:
	@rm -Rf *.o *~ rdma_server rdma_client rdma_user
