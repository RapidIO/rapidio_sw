LIBRDMA_PATH=..
LIBMPORT_PATH=../../libmport
COMMON_LIBS_PATH=../../common/libs_a

RDMAAPP_CFLAGS = -Wall -Werror -Wextra -g -ggdb -DRDMA_LL=7 -I../../include -I../../common/include -I$(KINC) -O3

BUILD_STRING = $(CXX) --std=gnu++0x \
	-L$(LIBRDMA_PATH) -L$(LIBMPORT_PATH) -L$(COMMON_LIBS_PATH) \
	-Wl,-rpath=$(LIBMPORT_PATH) \
	-o $@ -I$(KERNINC) $(RDMAAPP_CFLAGS) \
	src/$@.c -lrt -llog -lmport -lrdma

all: rdma_server rdma_client rdma_user

# RDMA Server App
rdma_server: $(LIBRDMA_PATH)/librdma.a $(COMMON_LIBS_PATH)/liblog.a src/rdma_server.c ../../include/*.h
	@echo ---------- Building rdma_server...
	$(BUILD_STRING)

# RDMA User App
rdma_user: $(LIBRDMA_PATH)/librdma.a $(COMMON_LIBS_PATH)/liblog.a src/rdma_user.c ../../include/*.h
	@echo ---------- Building rdma_user...
	$(BUILD_STRING)

# RDMA Client App
rdma_client: $(LIBRDMA_PATH)/librdma.a $(COMMON_LIBS_PATH)/liblog.a src/rdma_client.c ../../include/*.h
	@echo ---------- Building rdma_client...
	$(BUILD_STRING)

clean:
	@rm -Rf *.o *~ rdma_server rdma_client rdma_user
