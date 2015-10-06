debug?=NDEBUG
LIBMPORT_PATH=../libmport
LIBLOG_PATH=../liblog
LIBCLI_PATH=../libcli

CM_BUILD_STRING = $(CXX) -L$(LIBMPORT_PATH) -L$(LIBLOG_PATH) -L$(LIBCLI_PATH) \
	-Wl,-rpath=$(LIBMPORT_PATH) -Wl,-rpath=$(LIBLOG_PATH) \
	-Wl,-rpath=$(LIBCLI_PATH) \
	-DRDMA_LL=6 \
	-I../../common/include -I../../include -o $@ $(RDMAD_CFLAGS) \
	 src/$@.cpp -lmport -llog -lcli -lrt

UNIX_BUILD_STRING = $(CXX) -L$(LIBLOG_PATH) -L$(LIBCLI_PATH) \
	-Wl,-rpath=$(LIBLOG_PATH) \
	-Wl,-rpath=$(LIBCLI_PATH) \
	-DRDMA_LL=6 \
	-I../../common/include -I../../include -I../../rdma/inc -o $@ $(RDMAD_CFLAGS) \
	 src/$@.cpp -llog -lcli -lrt -pthread

all: cm_sock_server cm_sock_client unix_sock_client unix_sock_server msg_q1 msg_q2

# Test apps for CM socket wrapper class
cm_sock_server: $(LIBMPORT_PATH)/libmport.so $(LIBLOG_PATH)/liblog.so src/cm_sock_server.cpp ../../common/include/*.h
	@echo ---------- Building cm_sock_server ...
	$(CM_BUILD_STRING)

cm_sock_client: $(LIBMPORT_PATH)/libmport.so $(LIBLOG_PATH)/liblog.so src/cm_sock_client.cpp ../../common/include/*.h
	@echo ---------- Building cm_sock_client ...
	$(CM_BUILD_STRING)

# Test apps for UNIX socket wrapper class
unix_sock_server: $(LIBLOG_PATH)/liblog.so src/unix_sock_server.cpp ../../common/include/*.h
	@echo ---------- Building unix_sock_server ...
	$(UNIX_BUILD_STRING)

unix_sock_client: $(LIBLOG_PATH)/liblog.so src/unix_sock_client.cpp ../../common/include/*.h
	@echo ---------- Building uinx_sock_client ...
	$(UNIX_BUILD_STRING)

msg_q1:	src/msg_q1.cpp
	@echo ---------- Building msg_q1 ...
	$(UNIX_BUILD_STRING)

msg_q2:	src/msg_q1.cpp
	@echo ---------- Building msg_q2 ...
	$(UNIX_BUILD_STRING)

clean:
	@rm -f cm_sock_server cm_sock_client unix_sock_server unix_sock_client msg_q1 msg_q2 *.o *~
