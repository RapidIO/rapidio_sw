#!/bin/bash

#  This script starts all RIO cluster nodes.

#  Warning:
#  ======== 
#  Please note that the master node (one where this script is executed)
#+ must be first entry in the list of nodes.

SOURCE_PATH="/opt/rapidio/rapidio_sw"
INSTALL_PATH=$SOURCE_PATH"/install"

NODES=(node1 node2 node3 node4) 

for node in "${NODES[@]}"
do
	echo "${node}"

	if [ -z "$1" ]
	then
		ssh root@"${node}" $INSTALL_PATH/kernel_start.sh
	else
		ssh root@"${node}" $INSTALL_PATH/kernel_start.sh noenum
	fi

	if [ "${node}" == "${NODES[0]}" ]
	then
		sleep 5
	fi
done
