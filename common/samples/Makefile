debug?=NDEBUG
KDIR?=/usr/src/linux
KINC=$(KDIR)/include
LIBMPORT_PATH=../../libmport
LIBLOG_PATH=../liblog

CM_BUILD_STRING = $(CXX) -L$(LIBMPORT_PATH) -L$(LIBLOG_PATH) \
	-Wl,-rpath=$(LIBMPORT_PATH) -Wl,-rpath=$(LIBLOG_PATH) \
	-I../../common/include -I../../include -I$(KINC) -o $@ $(RDMAD_CFLAGS) \
	 src/$@.cpp -lmport -llog -lrt

all: cm_sock_server cm_sock_client

# Test app for CM socket wrapper class
cm_sock_server: $(LIBMPORT_PATH)/libmport.so $(LIBLOG_PATH)/liblog.so src/cm_sock_server.cpp ../../common/include/*.h
	@echo ---------- Building cm_sock_server ...
	$(CM_BUILD_STRING)

cm_sock_client: $(LIBMPORT_PATH)/libmport.so $(LIBLOG_PATH)/liblog.so src/cm_sock_client.cpp ../../common/include/*.h
	@echo ---------- Building cm_sock_client ...
	$(CM_BUILD_STRING)

clean:
	@rm -f cm_sock_server cm_sock_client *.o *~
