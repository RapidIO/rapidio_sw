#!/bin/bash
#
# This script checks the state of all nodes for 
# - kernel module status
# - RapidIO daemon processes

RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

	NODES="node1 node2 node3 node4"

	OK=1	# Set OK to true before the checks

	#For each node check that all is well
	for node in $NODES
	do
		# Display note name
		echo "+++ $node +++"

		# Check that the node was properly enumerated
		RIODEVS=$(ssh root@"$node" "ls /sys/bus/rapidio/devices/")
		if [ -z "$RIODEVS" ]
		then
			echo "   not enumerated"
			OK=0
		else
			echo "   RIO devices: "$RIODEVS""
		fi

		# Display the 'destid' for the node
		DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid 2>/dev/null")
		echo "   mport destID=$DESTID"

		# Check that rio_mport_cdev was loaded
		RIO_MPORT_CDEV=$(ssh root@"$node" "lsmod | grep rio_mport_cdev")
		if [ -z "$RIO_MPORT_CDEV" ]
		then
			echo "   rio_mport_cdev *NOT* loaded"
			OK=0
		else
			echo "   rio_mport_cdev loaded"
		fi

		# Check that rio_cm was loaded
		RIO_CM=$(ssh root@"$node" "lsmod | grep rio_cm")
		if [ -z "$RIO_CM" ]
		then
			echo "   rio_cm         *NOT* loaded"
			OK=0
		else
			echo "   rio_cm         loaded"
		fi

		# Check that fmd is running
		FMD_PID=$(ssh root@"$node" pgrep fmd)
		if [ -z "$FMD_PID" ]
		then
			echo "   FMD            *NOT* running"
			OK=0
		else
			echo "   FMD   running, PID=$FMD_PID"
		fi

		# Check that rdmad is running
		RDMAD_PID=$(ssh root@"$node" pgrep rdmad)
		if [ -z "$RDMAD_PID" ]
		then
			echo "   RDMAD          *NOT* running"
			OK=0
		else
			echo "   RDMAD running, PID=$RDMAD_PID"
		fi

		# Check that rsktd is running
		RSKTD_PID=$(ssh root@"$node" pgrep rsktd)
		if [ -z "$RSKTD_PID" ]
		then
			echo "   RSKTD          *NOT* running"
			OK=0
		else
			echo "   RSKTD running, PID=$RSKTD_PID"
		fi
	done


exit 0
