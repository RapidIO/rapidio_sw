#!/bin/bash

#  This script starts the Centaurus RIOSocket kernel IP Frame Tunnelling module

#  Warning:
#  ======== 
#  Please note that the master node (one where this script is executed)
#+ must be first entry in the list of nodes.

SOURCE_PATH="/opt/rapidio/rapidio_sw"
INSTALL_PATH=$SOURCE_PATH"/install"

. /etc/rapidio/nodelist.sh

for node in $NODES
do
	echo "${node}"

	ssh root@"${node}" $INSTALL_PATH/kernel_centaurus_start.sh
done
