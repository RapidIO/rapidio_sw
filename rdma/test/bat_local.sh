#!/bin/bash

# Script for automated execution of BAT tests.

# Location of binaries
RDMA_ROOT_PATH=/home/srio/git/rapidio_sw

# Location of RIO mport infor (e.g. destid)
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

# Use NODES for common programs such as FMD and RDMAD
# Note the ORDER is IMPORTANT as we MUST start FMD
# on the SERVER machine first. For the local setup it is 10.10.10.51
NODES="10.10.10.51 10.10.10.50"

# Use SERVER_NODES for nodes that shall run "bat_server"
SERVER_NODES="10.10.10.51"

# Use CLIENT_NODES for nodes that run "bat_client"
CLIENT_NODES="10.10.10.50"

# Server-base port number. Increments by 1 for each new server on the same machine.
# Also increments for other machines. So the first machine shall use 2224, 2225, and 2226;
# the second machine shall use 2227, 2228, and 2229; and so on.
SERVER_CM_CHANNEL_START=2224
SERVER_CM_CHANNEL=$SERVER_CM_CHANNEL_START

# Unix signal to send to processes via the 'kill' command
SIGINT=2	# CTRL-C



# Load drivers on each node
for node in $NODES
do
	THE_DRIVER=$(ssh root@"$node" lsmod | grep rio_cm)
	if [ -n "$THE_DRIVER" ]
		then
			echo "rio_cm already loaded on $node!";
		else
			ssh root@"$node" "modprobe rio_cm"
			sleep 1
	fi

	THE_DRIVER=$(ssh root@"$node" lsmod | grep rio_mport_cdev)
	if [ -n "$THE_DRIVER" ]
		then
			echo "rio_mport_cdev already loaded on $node!";
		else
			ssh root@"$node" "modprobe rio_mport_cdev"
			sleep 1
	fi

	sleep 1
done

# Start Fabric Management Daemon on all nodes
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Starting fmd on $node destID=$DESTID"

	# Only start fmd if not already running
	THE_PID=$(ssh root@"$node" pgrep fmd)
	if [ -n "$THE_PID" ]
		then
			echo "fmd already running on $node, fmd PID=$THE_PID"
		else
			ssh root@"$node" "screen -dmS fmd $RDMA_ROOT_PATH/fabric_management/daemon/fmd -l7"
			sleep 1
			THE_PID=$(ssh root@"$node" pgrep -d"," fmd)
			echo "$node fmd pid=$THE_PID"
	fi
done

# Start RDMAD on all nodes
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Start rdmad on $node destID=$DESTID"

	# Only start rdmad if not already running
	THE_PID=$(ssh root@"$node" pgrep rdmad)
	if [ -n "$THE_PID" ]
		then
			THE_PID=$(ssh root@"$node" pgrep -d, rdmad)
			echo "rdmad already running on $node, rdmad PID=$THE_PID"
		else
			ssh root@"$node" "screen -dmS rdmad $RDMA_ROOT_PATH/rdma/rdmad"
			sleep 1
			THE_PID=$(ssh root@"$node" pgrep -d, rdmad)
			echo "$node rdmad PID=$THE_PID"
	fi
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
	BAT_SERVER_PID=$(ssh root@"$node" pgrep -d, bat_server)
	echo "$node bat_server pids=$BAT_SERVER_PID"
done

# NOTE: For now run_bat.pl runs a single test sequence originating
# at this (local) machine. We should be able to modify it to run
# multiple tests originating at multiple nodes

#/usr/bin/perl run_bat.pl -c$SERVER_CM_CHANNEL_START -d$DESTID

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
				for proc in $THE_PID
				do
					echo "Killing bat_server on $node, bat_server PID=$proc"
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
