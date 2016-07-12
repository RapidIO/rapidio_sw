#!/bin/bash

SOURCE_PATH=/opt/rapidio/rapidio_sw
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

. /etc/rapidio/nodelist.sh

for node in $NODES; do
	DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid")
	echo $DESTID | grep -qi ffff && {
		echo "Node $node not enumerated? Destid is $DESTID" 2>&1
		exit 1;
	}

	if (( $DESTID == 0 )); then
		echo "Node $node has destid $DESTID which cannot yield a correct IPv4 addr. Use FMD. Skipping." 2>&1
		continue;
	fi

	ssh root@"$node" "/sbin/lsmod" | grep -q riosocket && continue;

	ssh root@"$node" "/sbin/modprobe riosocket; /sbin/lsmod" | grep -q riosocket || {
		echo "Node $node won't load riosocket kernel module" 2>&1
		exit 2;
	} 

	# XXX This IPv4 assignment is naive at best and works with up to 254 node clusters
	# XXX DESTID=0 will yield 169.254.0.0 which is bcast addr. Use FMD for enumeration.
	ssh root@"$node" "/sbin/ifconfig rsock0 169.254.0.$DESTID"
done
