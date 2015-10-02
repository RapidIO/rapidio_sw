#!/bin/bash

# Temporary script to speed up RSKT test apps development.

RDMA_ROOT_PATH=/home/sherif/git/rapidio_sw
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

NODES="GRY10 GRY09"

for node in $NODES
do
	# Kill RSKTD
	THE_PID=$(ssh root@"$node" pgrep rsktd)
	echo "Killing rsktd on $node RSKTD PID=$THE_PID"
	for proc in $THE_PID
	do
		ssh root@"$node" "kill -s 2 $proc"
	done

	# Kill RDMAD
	THE_PID=$(ssh root@"$node" pgrep rdmad)
	echo "Killing rdmad on $node RDMAD PID=$THE_PID"
	for proc in $THE_PID
	do
		ssh root@"$node" "kill -s 2 $proc"
	done

	# Kill FMD
	THE_PID=$(ssh root@"$node" pgrep fmd)
	echo "Killing fmd on $node RDMAD PID=$THE_PID"
	for proc in $THE_PID
	do
		ssh root@"$node" "kill -s 2 $proc"
	done

	# Unload rio_cm
	ssh root@"$node" "modprobe -r rio_cm"
	sleep 1

	# Load rio_cm
	ssh root@"$node" "modprobe rio_cm"
done

