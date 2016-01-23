#!/bin/bash

#  This script stops all RIO cluster nodes.

SOURCE_PATH="/opt/rapidio/rapidio_sw"
INSTALL_PATH=$SOURCE_PATH"/install"

NODES=(node1 node2 node3 node4) 

for node in "${NODES[@]}"
do
	echo "${node}"

	ssh root@"${node}" $INSTALL_PATH/kernel_stop.sh
done
