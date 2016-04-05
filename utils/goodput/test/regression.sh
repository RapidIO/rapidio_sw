#!/bin/bash

# Log regression script to execute all goodput measurements on two nodes.
#
# First node entered is the master node, where requests originate.
# Second node entered is the slave node, where requests are terminated.
# Optionally, a directory where the goodput source code resides on both
#   nodes can be entered.  The directory default value is the parent
#   directory of the directory containing this script.
# 
# There are many steps to this script.

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
#	cat build.txt | ssh root@"$node" 

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

RIOSOCK=234
WAIT=30
DMA_TRANS=( 0 )
# DMA_TRANS=( 0 1 2 3 4 )
DMA_SYNC=( 0 )
# DMA_SYNC=( 0 1 2 )
LOGNAME=start_target.log

cp template_target.txt target.txt
sed -i 's?HOMEDIR?'${HOMEDIR}'?g' target.txt
sed -i 's?RIOSOCK?'${RIOSOCK}'?g' target.txt
sed -i 's?LOGNAME?'${LOGNAME}'?g' target.txt

echo GETTING SLAVE DESTID AND ADDRESS, STARTING SLAVE GOODPUT
cat target.txt | ssh root@"$SLAVE_NODE" 

if ( ! ssh root@"$SLAVE_NODE" test -s $HOMEDIR/logs/${LOGNAME} ); then
		echo $'\n'
		echo "$SLAVE_NODE $HOMEDIR/logs/${LOGNAME} BAD. Exiting..."
		exit 1
fi

SLAVE_ADDR=$(ssh root@${SLAVE_NODE} 'grep -E IBWIN '${HOMEDIR}/logs/${LOGNAME} | awk {$'print $8\r'})
SLAVE_DESTID=$(ssh root@${SLAVE_NODE} 'grep -E dest_id '${HOMEDIR}/logs/${LOGNAME} | awk {$'print $5\r'})

echo "SLAVE_ADDR   = " $SLAVE_ADDR
echo "SLAVE_DESTID = " $SLAVE_DESTID

echo GETTING MASTER DESTID AND ADDRESS, STARTING MASTER GOODPUT
cat target.txt | ssh root@"$MAST_NODE" 

if ( ! ssh root@"$MAST_NODE" test -s $HOMEDIR/logs/${LOGNAME} ); then
		echo $'\n'
		echo "$MAST_NODE $HOMEDIR/logs/${LOGNAME} BAD. Exiting..."
		exit 1
fi

MAST_ADDR=$(ssh root@${MAST_NODE} 'grep -E IBWIN '${HOMEDIR}/logs/${LOGNAME} | awk {$'print $8\r'})
MAST_DESTID=$(ssh root@${MAST_NODE} 'grep -E dest_id '${HOMEDIR}/logs/${LOGNAME} | awk {$'print $5\r'})

echo "MAST_ADDR   = " $MAST_ADDR
echo "MAST_DESTID = " $MAST_DESTID

echo GENERATING SCRIPTS ON SLAVE
echo PARAMETERS ARE $WAIT 0 0 $MAST_DESTID $MAST_ADDR $RIOSOCK
ssh root@${SLAVE_NODE} << DONE_SCRIPT_GEN
cd $HOMEDIR/scripts
./create_perf_scripts.sh $WAIT 0 0 $MAST_DESTID $MAST_ADDR $RIOSOCK
DONE_SCRIPT_GEN

## DMA_LAT_SZ=( 
OBWIN_LAT_SZ=( 1B 2B 4B 8B )
OBWIN_LAT_PREFIX=ol


cd ${HOMEDIR}

for TRANS in ${DMA_TRANS[@]}; do
	for SYNC in ${DMA_SYNC[@]}; do
		cd ${HOMEDIR}/scripts
		./create_perf_scripts.sh $WAIT $TRANS $SYNC $SLAVE_DESTID $SLAVE_ADDR $RIOSOCK
		echo 'SCRIPT SETTINGS '$WAIT $TRANS $SYNC $SLAVE_DESTID $SLAVE_ADDR $RIOSOCK
# FIXME: REMOVE THIS LINE FOR PRODUCTION TEST
		rm -f ${HOMEDIR}/scripts/run_all_perf

		sudo screen -S goodput -p 0 -X stuff $'scrp scripts\r'
		sudo screen -S goodput -p 0 -X stuff $'. run_all_perf\r'

		sudo screen -S goodput -p 0 -X stuff $'scrp scripts/performance/obwin_lat\r'

		cd ${HOMEDIR}/test
		LOGNAME=obwin_lat_write.log
		cp template_logstart.txt logstart.txt
		sed -i 's?LOGNAME?'${LOGNAME}'?g' logstart.txt
		cat logstart.txt | ssh root@"$MAST_NODE" 

		for SZ in ${OBWIN_LAT_SZ[@]}; do
			SCRIPTNAME='olT'$SZ'.txt'

			# Run slave target loop for write latency test 
			cp template_obwin_lat.txt obwin_lat.txt
			sed -i 's?HOMEDIR?'${HOMEDIR}'?g' obwin_lat.txt
			sed -i 's?SCRIPTNAME?'${SCRIPTNAME}'?g' obwin_lat.txt
			cat obwin_lat.txt | ssh root@"$SLAVE_NODE" 
			sleep 1
			
			SCRIPTNAME='olW'$SZ'.txt'
			# Run slave target loop for write latency test 
			cp template_obwin_lat.txt obwin_lat.txt
			sed -i 's?HOMEDIR?'${HOMEDIR}'?g' obwin_lat.txt
			sed -i 's?SCRIPTNAME?'${SCRIPTNAME}'?g' obwin_lat.txt
			cat obwin_lat.txt | ssh root@"$MAST_NODE" 
			sleep $WAIT
		done

		cp template_logstop.txt logstop.txt
		sed -i 's?LOGNAME?'${LOGNAME}'?g' logstop.txt
		cat logstop.txt | ssh root@"$MAST_NODE" 
	done
done


exit
