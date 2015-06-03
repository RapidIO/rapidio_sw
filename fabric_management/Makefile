LINUX_INCLUDE_DIR ?= /usr/src/linux/include
LOCAL_LIBRARY_DIR="libs"

export LOCAL_LIBRARY_DIR
export LINUX_INCLUDE_DIR

TOP_LEVEL = $(shell pwd)

CC= $(CROSS_COMPILE)gcc
CCFLAGS= -c -g -ggdb -fPIC -Wall -Werror -O2 -I. \
	-I./librio_switch/inc \
	-I./cli -I./libriocp_pe/inc -I./librio_maint/inc \
	-I./libriocp_pe/src -I./librio_fmd/inc -I$(LINUX_INCLUDE_DIR) \
	-I../common/include

CLI_DEPS= compile_constants.h

CLI_OBJS= LIN_RTA_CLI.o

DAEMON_DEPS= compile_constants.h

DAEMON_OBJS= rio_daemon.o

LIB_DEPS= libriocp_pe libcli librio_maint libswitch_api librio_fmd
.PHONY: $(LIB_DEPS)


%.o: %.c $(CLI_DEPS) $(DAEMON_DEPS) $(LIB_DEPS)
	$(CC) -o $@ $< $(CCFLAGS)

libriocp_pe:
	$(MAKE) all -C libriocp_pe
	mkdir -p $(LOCAL_LIBRARY_DIR)
	cp libriocp_pe/libriocp_pe.a $(LOCAL_LIBRARY_DIR)

libcli:
	$(MAKE) all -C cli
	mkdir -p $(LOCAL_LIBRARY_DIR)
	cp cli/libclidb.so $(LOCAL_LIBRARY_DIR)

librio_maint:
	$(MAKE) all -C librio_maint
	mkdir -p $(LOCAL_LIBRARY_DIR)
	cp librio_maint/librio_maint.a $(LOCAL_LIBRARY_DIR)

librio_fmd:
	$(MAKE) all -C librio_fmd
	mkdir -p $(LOCAL_LIBRARY_DIR)
	cp librio_fmd/librio_fmd.a $(LOCAL_LIBRARY_DIR)

libswitch_api:
	$(MAKE) all -C librio_switch
	mkdir -p $(LOCAL_LIBRARY_DIR)
	cp librio_switch/libswitch_api.a $(LOCAL_LIBRARY_DIR)
	

daemon: $(DAEMON_OBJS) 
	$(CC) -o fmd $^ -pthread -L$(LOCAL_LIBRARY_DIR) \
	-lrio_fmd -lriocp_pe -lclidb -lrio_maint -lswitch_api -lrt \
	-D__CLI_LINUX__ 

all: $(LIB_DEPS) daemon 

clean:
	rm -f *.o *~ *.exe core $(LD_LIBRARY_PATH)/*.so
	$(MAKE) clean -C librio_switch
	$(MAKE) clean -C librio_maint
	$(MAKE) clean -C cli
	$(MAKE) clean -C librio_fmd
	$(MAKE) clean -C libriocp_pe
