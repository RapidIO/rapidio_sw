#!/bin/bash

MAST_NODE=node1
SLAVE_NODE=node2

HOMEDIR=$PWD
PREVDIR=/..

HOMEDIR=$HOMEDIR$PREVDIR

PRINT_HELP=0

if [ -n "$1" ]
  then
    MAST_NODE=$1
else
	PRINT_HELP=1
fi

if [ -n "$2" ]
  then
    SLAVE_NODE=$2
else
	PRINT_HELP=1
fi

if [ -n "$3" ]
  then
    HOMEDIR=$3
fi

if [ $PRINT_HELP != "0" ]; then
	echo $'\nScript requires the following parameters:'
	echo $'MAST       : Name of master node'
	echo $'SLAVE      : Name of slave node'
	echo $'DIR        : Directory on both MAST and SLAVE to run tests.'
	exit 1
fi;

echo $'\nStarting regression:\n'
echo $'MAST = ' $MAST_NODE
echo $'SLAVE= ' $SLAVE_NODE
echo $'DIR  = ' $HOMEDIR

NODES=( "$MAST_NODE" "$SLAVE_NODE" )

for node in ${NODES[@]}; do
	if ( ssh root@"$node" '[ -d ' $HOMEDIR ' ]' ); then
		echo $'\n'
		echo $node " $HOMEDIR exists!"
		echo $'\n'
	else
		echo $node " $HOMEDIR does not exist!!! exiting..."
		exit 1
	fi

	cp template_build.txt build.txt
	sed -i 's?HOMEDIR?'${HOMEDIR}'?g' build.txt
	cat build.txt | ssh root@"$node" 

	if ( ! ssh root@"$node" test -f $HOMEDIR/goodput ); then
		echo $'\n'
		echo $node " $HOMEDIR/goodput could not be built. Exiting..."
		exit 1
	fi
	if ( ! ssh root@"$node" test -f $HOMEDIR/ugoodput ); then
		echo $'\n'
		echo $node " $HOMEDIR/ugoodput could not be built. Exiting..."
		exit 1
	fi
done


cp template_target.txt target.txt
sed -i 's?HOMEDIR?'${HOMEDIR}'?g' target.txt
cat target.txt | ssh root@"$MAST_NODE" 

