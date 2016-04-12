#!/bin/bash
SOURCE_PATH=/opt/rapidio/rapidio_sw
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

# Temporary script to speed up RSKT test apps development.
NODES="node1 node2 node3 node4"

# Load drivers on each node

$SOURCE_PATH/rio_start.sh noenum

# Start Fabric Management Daemon on each node
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Starting fmd on $node destID=$DESTID"
	ssh root@"$node" "screen -dmS fmd $SOURCE_PATH/fabric_management/daemon/fmd -l7"
	sleep 1
	FMD_PID=$(ssh root@"$node" pgrep fmd)
	echo "$node fmd pid=$FMD_PID"
done

# Start SOURCE_PATH on each node
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Start rdmad on $node destID=$DESTID"
	ssh root@"$node" "screen -dmS rdmad $SOURCE_PATH/rdma/rdmad"
	sleep 1
	RDMA_PID=$(ssh root@"$node" pgrep rdmad)
	echo "$node rdmad pid=$RDMA_PID"
done

# Start RSKTD on each node
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Start rsktd on $node destID=$DESTID"
	ssh root@"$node" "screen -dmS rsktd $SOURCE_PATH/rdma/rskt/daemon/rsktd -l7 -s 32"
	sleep 1
	RSKTD_PID=$(ssh root@"$node" pgrep rsktd)
	echo "$node rsktd pid=$RSKTD_PID"
done

if arch | awk -vx=1 '/(x86_64|i[3-6]86|ppc64)/{x=0;}END{exit x;}'; then
	# Start UMDD on each node, only on known architectures with PCIe & Tsi721
	for node in $NODES
	do
		DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
		echo "Start UMDd/SHM on $node destID=$DESTID"
		ssh root@"$node" "screen -dmS umdd sh $SOURCE_PATH/umdd_tsi721/umdd.sh"
		sleep 1
		UMDD_PID=$(ssh root@"$node" pgrep umdd)
		echo "$node UMDd/SHM pid=$UMDD_PID"
	done
fi
