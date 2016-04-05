#!/bin/bash

# Script for automated execution of RSKT tests.
#
# Usage:  sudo ./rskt_node.sh <server> <client1> <client2> <client3>"

# Check that at least a server and a client IP have been specified on command line
if [ "$#" -lt 2 ]; then
	echo "Usage:  sudo ./rskt_node.sh <server> <client1> [<client2>] [<client3>]"
	exit
fi

# Location of binaries
RDMA_ROOT_PATH=/home/srio/git/rapidio_sw

# Location of RIO mport infor (e.g. destid)
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

# Unix signal to send to processes via the 'kill' command
SIGINT=2	# CTRL-C

# Use SERVER_NODES for nodes that shall run "bat_server"
SERVER_NODES="$1"

# Use CLIENT_NODES for nodes that run "bat_client"
CLIENT_NODES="$2 $3 $4"

# Use NODES for common programs such as FMD, RDMAD, and RSKTD
# Note the ORDER is IMPORTANT as we MUST start FMD on the server first.
NODES="$SERVER_NODES $CLIENT_NODES"

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
done

sleep 1

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

sleep 1

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

sleep 1

# Start RSKTD on all nodes
for node in $NODES
do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Start rsktd on $node destID=$DESTID"

	# Only start rdmad if not already running
	THE_PID=$(ssh root@"$node" pgrep rsktd)
	if [ -n "$THE_PID" ]
		then
			THE_PID=$(ssh root@"$node" pgrep -d, rsktd)
			echo "rsktd already running on $node, rsktd PID=$THE_PID"
		else
			ssh root@"$node" "screen -dmS rsktd $RDMA_ROOT_PATH/rdma/rskt/daemon/rsktd"
			sleep 1
			THE_PID=$(ssh root@"$node" pgrep -d, rsktd)
			echo "$node rsktd PID=$THE_PID"
	fi
done

sleep 1

# Run rskt_server on each server node. 
for server_node in $SERVER_NODES
do
	# First get the destination ID of current node
	SERVER_DESTID=$(ssh root@"$server_node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo "Start rskt_server on $server_node destID=$SERVER_DESTID"

	# Create server on current node
	echo "screen -dmS server1 $RDMA_ROOT_PATH/rdma/rskt/daemon/rskt_server"
	ssh root@"$server_node" "screen -dmS server1 $RDMA_ROOT_PATH/rdma/rskt/daemon/rskt_server"

	# Display PIDs for verification
	sleep 1
	RSKT_SERVER_PID=$(ssh root@"$server_node" pgrep -d, rskt_server)
	echo "$server_node rskt_server pids=$RSKT_SERVER_PID"

	# Run multiple rskt_clients on client node(s)
	for client_node in $CLIENT_NODES
	do
		echo "Starting rskt_client on $client_node"
		# Run a client per node using the 'screen' terminal
		ssh root@"$client_node" "screen -dmS client1 $RDMA_ROOT_PATH/rdma/rskt/daemon/rskt_client -d$SERVER_DESTID -r5000"
	done
	# This is a second client on the last node. It is NOT run using the
	# 'screen' terminal since we want it to block until  all clients have finished
	ssh root@"$client_node" "$RDMA_ROOT_PATH/rdma/rskt/daemon/rskt_client -d$SERVER_DESTID -r10000"
done


# ******************* Kill all processes *******************

# Kill rskt_servers on the server node(s)
for node in $SERVER_NODES
do
	# Kill bat_server instances
	for (( ; ; ))
	do
		THE_PID=$(ssh root@"$node" pgrep rskt_server)
		if [ -n "$THE_PID" ]
			then
				for proc in $THE_PID
				do
					echo "Killing rskt_server on $node, rskt_server PID=$proc"
					ssh root@"$node" "kill -s $SIGINT $proc"
					sleep 1
				done
			else
				break
		fi
	done
done

# Kill rsktd
for node in $NODES
do
	for (( ; ; ))
	do
		THE_PID=$(ssh root@"$node" pgrep rsktd)
		if [ -n "$THE_PID" ]
			then
				echo "Killing rsktd on $node, rsktd PID=$THE_PID"
				for proc in $THE_PID
				do
					ssh root@"$node" "kill -9 $proc"
					sleep 1
				done
			else
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
					ssh root@"$node" "kill -9 $proc"
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
					ssh root@"$node" "kill -9 $proc"
					sleep 1
				done
			else
				break
		fi
	done
done

# Unload drivers
for node in $NODES
do
	# Unload rio_cm
	echo "Unloading rio_cm on $node"
	ssh root@"$node" "modprobe -r rio_cm"
	sleep 1

	# Unload rio_mport_cdev
	echo "Unloading rio_mport_cdev on $node"
	ssh root@"$node" "modprobe -r rio_mport_cdev"
	sleep 1
done

echo "GOODBYE!"
