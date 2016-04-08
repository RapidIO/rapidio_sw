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

WAIT_TIME=30
TRANS=5
IBA_ADDR=200000000
ACC_SIZE=40000
BYTES=400000
SKT_PREFIX=234
SYNC_IN=3
SYNC2_IN=${SYNC_IN}
SYNC3_IN=${SYNC_IN}

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
    shift
fi

if [ -n "$3" ]
  then
    WAIT_TIME=$3
    shift
fi

if [ -n "$3" ]
  then
    TRANS_IN=$3
    shift
fi

if [ -n "$3" ]
  then
    SYNC_IN=$3
    shift
fi

if [ -n "$3" ]
  then
    IBA_ADDR=$3
    shift
fi

if [ -n "$3" ]
  then
    SKT_PREFIX=$3
    shift
fi

if [ -n "$3" ]
  then
    SYNC2_IN=$3
    shift
fi

if [ -n "$3" ]
  then
    SYNC3_IN=$3
    shift
fi

if [ $PRINT_HELP != "0" ]; then
	echo $'\nScript requires the following parameters:'
	echo $'MAST       : Name of master node'
	echo $'SLAVE      : Name of slave node'
	echo $'All parameters after this are optional.  Default values shown.'
	echo $'DIR        : Directory on both MAST and SLAVE to run tests.'
	echo $'             Default is ' $HOMEDIR
        echo $'WAIT       : Time in seconds to wait before perf measurement'
	echo $'             Default is ' $WAIT_TIME
        echo $'DMA_TRANS  : DMA transaction type'
        echo $'             0 NW, 1 SW, 2 NW_R, 3 SW_R 4 NW_R_ALL'
	echo $'             Default is to loop through all.'
        echo $'DMA_SYNC   : 0 - blocking, 1 - async, 2 - fire and forget'
	echo $'             Default is to loop through all.'
        echo $'IBA_ADDR   : RapidIO address of inbound window for both nodes'
	echo $'             Default is ' $IBA_ADDR
        echo $'SKT_PREFIX : First 3 digits of 4 digit socket numbers'
	echo $'             Default is ' $SKT_PREFIX
        echo $'\nOptional parameters, if not entered same as DMA_SYNC'
        echo $'DMA_SYNC2  : 0 - blocking, 1 - async, 2 - fire and forget'
        echo $'DMA_SYNC3  : 0 - blocking, 1 - async, 2 - fire and forget'
	exit 1
fi;

INTERP_TRANS=(NW SW NW_R SW_R NW_R_ALL ALL);
INTERP_SYNC=(BLOCK ASYNC FAF ALL);

echo $'\nStarting regression:\n'
echo 'MAST       :' $MAST_NODE
echo 'SLAVE      :' $SLAVE_NODE
echo 'DIR        :' $HOMEDIR
echo 'WAIT TIME  :' $WAIT_TIME ' SECONDS'
echo 'TRANS      :' $TRANS ${INTERP_TRANS[TRANS_IN]}
echo 'SYNC       :' $SYNC_IN  ${INTERP_SYNC[SYNC_IN]}
echo 'SYNC2      :' $SYNC2_IN ${INTERP_SYNC[SYNC2_IN]}
echo 'SYNC3      :' $SYNC3_IN ${INTERP_SYNC[SYNC3_IN]}
echo 'IBA_ADDR   :' $IBA_ADDR
echo 'SKT_PREFIX :' $SKT_PREFIX

if [ "$TRANS_IN" == "5" ]; then
	DMA_TRANS=( 0 1 2 3 4 )
else
	DMA_TRANS=( "$TRANS_IN" )
fi

if [ "$SYNC_IN" == "3" ]; then
	DMA_SYNC=( 0 1 2 )
else
	DMA_SYNC=( "${SYNC_IN}" )
	if [ "$SYNC2_IN" == "3" ]; then
		SYNC2=${SYNC_IN}
	else
		SYNC2=${SYNC2_IN}
	fi
	if [ "$SYNC3_IN" == "3" ]; then
		SYNC3=${SYNC_IN}
	else
		SYNC3=${SYNC3_IN}
	fi
fi

# Before proceeding further, recompile goodput and ugoodput
# and demonstrate that each can execute the startup script

NODES=( "$MAST_NODE" "$SLAVE_NODE" )
declare -a DESTIDS
IDX=0

LOGNAME=start_target.log

for node in ${NODES[@]}; do
	if ( ssh -T root@"$node" '[ -d ' $HOMEDIR ' ]' ); then
		echo $' '
		echo $node " Directory $HOMEDIR exists!"
	else
		echo $node " Directory $HOMEDIR does not exist!!! exiting..."
		exit 1
	fi

	echo $node " Building goodput and ugoodput."
	ssh -T root@"$node" <<BUILD_SCRIPT   
cd $HOMEDIR
make -s clean
make -s all
BUILD_SCRIPT

	if ( ! ssh -T root@"$node" test -f $HOMEDIR/goodput ); then
		echo $'\n'
		echo $node " $HOMEDIR/goodput could not be built. Exiting..."
		exit 1
	fi
	if ( ! ssh -T root@"$node" test -f $HOMEDIR/ugoodput ); then
		echo $'\n'
		echo $node " $HOMEDIR/ugoodput could not be built. Exiting..."
		exit 1
	fi

	echo $node " STARTING GOODPUT"

	ssh -T root@"$node" <<NODE_START
cd ${HOMEDIR}/scripts
./create_start_scripts.sh ${SKT_PREFIX} ${IBA_ADDR}
cd ..

rm -f logs/${LOGNAME}

echo $node " Quitting out of OLD goodput screen session, if one exists"
screen -S goodput -p 0 -X stuff $'quit\r'

sleep 10

screen -dmS goodput ./goodput
screen -S goodput -p 0 -X stuff $'log logs/${LOGNAME}\r'
screen -S goodput -p 0 -X stuff $'. start_target\r'
screen -S goodput -p 0 -X stuff $'close\r'

sleep 1
NODE_START

	if ( ! ssh -T root@"$node" test -s $HOMEDIR/logs/${LOGNAME} ); then
		echo $'\n'
		echo "$node $HOMEDIR/logs/${LOGNAME} check failed. Exiting..."
		exit 1
	fi

	DESTIDS[${IDX}]=$(ssh -T root@${node} 'grep -E dest_id '${HOMEDIR}/logs/${LOGNAME} | awk {$'print $5\r'})

	echo $node " Destination ID is " ${DESTIDS[${IDX}]}
	echo $' '
	IDX=${IDX}+1

done

echo GENERATING SCRIPTS ON SLAVE ${SLAVE_NODE}, PARAMETERS ARE
echo 'WAIT TIME  :' $WAIT_TIME ' SECONDS'
echo 'TRANS      : 0 ' ${INTERP_TRANS[0]}
echo 'SYNC       : 0 ' ${INTERP_SYNC[0]}
echo 'SYNC2      : 0 ' ${INTERP_SYNC[0]}
echo 'SYNC3      : 0 ' ${INTERP_SYNC[0]}
echo 'DID        :' ${DESTIDS[0]}
echo 'IBA_ADDR   : 0x' $IBA_ADDR
echo 'SKT_PREFIX : 0x' $SKT_PREFIX

# NOTE: DESTIDS[0] below is correct, because the slave needs to know
#       the destination ID of the master.

ssh -T root@${SLAVE_NODE} > /dev/null <<SLAVE_SCRIPT_GEN
cd $HOMEDIR/scripts
./create_perf_scripts.sh $WAIT_TIME 0 0 ${DESTIDS[0]} $IBA_ADDR $SKT_PREFIX 0 0
SLAVE_SCRIPT_GEN

## DMA_LAT_SZ=( 
OBWIN_LAT_SZ=( 1B 2B 4B 8B )
OBWIN_LAT_PREFIX=ol

DMA_LAT_SZ=(1B 2B 4B 8B 16B 32B 64B 128B 256B 512B
	1K 2K 4K 8K 16K 32K 64K 128K 256K 512K
	1M 2M 4M)
DMA_LAT_PREFIX=dl

cd ${HOMEDIR}

echo ${DMA_TRANS[0]}
echo ${DMA_SYNC[0]}

for TRANS in ${DMA_TRANS[@]}; do
	for SYNC in ${DMA_SYNC[@]}; do
		if [ ! $SYNC_IN == 3 ]; then
			if [ "$SYNC2_IN" == "3" ]; then
				SYNC2=$SYNC
			fi
			if [ "$SYNC3_IN" == "3" ]; then
				SYNC3=$SYNC
			fi
		fi
		echo GENERATING SCRIPTS ON MASTER ${MAST_NODE}, PARAMETERS ARE
		echo 'WAIT TIME  :' $WAIT_TIME ' SECONDS'
		echo 'TRANS      :' $TRANS ${INTERP_TRANS[TRANS]}
		echo 'SYNC       :' $SYNC  ${INTERP_SYNC[SYNC]}
		echo 'SYNC2      :' $SYNC2 ${INTERP_SYNC[SYNC2]}
		echo 'SYNC3      :' $SYNC3 ${INTERP_SYNC[SYNC3]}
		echo 'DID        :' ${DESTIDS[1]}
		echo 'IBA_ADDR   : 0x' $IBA_ADDR
		echo 'SKT_PREFIX : 0x' $SKT_PREFIX

		echo ' '
		echo 'EXECUTING ' ${HOMEDIR}/scripts/run_all_perf
		let "TOT_WAIT = ((450 * ($WAIT_TIME + 1)) / 60) + 1"
		echo 'ESTIMATING ' $TOT_WAIT ' MINUTES TO COMPLETION...'

# NOTE: DESTIDS[1] below is correct, because the master needs to know
#       the destination ID of the slave
		LOG_FILE_NAME=${HOMEDIR}/logs/
		ssh -T root@${MAST_NODE} rm -f ${LOG_FILE_NAME}/*.log
		ssh -T root@${MAST_NODE} rm -f ${LOG_FILE_NAME}/*.res

		LOG_FILE_NAME=${HOMEDIR}/logs/run_all_perf_done.log

		ssh -T root@${MAST_NODE} > /dev/null << MAST_SCRIPT_RUN
cd $HOMEDIR/scripts
./create_perf_scripts.sh $WAIT_TIME $TRANS $SYNC ${DESTIDS[1]} $IBA_ADDR $SKT_PREFIX $SYNC2 $SYNC3
screen -S goodput -p 0 -X stuff $'scrp scripts\r'
screen -S goodput -p 0 -X stuff $'. run_all_perf\r'
MAST_SCRIPT_RUN

		while ( ! ssh -T root@"${MAST_NODE}" test -s ${LOG_FILE_NAME}) 
		do
			sleep 60
			let "TOT_WAIT = $TOT_WAIT - 1"
			echo 'NOW ' $TOT_WAIT ' MINUTES TO COMPLETION...'
		done

		let "LONG_WAIT= $WAIT_TIME * 2"

		SUBDIR=scripts/performance/obwin_lat
		echo 'EXECUTING ALL SCRIPTS IN ' ${HOMEDIR}'/'${SUBDIR}

		LOGNAME=obwin_lat_write.log
		ssh -T root@${MAST_NODE} << MAST_OBWIN_LAT_ST
screen -S goodput -p 0 -X stuff $'log logs/${LOGNAME}\r'
MAST_OBWIN_LAT_ST

		for SZ in ${OBWIN_LAT_SZ[@]}; do
			# Run slave target loop for write latency test 
			SCRIPTNAME='olT'$SZ'.txt'
			echo ${SLAVE_NODE} ${SCRIPTNAME}
			ssh -T root@"${SLAVE_NODE}"  << SLAVE_OBWIN_LAT_WR
screen -S goodput -p 0 -X stuff $'scrp ${SUBDIR}\r'
screen -S goodput -p 0 -X stuff $'. ${SCRIPTNAME}\r'
SLAVE_OBWIN_LAT_WR
			sleep 2
			
			# Run master loop for write latency test 
			SCRIPTNAME='olW'$SZ'.txt'
			echo ${MAST_NODE} ${SCRIPTNAME}
			ssh -T root@"${MAST_NODE}"  << MAST_OBWIN_LAT_WR
screen -S goodput -p 0 -X stuff $'scrp ${SUBDIR}\r'
screen -S goodput -p 0 -X stuff $'. ${SCRIPTNAME}\r'
MAST_OBWIN_LAT_WR
			sleep $LONG_WAIT
		done

		ssh -T root@${MAST_NODE} << MAST_OBWIN_LAT_ST
screen -S goodput -p 0 -X stuff $'close\r'
MAST_OBWIN_LAT_ST

		SUBDIR=scripts/performance/dma_lat
		echo 'EXECUTING ALL SCRIPTS IN ' ${HOMEDIR}'/'${SUBDIR}

		LOGNAME=dma_lat_write.log
		ssh -T root@${MAST_NODE} << MAST_DMA_LAT_ST
screen -S goodput -p 0 -X stuff $'log logs/${LOGNAME}\r'
MAST_DMA_LAT_ST

		ssh -T root@${SLAVE_NODE} << SLAVE_DMA_LAT_ST
screen -S goodput -p 0 -X stuff $'log logs/${LOGNAME}\r'
SLAVE_DMA_LAT_ST

		for SZ in ${DMA_LAT_SZ[@]}; do
			# Run slave target loop for write latency test 
			SCRIPTNAME='dlT'${SZ}'.txt'
			echo ${SLAVE_NODE} ${SCRIPTNAME}
			ssh -T root@"${SLAVE_NODE}"  << SLAVE_DMA_LAT_WR
screen -S goodput -p 0 -X stuff $'scrp ${SUBDIR}\r'
screen -S goodput -p 0 -X stuff $'. ${SCRIPTNAME}\r'
SLAVE_DMA_LAT_WR
			sleep 2
			
			# Run master loop for write latency test 
			SCRIPTNAME='dlW'${SZ}'.txt'
			echo ${MAST_NODE} ${SCRIPTNAME}
			ssh -T root@"${MAST_NODE}"  << MAST_DMA_LAT_WR
screen -S goodput -p 0 -X stuff $'scrp ${SUBDIR}\r'
screen -S goodput -p 0 -X stuff $'. ${SCRIPTNAME}\r'
MAST_DMA_LAT_WR
			sleep ${LONG_WAIT}

			ssh -T root@"${MAST_NODE}"  << MAST_DMA_LAT_END
screen -S goodput -p 0 -X stuff $'kill 0\r'
screen -S goodput -p 0 -X stuff $'wait 0 d\r'
MAST_DMA_LAT_END
			sleep 1
		done

		ssh -T root@${MAST_NODE} << MAST_DMA_LAT_ST
screen -S goodput -p 0 -X stuff $'close\r'
MAST_DMA_LAT_ST

		ssh -T root@${SLAVE_NODE} << SLAVE_DMA_LAT_ST
screen -S goodput -p 0 -X stuff $'close\r'
SLAVE_DMA_LAT_ST
		
		# Now start analyzing/checking output.
		
		ssh -T root@${MAST_NODE} << MAST_LOGS_CHECK
cd ${HOMEDIR}/logs
./summ_thru_logs.sh > all_thru.res
./summ_lat_logs.sh > all_lat.res
./check_thru_logs.sh all_thru.res
./check_lat_logs.sh all_lat.res
MAST_LOGS_CHECK

		# If there was a failure, keep the goodput sessions around
		# for debug purposes.
		if -s ${HOMEDIR}/logs/thru_fail.txt; then
			exit;
		fi;

		if -s ${HOMEDIR}/logs/lat_fail.txt; then
			exit;
		fi;

	done
done

for node in ${NODES[@]}; do
	ssh -T root@"$node" <<NODE_STOP
cd ${HOMEDIR}/scripts
echo $node " Stopping goodput sessions, if they exist."
screen -S goodput -p 0 -X stuff $'quit\r'
sleep 10
NODE_STOP

exit
