debug?=NDEBUG
LIBRDMA_PATH=..
LIBMPORT_PATH=../../libmport
LIBLOG_PATH=../../common/liblog

RDMAAPP_CFLAGS = -Wall -Werror -Wextra -g -ggdb -D$(debug) -I../../include -O3

BUILD_STRING = $(CC) -L$(LIBRDMA_PATH) -L$(LIBLOG_PATH) -L$(LIBMPORT_PATH) \
	-Wl,-rpath=$(LIBRDMA_PATH) -Wl,-rpath=$(LIBLOG_PATH) -Wl,-rpath=$(LIBMPORT_PATH) \
	-o $@ $(RDMAAPP_CFLAGS) src/$@.c -lrdma

all: rdma_server rdma_client rdma_user

# RDMA Server App
rdma_server: $(LIBRDMA_PATH)/librdma.so $(LIBLOG_PATH)/liblog.so src/rdma_server.c ../../include/*.h
	@echo ---------- Building rdma_server...
	$(BUILD_STRING)

# RDMA User App
rdma_user: $(LIBRDMA_PATH)/librdma.so $(LIBLOG_PATH)/liblog.so src/rdma_user.c ../../include/*.h
	@echo ---------- Building rdma_user...
	$(BUILD_STRING)

# RDMA Client App
rdma_client: $(LIBRDMA_PATH)/librdma.so $(LIBLOG_PATH)/liblog.so src/rdma_client.c ../../include/*.h
	@echo ---------- Building rdma_client...
	$(BUILD_STRING)

clean:
	@rm -Rf *.o *~ rdma_server rdma_client rdma_user
