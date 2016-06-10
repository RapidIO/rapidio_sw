#!/bin/bash

SOURCE_PATH=/opt/rapidio/rapidio_sw
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

START_RDMAD=y
START_RSKTD=y
START_UMDD=y
START_DMATUN=n

echo "$@" | grep -qw dmatun &&  START_DMATUN=y;
echo "$@" | grep -q norsktd && START_RSKTD=n;
echo "$@" | grep -q noumdd  && START_UMDD=n;
echo "$@" | grep -q nordmad && {
  START_RDMAD=n; START_RSKTD=n
}

# Platform check for UMD-based components, start only on known architectures with PCIe & Tsi721
arch | awk -vx=1 '/(x86_64|i[3-6]86|ppc64)/{x=0;}END{exit x;}' || { # not i686, x86_64, PPC64
  START_UMDD=n;
  START_DMATUN=n;
}

set | grep ^START_

. /etc/rapidio/nodelist.sh

# Load drivers on each node

$SOURCE_PATH/rio_start.sh noenum

# Start Fabric Management Daemon on each node
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Starting fmd on $node destID=$DESTID"
	ssh root@"$node" screen -dmS fmd $SOURCE_PATH/fabric_management/daemon/fmd -l3
	sleep 1
	FMD_PID=$(ssh root@"$node" pgrep fmd)
	echo "$node fmd pid=$FMD_PID"
done

sleep 1; # Allow FMD to enumerate nodes

# Start DMA Tun on each node. It is important to start first
# to be able to grab from kernel a size-aligned CMA IBwin.
if [ "$START_DMATUN" = 'y' ]; then
	for node in $NODES; do
		DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
		[ "$DESTID" = '0xffffffff' ] && { echo "Node $node not enumerated skipping DMA Tun"; continue; }
		echo "Start DMA Tun on $node destID=$DESTID"
		ssh root@"$node" "screen -dmS dmatun sh $SOURCE_PATH/utils/goodput/dmatun.sh"
		sleep 1
		DMATUN_PID=$(ssh root@"$node" pgrep ugoodput)
		echo "$node DMA Tun pid=$DMATUN_PID"
	done
fi

# Start RDMAD on each node
if [ "$START_RDMAD" = 'y' ]; then
	for node in $NODES; do
		DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
		echo "Start rdmad on $node destID=$DESTID"
		ssh root@"$node" "screen -dmS rdmad $SOURCE_PATH/rdma/rdmad -l3"
		sleep 1
		RDMA_PID=$(ssh root@"$node" pgrep rdmad)
		echo "$node rdmad pid=$RDMA_PID"
	done
fi

# Start RSKTD on each node
if [ "$START_RSKTD" = 'y' ]; then
	for node in $NODES; do
		DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
		echo "Start rsktd on $node destID=$DESTID"
		ssh root@"$node" "screen -dmS rsktd $SOURCE_PATH/rdma/rskt/daemon/rsktd -l3"
		sleep 1
		RSKTD_PID=$(ssh root@"$node" pgrep rsktd)
		echo "$node rsktd pid=$RSKTD_PID"
	done
fi

if [ "$START_UMDD" = 'y' ]; then
	for node in $NODES; do
		DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
		[ "$DESTID" = '0xffffffff' ] && { echo "Node $node not enumerated skipping UMDD"; continue; }
		echo "Start UMDd/SHM on $node destID=$DESTID"
		ssh root@"$node" "screen -dmS umdd sh $SOURCE_PATH/umdd_tsi721/umdd.sh"
		sleep 1
		UMDD_PID=$(ssh root@"$node" pgrep umdd)
		echo "$node UMDd/SHM pid=$UMDD_PID"
	done
fi
