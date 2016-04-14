#!/bin/bash
SOURCE_PATH=/opt/rapidio/rapidio_sw
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

# Temporary script to speed up RSKT test apps development.
NODES="node1 node2 node3 node4"

# Load drivers on each node

$SOURCE_PATH/rio_start.sh noenum

arch | awk -vx=1 '/(x86_64|i[3-6]86|ppc64)/{x=0;}END{exit x;}' || exit 1;

# Start DMA Tun on each node, only on known architectures with PCIe & Tsi721
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Start DMA Tun on $node destID=$DESTID"
	ssh root@"$node" "screen -dmS dmatun sh $SOURCE_PATH/utils/goodput/dmatun.sh"
	sleep 1
	DMATUN_PID=$(ssh root@"$node" pgrep ugoodput)
	echo "$node DMA Tun pid=$DMATUN_PID"
done
