#!/bin/bash

# Script to clean up BAT tests

RDMA_ROOT_PATH=/home/srio/git/rapidio_sw
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

NODES="10.10.10.50 10.10.10.51"
SERVERS="10.10.10.51"

for node in $SERVERS
do
	# Kill bat_server
	THE_PID=$(ssh root@"$node" pgrep bat_server)
	echo "Killing bat_server on $node bat_server PID=$THE_PID"
	for proc in $THE_PID
	do
		ssh root@"$node" "kill -s 2 $proc"
	done
done

for node in $NODES
do
	# Kill RDMAD
	THE_PID=$(ssh root@"$node" pgrep rdmad)
	echo "Killing rdmad on $node RDMAD PID=$THE_PID"
	for proc in $THE_PID
	do
		ssh root@"$node" "kill -s 2 $proc"
	done

	# Kill FMD
	THE_PID=$(ssh root@"$node" pgrep fmd)
	echo "Killing fmd on $node FMD PID=$THE_PID"
	for proc in $THE_PID
	do
		ssh root@"$node" "kill -s 2 $proc"
	done

	# Unload rio_cm
	echo "Unloading rio_cm on $node"
	ssh root@"$node" "modprobe -r rio_cm"
	sleep 1

	# Load rio_cm
	echo "Reloading rio_cm on $node"
	ssh root@"$node" "modprobe rio_cm"
done
