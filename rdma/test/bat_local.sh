#!/bin/bash
RDMA_ROOT_PATH=/home/srio/git/rapidio_sw
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

# ***************** Script for BAT tests ***********************
# This should be run on either a client or a server node
# Subsequent to running this shell one can manually run
# run_bat.pl
#
# NOTE: Once that is working will look into calling run_bat.pl
# from here, then killing the processes when done.
# Subsequent to that we can look into repeating the entire process
# as we did with RSKTD.

# ALSO NOTE: For this local version we DON'T explicitly load
# rio_cm & rio_mport_cdev as they are loaded by default
# on the local machines.

# Use NODES for common programs such as FMD and RDMAD
NODES="10.10.10.51 10.10.10.50"

# Use SERVERS for nodes that shall run "bat_server"
SERVERS="10.10.10.51"

# Use CLIENTS for nodes that run "bat_client"
CLIENTS="10.10.10.50"

# Server-base port number. Increments by for each new server on the same machine.
SERVER_CM_CHANNEL1=2224
SERVER_CM_CHANNEL2=2225
SERVER_CM_CHANNEL3=2226

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

# Start 3 BAT_SERVERs on each server node
for node in $SERVERS
do
	# Start 3 servers on channels 2224, 2225, 2226
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Start bat_servers on $node destID=$DESTID"
	echo "screen -dmS bat_server $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL"
	ssh root@"$node" "screen -dmS bat_server $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL1"
	echo "screen -dmS bat_server $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL"
	ssh root@"$node" "screen -dmS bat_server $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL2"
	echo "screen -dmS bat_server $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL"
	ssh root@"$node" "screen -dmS bat_server $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL3"
	sleep 1

	# Display PIDs for verification
	BAT_SERVER_PID=$(ssh root@"$node" pgrep bat_server)
	echo "$node bat_server pids=$BAT_SERVER_PID"
done

