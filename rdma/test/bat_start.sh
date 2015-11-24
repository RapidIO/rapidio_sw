#!/bin/bash
RDMA_ROOT_PATH=/home/srio/git/rapidio_sw
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

# Script for BAT tests

# Use NODES for common programs such as FMD and RDMAD
NODES="10.10.10.51 10.10.10.50"

# Use SERVERS for nodes that shall run "bat_server"
SERVERS="10.10.10.51"

# Use CLIENTS for nodes that run "bat_client"
CLIENTS="10.10.10.50"

# Server-base port number. Increments by for each new server on the same machine.
SERVER_CM_CHANNEL=2224

# Start Fabric Management Daemon on each node
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Starting fmd on $node destID=$DESTID"
	ssh root@"$node" "screen -dmS fmd $RDMA_ROOT_PATH/fabric_management/daemon/fmd -l7"
	sleep 1
	FMD_PID=$(ssh root@"$node" pgrep fmd)
	echo "$node fmd pid=$FMD_PID"
done

# Start RDMAD on each node
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Start rdmad on $node destID=$DESTID"
	ssh root@"$node" "screen -dmS rdmad $RDMA_ROOT_PATH/rdma/rdmad"
	sleep 1
	RDMAD_PID=$(ssh root@"$node" pgrep rdmad)
	echo "$node rdmad pid=$RDMAD_PID"
done

# Start BAT_SERVER on each server node
for node in $SERVERS
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Start rsktd on $node destID=$DESTID"
	ssh root@"$node" "screen -dmS bat_server $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL"
	sleep 1
	# Display PID for verification
	BAT_SERVER_PID=$(ssh root@"$node" pgrep bat_server)
	echo "$node rsktd pid=$BAT_SERVER_PID"

	# Increment channel number
	((SERVER_CM_CHANNEL++ ))
done

