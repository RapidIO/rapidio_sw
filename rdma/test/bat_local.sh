#!/bin/bash
RDMA_ROOT_PATH=/home/srio/git/rapidio_sw
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

# Script for automated execution of BAT tests.

# NOTE: For this local version we DON'T explicitly load
# rio_cm & rio_mport_cdev as they are loaded by default
# on the local machines.

# Use NODES for common programs such as FMD and RDMAD
# Note the ORDER is IMPORTANT as we MUST start FMD
# on the SERVER machine first. For the local setup it is 10.10.10.51
NODES="10.10.10.51 10.10.10.50"

# Use SERVER_NODES for nodes that shall run "bat_server"
SERVER_NODES="10.10.10.51"

# Use CLIENT_NODES for nodes that run "bat_client"
CLIENT_NODES="10.10.10.50"

# Server-base port number. Increments by for each new server on the same machine.
# Also increments for other machines. So the first machine shall use 2224, 2225, and 2226;
# the second machine shall use 2227, 2228, and 2229; and so on.
SERVER_CM_CHANNEL_START=2224
SERVER_CM_CHANNEL=$SERVER_CM_CHANNEL_START

# Unix signal to send to processes via the 'kill' command
SIGINT=2	# CTRL-C

# Start Fabric Management Daemon on all nodes
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Starting fmd on $node destID=$DESTID"
	ssh root@"$node" "screen -dmS fmd $RDMA_ROOT_PATH/fabric_management/daemon/fmd -l7"
	sleep 1
	FMD_PID=$(ssh root@"$node" pgrep fmd)
	echo "$node fmd pid=$FMD_PID"
done

# Start RDMAD on all nodes
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Start rdmad on $node destID=$DESTID"
	ssh root@"$node" "screen -dmS rdmad $RDMA_ROOT_PATH/rdma/rdmad"
	sleep 1
	RDMAD_PID=$(ssh root@"$node" pgrep rdmad)
	echo "$node rdmad pid=$RDMAD_PID"
done

sleep 2

# Start 3 BAT_SERVERs on each server node. This is mainly because we have a BAT
# test case that creates a memory space from one server app, and opens that memory
# space from 2 other server apps. So we need three.
for node in $SERVER_NODES
do
	# First get the destination ID of current node
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Start bat_servers on $node destID=$DESTID"

	# Create first server on current node
	echo "screen -dmS bat_server $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL"
	ssh root@"$node" "screen -dmS server1 $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL"

	# Increment CM channel number for the next server app on the same node
	((SERVER_CM_CHANNEL++ ))

	# Create second server on current node
	echo "screen -dmS bat_server $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL"
	ssh root@"$node" "screen -dmS server2 $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL"

	# Increment CM channel number for the next server app on the same node
	((SERVER_CM_CHANNEL++ ))

	# Create third server on current node
	echo "screen -dmS bat_server $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL"
	ssh root@"$node" "screen -dmS server3 $RDMA_ROOT_PATH/rdma/test/bat_server -c$SERVER_CM_CHANNEL"

	# Increment CM channel number for next node
	((SERVER_CM_CHANNEL++ ))

	# Display PIDs for verification
	sleep 1
	BAT_SERVER_PID=$(ssh root@"$node" pgrep bat_server)
	echo "$node bat_server pids=$BAT_SERVER_PID"
done

# NOTE: For now run_bat.pl runs a single test sequence originating
# at this (local) machine. We should be able to modify it to run
# multiple tests originating at multiple nodes

/usr/bin/perl run_bat.pl -c$SERVER_CM_CHANNEL_START -d$DESTID

# ******************* Kill all processes *******************

# Kill bat_servers on the server node(s)
for node in $SERVER_NODES
do
	# Kill bat_server instances
	for (( ; ; ))
	do
		THE_PID=$(ssh root@"$node" pgrep bat_server)
		if [ -n "$THE_PID" ]
			then
				echo "Killing bat_server on $node, bat_server PID=$THE_PID"
				for proc in $THE_PID
				do
					ssh root@"$node" "kill -s $SIGINT $proc"
					sleep 1
				done
			else
				echo "No (more) bat_server(s) running on $node"
				break
		fi
	done
done

# Kill rdmad
for node in $NODES
do
	for (( ; ; ))
	do
		THE_PID=$(ssh root@"$node" pgrep rdmad)
		if [ -n "$THE_PID" ]
			then
				echo "Killing rdmad on $node, rdmad PID=$THE_PID"
				for proc in $THE_PID
				do
					ssh root@"$node" "kill -s $SIGINT $proc"
					sleep 1
				done
			else
				break
		fi
	done
done

# Kill fmd
for node in $NODES
do
	for (( ; ; ))
	do
		THE_PID=$(ssh root@"$node" pgrep fmd)
		if [ -n "$THE_PID" ]
			then
				echo "Killing fmd on $node, fmd PID=$THE_PID"
				for proc in $THE_PID
				do
					ssh root@"$node" "kill -s 2 $proc"
					sleep 1
				done
			else
				break
		fi
	done
done


echo "GOODBYE!"
