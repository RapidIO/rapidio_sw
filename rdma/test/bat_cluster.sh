#!/bin/bash

if [ "$#" -lt 4 ]; then
	echo "Usage:  sudo ./bat_cluster.sh <node1> <node2> <node3> <node4> [<test>]"
	exit
fi

# Location of binaries
RDMA_ROOT_PATH=/home/sherif/git/rapidio_sw

ssh -t root@"$1" "screen -dmS $1 $RDMA_ROOT_PATH/rdma/test/bat_node.sh $1 $2 $3 $4 1100 $5"
ssh -t root@"$2" "screen -dmS $2 $RDMA_ROOT_PATH/rdma/test/bat_node.sh $2 $1 $3 $4 1200 $5"
ssh -t root@"$3" "screen -dmS $3 $RDMA_ROOT_PATH/rdma/test/bat_node.sh $3 $1 $2 $4 1300 $5"
ssh -t root@"$3" "screen -dmS $4 $RDMA_ROOT_PATH/rdma/test/bat_node.sh $4 $1 $2 $3 1400 $5"

