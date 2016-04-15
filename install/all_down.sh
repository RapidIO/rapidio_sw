#!/bin/bash

#  This script shuts down all RIO cluster nodes.

#  Warning:
#  ======== 
#  Please note that the master node (one where this script is executed)
#+ must be last entry in the list of nodes.

. /etc/rapidio/nodelist.sh

for node in $NODES
do
	ssh root@"$node" poweroff 
done

exit 0
