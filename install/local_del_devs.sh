#!/bin/bash
# NOTE; This script deletes all currently available RapidIO devcies.
#       RIO_MPORT_CDEV must be loaded when this script is executed.

shopt -s nullglob

loaded=0;
if lsmod | grep rio_mport_cdev &> /dev/null ; then
	loaded=1
else
	modprobe rio-mport-cdev
fi

FILES=/sys/bus/rapidio/devices/*
for filename in $FILES ; do
	only_file=$(basename $filename)
	echo "        " $only_file
    	/opt/rapidio/rapidio_sw/common/libmport/riodp_test_devs -d -N $only_file
done

if [ $loaded -eq 0 ]; then
	modprobe -r rio-mport-cdev
fi
