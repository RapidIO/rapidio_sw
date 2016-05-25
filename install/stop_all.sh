#!/bin/bash

# Temporary script to speed up RSKT test apps development.

SOURCE_PATH=/opt/rapidio/rapidio_sw
SCRIPTS_PATH=$SOURCE_PATH/install
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

. /etc/rapidio/nodelist.sh

for node in $REVNODES
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
	echo "Killing -fmd- on $node  FMD  PID=$THE_PID"
	for proc in $THE_PID
	do
		ssh root@"$node" "kill -s 2 $proc"
	done

	# Kill ShM UMD
	THE_PID=$(ssh root@"$node" pgrep umdd)
	echo "Killing umdd- on $node UMDD  PID=$THE_PID"
	for proc in $THE_PID
	do
		ssh root@"$node" screen -S umdd -p 0 -X stuff $'quit\r'
	done

	sleep 2

	ssh root@"$node" "modprobe -r rio_mport_cdev"
	ssh root@"$node" "modprobe -r rio_cm"
done

