#!/bin/bash

# This script starts RDMA daemon on all computing nodes listed in NODES.
# NOTE:
# RapidIO RDMA software installation path must be identical on all nodes.

RDMA_ROOT_PATH=/opt/rapidio/cern/rapidio_sw
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0
NODES="gry09 gry10 gry11 gry12"
NUM_ITERATIONS=1

for (( i=0; i<NUM_ITERATIONS; i++ ))
do
	echo -n "Iteration " $i
	echo ""

	# Start Fabric Management Daemon on each node
	for node in $NODES
	do
		DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
		echo "Start fmd on $node destID=$DESTID"
		ssh root@"$node" "screen -dmS fmd $RDMA_ROOT_PATH/fabric_management/daemon/fmd -l7"
		sleep 5
	done

	# Start RDMAD on each node
	for node in $NODES
	do
		DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
		echo "Start rdmad on $node destID=$DESTID"
		ssh root@"$node" "screen -dmS rdmad $RDMA_ROOT_PATH/rdma/rdmad"
		sleep 5
		RDMAD_PID=$(ssh root@"$node" pgrep rdmad)
		echo "$node RDMAD PID=$RDMAD_PID"
	done

	# Start RSKTD on each node
	for node in $NODES
	do
		echo "Start rsktd on $node"
		ssh root@"$node" "screen -dmS rsktd $RDMA_ROOT_PATH/rdma/rskt/daemon/rsktd -l7"
		sleep 5
	done

	# Now check that everything is running OK
	
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
		MPORTCDEV=$(ssh root@"$node" "lsmod | grep rio_mport_cdev")
		if [ -z "$MPORTCDEV" ]
		then
			echo "   MPORT_CDEV *NOT* loaded"
			OK=0
		else
			echo "   MPORT_CDEV loaded"
		fi

		# Check that rio_cm was loaded
		MPORTCDEV=$(ssh root@"$node" "lsmod | grep rio_cm")
		if [ -z "$MPORTCDEV" ]
		then
			echo "   RIO_CM     *NOT* loaded"
			OK=0
		else
			echo "   RIO_CM     loaded"
		fi

		# Check that fmd is running
		RDMAD_PID=$(ssh root@"$node" pgrep fmd)
		if [ -z "$RDMAD_PID" ]
		then
			echo "   FMD   *NOT* running"
			OK=0
		else
			echo "   FMD   is running PID=$RDMAD_PID"
		fi

		# Check that rdmad is running
		RDMAD_PID=$(ssh root@"$node" pgrep rdmad)
		if [ -z "$RDMAD_PID" ]
		then
			echo "   RDMAD *NOT* running"
			OK=0
		else
			echo "   RDMAD is running PID=$RDMAD_PID"
		fi

		# Check that rsktd is running
		RDMAD_PID=$(ssh root@"$node" pgrep rsktd)
		if [ -z "$RDMAD_PID" ]
		then
			echo "   RSKTD *NOT* running"
			OK=0
		else
			echo "   RSKTD is running PID=$RDMAD_PID"
		fi
	done

	# If there is any failure then stop so we can examine the logs
	if [ $OK -eq 0 ]
	then
		echo "	Something failed. Ending test."
		echo ""
		i=MAX_ITERATIONS
	else
		echo "	Everything worked. Retrying, but cleaning up first"
		echo ""

		# For each node, kill RSKTD RDMAD and FMD, and reload the drivers
		for node in $NODES
		do
			# Kill RSKTD
			THE_PID=$(ssh root@"$node" pgrep rsktd)
			echo "$node RSKTD PID=$THE_PID"
			for proc in $THE_PID
			do
				ssh root@"$node" "kill -s 2 $proc"
			done

			# Kill RDMAD
			THE_PID=$(ssh root@"$node" pgrep rdmad)
			echo "$node RDMAD PID=$THE_PID"
			for proc in $THE_PID
			do
				ssh root@"$node" "kill -s 2 $proc"
			done

			# Kill FMD
			THE_PID=$(ssh root@"$node" pgrep fmd)
			echo "$node RDMAD PID=$THE_PID"
			for proc in $THE_PID
			do
				ssh root@"$node" "kill -s 2 $proc"
			done

			# Unload then reload drivers
			ssh root@"$node" "modprobe -r rio_mport_cdev"
			ssh root@"$node" "modprobe -r rio_cm"
			ssh root@"$node" "modprobe rio_cm"
			ssh root@"$node" "modprobe rio_mport_cdev"
		done
	fi # 	if [ $OK -eq 0 ]
done #for (( i=0; i<MAX_NO; i++ ))

echo "All iterations done. Goodbye"
echo ""

exit 0

