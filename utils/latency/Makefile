# Makefile for building latency test
#

KDIR?=
LINUX_INCLUDE_DIR ?= $(KDIR)/include
CXX ?= $(CROSS_COMPILE)g++
config?=NODEBUG
COMMONINC=common

# The following assumes Fab_Man was cloned at the same level
# as Data_Path_Lib, but can be overriden on the command-line.
APIINC?=../../fabric_management/librio_switch/inc

# User-mode flags
UCFLAGS = -O3 -Wall -g -I$(APIINC) -I$(COMMONINC) -I/usr/src/linux/include -I../../common/include -lrt -D$(config) -std=gnu++0x

KMFLAGS = -I/usr/src/linux/include -pthread

UBRIDGE_SOURCES = test/ubridge.c common/pcie_utils.c \
	common/inbound_utils.c common/time_utils.c common/tsi721_config.c \
	common/peer_utils.c common/obwin_utils.c  \
	../../common/libmport/libmport.a

UDMA_SOURCES = test/udma.c common/pcie_utils.c \
	common/inbound_utils.c common/time_utils.c common/tsi721_config.c \
	common/peer_utils.c common/dma_utils.c \
	../../common/libmport/libmport.a

UMSG_SOURCES = test/umsg.c common/pcie_utils.c \
	common/inbound_utils.c common/time_utils.c common/tsi721_config.c \
	common/peer_utils.c common/dma_utils.c common/msg_utils.c \
	../../common/libmport/libmport.a


KDMA_SOURCES = test/kdma.c common/pcie_utils.c \
	common/inbound_utils.c common/time_utils.c common/tsi721_config.c \
	common/peer_utils.c common/dma_utils.c common/latency_threads.c  \
	../../common/libmport/libmport.a

KMSG_SOURCES = test/kmsg.c common/pcie_utils.c \
	common/inbound_utils.c common/time_utils.c common/tsi721_config.c \
	common/peer_utils.c common/dma_utils.c common/latency_threads.c	 \
	../../common/libmport/libmport.a


all: ubridge udma umsg kdma kmsg

ubridge: Makefile $(UBRIDGE_SOURCES) $(COMMONINC)/*.h
	$(CXX) $(UCFLAGS) $(UBRIDGE_SOURCES) -o $@

udma: Makefile $(UDMA_SOURCES) $(COMMONINC)/*.h
	$(CXX) $(UCFLAGS) $(UDMA_SOURCES) -o $@

umsg: Makefile $(UMSG_SOURCES) $(COMMONINC)/*.h
	$(CXX) $(UCFLAGS) $(UMSG_SOURCES) -o $@

kdma: $(KDMA_SOURCES) $(COMMONINC)/*.h
	$(CXX) $(KMFLAGS) $(UCFLAGS) $(KDMA_SOURCES) -o $@

kmsg: $(KMSG_SOURCES) $(COMMONINC)/*.h
	$(CXX) $(KMFLAGS) $(UCFLAGS) $(KMSG_SOURCES) -o $@

clean:
	rm -f *.o ubridge udma umsg kdma kmsg core *~ test/*~ readme/*~ common/*~

