#!/bin/bash

# Log regression script to execute all ugoodput measurements on two nodes.
#
# First node entered is the master node, where requests originate.
# Second node entered is the slave node, where requests are terminated.
# Optionally, a directory where the ugoodput source code resides on both
#   nodes can be entered.  The directory default value is the parent
#   directory of the directory containing this script.
# 
# There are many steps to this script.

MAST_NODE=node1
SLAVE_NODE=node2

MAST_MPNUM=0
SLAVE_MPNUM=0

DFLT_HOMEDIR=$PWD
PREVDIR=/..

DFLT_HOMEDIR=$DFLT_HOMEDIR$PREVDIR
DFLT_WAIT_TIME=30
DFLT_IBA_ADDR=0x200000000
DFLT_BUFC=0x100
DFLT_STS=0x100
DFLT_DMA_CHAN=2
DFLT_MBOX_CHAN=2
DFLT_TX_CPU=2
DFLT_STS_CPU=3

HOMEDIR=$DFLT_HOMEDIR

WAIT_TIME=$DFLT_WAIT_TIME

INTERP_TRANS=(READ LAST_NW_R NW NW_R_ALL DEFAULT);
WR_TRANS_IN=4
IBA_ADDR=$DFLT_IBA_ADDR
ACC_SIZE=0x40000
BYTES=0x400000
BUFC_IN=$DFLT_BUFC
STS_IN=$DFLT_STS
DMA_CHAN_IN=$DFLT_DMA_CHAN
MBOX_CHAN_IN=$DFLT_MBOX_CHAN
TX_CPU_IN=$DFLT_TX_CPU
STS_CPU_IN=$DFLT_STS_CPU

unset OVERRIDE_IN

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
    MAST_MPNUM=$3
	shift
else
	PRINT_HELP=1
fi

if [ -n "$3" ]
  then
    SLAVE_MPNUM=$3
	shift
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
    WR_TRANS_IN=$3
	if [ "$WR_TRANS_IN" -lt 1 ]; then
		PRINT_HELP=1
	fi
        if [ "$WR_TRANS_IN" -gt 3 ]; then
                PRINT_HELP=1
        fi
    shift
fi

if [ -n "$3" ]
  then
    IBA_ADDR=$3
    shift
fi

if [ -n "$3" ]
  then
    BUFC_IN=$3
    shift
fi

if [ -n "$3" ]
  then
    STS_IN=$3
    shift
fi

if [ -n "$3" ]
  then
    DMA_CHAN_IN=$3
    shift
	if [ "$DMA_CHAN_IN" -lt 2 ]; then
		PRINT_HELP=1
	fi
        if [ "$DMA_CHAN_IN" -gt 7 ]; then
                PRINT_HELP=1
        fi
fi

if [ -n "$3" ]
  then
    MBOX_CHAN_IN=$3
    shift
	if [ "$MBOX_CHAN_IN" -lt 2 ]; then
		PRINT_HELP=1
	fi
        if [ "$MBOX_CHAN_IN" -gt 3 ]; then
                PRINT_HELP=1
        fi
fi

if [ -n "$3" ]
  then
    TX_CPU_IN=$3
    shift
fi

if [ -n "$3" ]
  then
    STS_CPU_IN=$3
    shift
fi

if [ -n "$3" ]
  then
    OVERRIDE_IN=$3
    shift
fi

if [ $PRINT_HELP != "0" ]; then
	echo $'\nScript requires the following parameters:'
	echo $'MAST       : Name of master node/IP address'
	echo $'SLAVE      : Name of slave node/IP address'
	echo $'MAST_MPNUM : Master node mport number (usually 0)'
	echo $'SLAVE_MPNUM: Slave node mport number (usually 0)'
	echo $'All parameters after this are optional.  Default values shown.'
	echo $'DIR        : Directory on both MAST and SLAVE to run tests.'
	echo $'             Default is ' $DFLT_HOMEDIR
        echo $'WAIT       : Time in seconds to wait before performance measurement'
	echo $'             Default is ' $DFLT_WAIT_TIME
        echo $'WR_TRANS   : DMA write transaction type'
        echo $'             1 LAST_NW_R, 2 NW, 3 ALL_NW_R'
	echo $'             Default is 2 NW.'
        echo $'IBA_ADDR   : Hexadecimal RapidIO address of inbound window for both nodes'
	echo $'             Default is ' $DFLT_IBA_ADDR
        echo $'BUFC       : Hexadecimal number of transmit buffers for MBOX and DMA'
	echo $'             Default is ' $DFLT_BUFC
        echo $'STS        : Hexadecimal number of transactions completion pointers'
	echo $'             for MBOX and DMA.  Default is ' $DFLT_STS
        echo $'DMA_CHAN   : DMA channel to use for the test, allowed 2-7'
	echo $'             Default is ' $DFLT_DMA_CHAN
        echo $'MBOX_CHAN  : Mailbox channel to use for the test, allowed 2 or 3'
	echo $'             Default is ' $DFLT_MBOX_CHAN
        echo $'TX_CPU     : Specific processor core used for transmission.'
	echo $'             -1 means "any core".  Allowed range is specific'
        echo $'             to the two tested nodes.  Use "lscpu" to learn'
        echo $'             what processors are available.'
	echo $'             Default is ' $DFLT_TX_CPU ' or any isolcpu.'
        echo $'STS_CPU    : Specific processor core used to manage completions.'
	echo $'             -1 means "any core".  Allowed range is specifc'
        echo $'             to the two tested nodes.  Use "lscpu" to learn'
        echo $'             what processors are available.'
	echo $'             Default is ' $DFLT_STS_CPU ' or any isolcpu.'
        echo $'OVERRIDE   : The default for TX_CPU and STS_CPU is to use any'
        echo $'             isolcpu configured on the target platform.'
        echo $'             Entering any value forces TX_CPU and STS_CPU to'
	echo $'             ignore isolcpus and use the TX_CPU and STS_CPU'
        echo $'             values.'
	exit 1
fi;

# ensure hex values are correctly prefixed
if [[ $IBA_ADDR != 0x* ]] && [[ $IBA_ADDR != 0X* ]]; then
        IBA_ADDR=0x$IBA_ADDR
fi

if [[ $BUFC_IN != 0x* ]] && [[ $BUFC_IN != 0X* ]]; then
        BUFC_IN=0x$BUFC_IN
fi

if [[ $STS_IN != 0x* ]] && [[ $STS_IN != 0X* ]]; then
        STS_IN=0x$STS_IN
fi

echo $'\nStarting User Mode Driver regression:\n'
echo 'MAST       :' $MAST_NODE
echo 'SLAVE      :' $SLAVE_NODE
echo 'MAST_MPNUM :' $MAST_MPNUM
echo 'SLAVE_MPNUM:' $SLAVE_MPNUM
echo 'DIR        :' $HOMEDIR
echo 'WAIT TIME  :' $WAIT_TIME ' SECONDS'
echo 'TRANS      :' $WR_TRANS_IN ${INTERP_TRANS[WR_TRANS_IN]}
echo 'IBA_ADDR   :' $IBA_ADDR
echo 'BUFC       :' $BUFC_IN
echo 'STS        :' $STS_IN
echo 'DMA_CHAN   :' $DMA_CHAN_IN
echo 'MBOX_CHAN  :' $MBOX_CHAN_IN
echo 'TX_CPU     :' $TX_CPU_IN
echo 'STS_CPU    :' $STS_CPU_IN
echo 'OVERRIDE   :' $OVERRIDE_IN

if [ "$WR_TRANS_IN" == "4" ]; then
	DMA_TRANS=2
else
	DMA_TRANS=$WR_TRANS_IN
fi

if [ -n $OVERRIDE_IN ]; then
	OVERRIDE=Y
else
	OVERRIDE=
fi

# Before proceeding further, recompile ugoodput and ugoodput
# and demonstrate that each can execute the startup script

NODES=( "$MAST_NODE" "$SLAVE_NODE" )
MPNUM=( "$MAST_MPNUM" "$SLAVE_MPNUM" )
declare -a DESTIDS
declare -a CPUS
IDX=0

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

	echo $node " STARTING UGOODPUT"

	LOGNAME=mport${MPNUM[${IDX}]}'/start_target.ulog'
	LOG_FILE_DIR=${HOMEDIR}/logs/mport${MPNUM[${IDX}]}

	ssh -T root@"$node" <<NODE_START
rm -f ${LOG_FILE_DIR}/*.ulog
rm -f ${LOG_FILE_DIR}/*.ures
rm -f ${LOG_FILE_DIR}/*.uout

cd ${HOMEDIR}/scripts
./create_start_scripts.sh ${MPNUM[${IDX}]} 123 ${IBA_ADDR}

cd ..

echo $node " Quitting out of OLD ugoodput screen session, if one exists"
screen -S ugoodput -p 0 -X stuff $'quit\r'

sleep 2

screen -dmS ugoodput ./ugoodput ${MPNUM[${IDX}]} bufc=$BUFC_IN sts=$STS_IN 
screen -S ugoodput -p 0 -X stuff $'log logs/${LOGNAME}\r'
screen -S ugoodput -p 0 -X stuff $'. start_target\r'
screen -S ugoodput -p 0 -X stuff $'close\r'

lscpu >>${LOG_FILE_DIR}/cpu_info.log 
dmidecode --type memory >> ${LOG_FILE_DIR}/mem_info.log
lspci -vvvv >> ${LOG_FILE_DIR}/pcie_info.log

sleep 1
NODE_START

	if ( ! ssh -T root@"$node" test -s $HOMEDIR/logs/${LOGNAME} ); then
		echo $'\n'
		echo "$node $HOMEDIR/logs/${LOGNAME} check failed. Exiting..."
		exit 1
	fi

	DESTIDS[${IDX}]=$(ssh -T root@${node} 'grep -E dest_id '${HOMEDIR}/logs/${LOGNAME} | awk {$'print $5\r'})

	CPUS[${IDX}]=$(ssh -T root@${node} 'grep -c ^processor /proc/cpuinfo')
	echo $node " Destination ID is " ${DESTIDS[${IDX}]}
	echo $node " CPU count is      " ${CPUS[${IDX}]}
	echo $' '

	IDX=${IDX}+1
done

LABEL_LOG=${HOMEDIR}/logs/mport${MAST_MPNUM}/label.ulog

ssh -T root@${MAST_NODE} << LOG_FILE_SLAVE 
echo 'Test run started ' "$(eval date)" > $LABEL_LOG
echo 'GENERATING SCRIPTS ON SLAVE ${SLAVE_NODE}, PARAMETERS ARE' >> $LABEL_LOG
echo 'WAIT TIME  :' $WAIT_TIME ' SECONDS' >> $LABEL_LOG
echo 'TRANS       ' $DMA_TRANS ${INTERP_TRANS[DMA_TRANS]} >> $LABEL_LOG
echo 'DID        :' ${DESTIDS[0]} >> $LABEL_LOG
echo 'IBA_ADDR   : ' $IBA_ADDR >> $LABEL_LOG
echo 'MPORT_NUM  : ' $SLAVE_MPNUM >> $LABEL_LOG
echo 'BUFC       :' $BUFC_IN >> $LABEL_LOG
echo 'STS        :' $STS_IN >> $LABEL_LOG
echo 'DMA_CHAN   :' $DMA_CHAN_IN >> $LABEL_LOG
echo 'MBOX_CHAN  :' $MBOX_CHAN_IN >> $LABEL_LOG
echo 'TX_CPU     :' $TX_CPU_IN >> $LABEL_LOG
echo 'STS_CPU    :' $STS_CPU_IN >> $LABEL_LOG
echo 'OVERRIDE   :' $OVERRIDE_IN >> $LABEL_LOG
LOG_FILE_SLAVE


if [[ "${TX_CPU_IN}" -gt  "${CPUS[0]}" ]]; then
	echo "Master: Transmit CPU "${TX_CPU_IN}" out of range "${CPUS[0]}
	exit
fi

if [[ "${STS_CPU_IN}" -gt  "${CPUS[0]}" ]]; then
	echo "Master: Status CPU "${STS_CPU_IN}" out of range "${CPUS[0]}
	exit
fi

if [[ "${TX_CPU_IN}" -gt "${CPUS[1]}" ]]; then
	echo "Slave: Transmit CPU "${TX_CPU_IN}" out of range "${CPUS[1]}
	exit
fi

if [[ "${STS_CPU_IN}" -gt "${CPUS[1]}" ]]; then
	echo "Slave: Status CPU "${STS_CPU_IN}" out of range "${CPUS[1]}
	exit
fi

ssh -T root@${MAST_NODE} cat ${LABEL_LOG}

# NOTE: DESTIDS[0] below is correct, because the slave needs to know
#       the destination ID of the master.

ssh -T root@${SLAVE_NODE} >> /dev/null <<SLAVE_SCRIPT_GEN
cd $HOMEDIR/scripts
./create_umd_scripts.sh $SLAVE_MPNUM $IBA_ADDR ${DESTIDS[0]} $DMA_TRANS $WAIT_TIME $BUFC_IN $STS_IN $DMA_CHAN_IN $MBOX_CHAN_IN $TX_CPU_IN $STS_CPU_IN $OVERRIDE
SLAVE_SCRIPT_GEN

DMA_LAT_SZ=(1B 2B 4B 8B 16B 32B 64B 128B 256B 512B
	1K 2K 4K 8K 16K 32K 64K 128K 256K 512K
	1M 2M 4M)
DMA_LAT_PREFIX=udl

ssh -T root@${MAST_NODE} << LOG_FILE_MASTER 
echo 'GENERATED SCRIPTS ON MASTER '${MAST_NODE}' at '$(eval date) >> $LABEL_LOG
echo 'MASTER PARAMETERS ARE' >> $LABEL_LOG
echo 'WAIT TIME  :' $WAIT_TIME ' SECONDS' >> $LABEL_LOG
echo 'TRANS      :' $DMA_TRANS ${INTERP_TRANS[DMA_TRANS]} >> $LABEL_LOG
echo 'DID        :' ${DESTIDS[1]} >> $LABEL_LOG
echo 'IBA_ADDR   : ' $IBA_ADDR >> $LABEL_LOG
echo 'MPORT_NUM  : ' $MAST_MPNUM >> $LABEL_LOG
echo 'BUFC       :' $BUFC_IN >> $LABEL_LOG
echo 'STS        :' $STS_IN >> $LABEL_LOG
echo 'DMA_CHAN   :' $DMA_CHAN_IN >> $LABEL_LOG
echo 'MBOX_CHAN  :' $MBOX_CHAN_IN >> $LABEL_LOG
echo 'TX_CPU     :' $TX_CPU_IN >> $LABEL_LOG
echo 'STS_CPU    :' $STS_CPU_IN >> $LABEL_LOG
echo 'OVERRIDE   :' $OVERRIDE_IN >> $LABEL_LOG
LOG_FILE_MASTER

ssh -T root@${MAST_NODE} tail --lines=11 ${LABEL_LOG}

# FIXME: Could not figure out how to cat file from slave node into
#        masters LABEL_LOG`
ssh -T root@${MAST_NODE} << GET_HW_DATA
echo $'\nMASTER PLATFORM CONFIGURATION INFORMATION\n' >> $LABEL_LOG
cat ${HOMEDIR}/logs/mport${MAST_MPNUM}/cpu_info.log >> $LABEL_LOG
cat ${HOMEDIR}/logs/mport${MAST_MPNUM}/mem_info.log >> $LABEL_LOG
cat ${HOMEDIR}/logs/mport${MAST_MPNUM}/pcie_info.log >> $LABEL_LOG
echo $'\nSLAVE PLATFORM CONFIGURATION INFORMATION\n' >> $LABEL_LOG
# ssh -T root@${SLAVE_NODE} cat ${HOMEDIR}/logs/mport${SLAVE_MPNUM}/cpu_info.log >> $LABEL_LOG'
GET_HW_DATA

LOG_FILE_NAME=${HOMEDIR}/logs/mport${MAST_MPNUM}/run_all_umd_done.ulog

echo 'EXECUTING ' ${HOMEDIR}/mport${MAST_MPNUM}/run_all_umd

ssh -T root@${MAST_NODE} >> /dev/null << MAST_SCRIPT_RUN
cd $HOMEDIR/scripts
# NOTE: DESTIDS[1] below is correct, because the master needs to know
#       the destination ID of the slave
./create_umd_scripts.sh $MAST_MPNUM $IBA_ADDR ${DESTIDS[1]} $DMA_TRANS $WAIT_TIME $BUFC_IN $STS_IN $DMA_CHAN_IN $MBOX_CHAN_IN $TX_CPU_IN $STS_CPU_IN $OVERRIDE
screen -S ugoodput -p 0 -X stuff $'scrp mport${MAST_MPNUM}\r'
screen -S ugoodput -p 0 -X stuff $'. run_all_umd\r'
MAST_SCRIPT_RUN

let "TOT_WAIT = ((60 * ($WAIT_TIME + 1)) / 60) + 1"
echo 'ESTIMATING ' $TOT_WAIT ' MINUTES TO COMPLETION...'

while ( ! ssh -T root@"${MAST_NODE}" test -s ${LOG_FILE_NAME}) 
do
	sleep 60
	let "TOT_WAIT = $TOT_WAIT - 1"
	echo 'NOW ' $TOT_WAIT ' MINUTES TO COMPLETION...'
done

# Run UMSG THROUGHPUT TEST ON SLAVE, THEN MASTER, THEN CHECK FOR DONE.ULOG

echo 'EXECUTING ' ${HOMEDIR}/mport${MAST_MPNUM}/umsg_thru_tx
LOG_FILE_NAME=${HOMEDIR}/logs/mport${MAST_MPNUM}/umsg_thru_tx_done.ulog

let "TOT_WAIT = ((60 * ($WAIT_TIME + 1)) / 60) + 1"
echo 'ESTIMATING ' $TOT_WAIT ' MINUTES TO COMPLETION...'

ssh -T root@${SLAVE_NODE} >> /dev/null << SLAVE_MSG_THRU_RUN
screen -S ugoodput -p 0 -X stuff $'scrp ${HOMEDIR}/mport${SLAVE_MPNUM}/umsg_thru\r'
screen -S ugoodput -p 0 -X stuff $'. m_rx.txt\r'
SLAVE_MSG_THRU_RUN

sleep ${WAIT_TIME}

ssh -T root@${MAST_NODE} >> /dev/null << MAST_MSG_THRU_RUN
screen -S ugoodput -p 0 -X stuff $'scrp ${HOMEDIR}/mport${MAST_MPNUM}\r'
screen -S ugoodput -p 0 -X stuff $'. umsg_thru_tx\r'
MAST_MSG_THRU_RUN

while ( ! ssh -T root@"${MAST_NODE}" test -s ${LOG_FILE_NAME}) 
do
	sleep 60
	let "TOT_WAIT = $TOT_WAIT - 1"
	echo 'NOW ' $TOT_WAIT ' MINUTES TO COMPLETION...'
done

# Run UMSG LATENCY TEST ON SLAVE, THEN MASTER, THEN CHECK FOR DONE.ULOG
let "LONG_WAIT= $WAIT_TIME * 2"

echo 'EXECUTING ' ${HOMEDIR}/mport${MAST_MPNUM}/umsg_lat_tx
LOG_FILE_NAME=${HOMEDIR}/logs/mport${MAST_MPNUM}/umsg_lat_tx_done.ulog

let "TOT_WAIT = ((60 * ($WAIT_TIME + 1)) / 60) + 1"
echo 'ESTIMATING ' $TOT_WAIT ' MINUTES TO COMPLETION...'

ssh -T root@${SLAVE_NODE} >> /dev/null << SLAVE_MSG_LAT_RUN
screen -S ugoodput -p 0 -X stuff $'scrp ${HOMEDIR}/mport${SLAVE_MPNUM}/umsg_lat\r'
screen -S ugoodput -p 0 -X stuff $'. m_rx.txt\r'
SLAVE_MSG_LAT_RUN

sleep ${LONG_WAIT}

ssh -T root@${MAST_NODE} >> /dev/null << MAST_MSG_LAT_RUN
screen -S ugoodput -p 0 -X stuff $'scrp ${HOMEDIR}/mport${MAST_MPNUM}\r'
screen -S ugoodput -p 0 -X stuff $'. umsg_lat_tx\r'
MAST_MSG_LAT_RUN

while ( ! ssh -T root@"${MAST_NODE}" test -s ${LOG_FILE_NAME}) 
do
	sleep 60
	let "TOT_WAIT = $TOT_WAIT - 1"
	echo 'NOW ' $TOT_WAIT ' MINUTES TO COMPLETION...'
done

SUBDIR=mport${MAST_MPNUM}/udma_lat
echo 'EXECUTING ALL SCRIPTS IN ' ${HOMEDIR}'/'${SUBDIR}

LOGNAME=mport${MAST_MPNUM}/udma_lat_write.log
ssh -T root@${MAST_NODE} << MAST_DMA_LAT_ST
screen -S ugoodput -p 0 -X stuff $'log logs/${LOGNAME}\r'
MAST_DMA_LAT_ST

ssh -T root@${SLAVE_NODE} << SLAVE_DMA_LAT_ST
screen -S ugoodput -p 0 -X stuff $'log logs/${LOGNAME}\r'
SLAVE_DMA_LAT_ST

for SZ in ${DMA_LAT_SZ[@]}; do
	# Run slave target loop for write latency test 
	SCRIPTNAME='udlT'${SZ}'.txt'
	SUBDIR=mport${SLAVE_MPNUM}/udma_lat
	echo ${SLAVE_NODE} ${SCRIPTNAME}
	ssh -T root@"${SLAVE_NODE}"  << SLAVE_DMA_LAT_WR
screen -S ugoodput -p 0 -X stuff $'scrp ${SUBDIR}\r'
screen -S ugoodput -p 0 -X stuff $'. ${SCRIPTNAME}\r'
SLAVE_DMA_LAT_WR
	sleep 2
	
	# Run master loop for write latency test 
	SCRIPTNAME='udlW'${SZ}'.txt'
	SUBDIR=mport${MAST_MPNUM}/udma_lat
	echo ${MAST_NODE} ${SCRIPTNAME}
	ssh -T root@"${MAST_NODE}"  << MAST_DMA_LAT_WR
screen -S ugoodput -p 0 -X stuff $'scrp ${SUBDIR}\r'
screen -S ugoodput -p 0 -X stuff $'. ${SCRIPTNAME}\r'
MAST_DMA_LAT_WR
	sleep ${LONG_WAIT}

	ssh -T root@"${MAST_NODE}"  << MAST_DMA_LAT_END
screen -S ugoodput -p 0 -X stuff $'kill 0\r'
screen -S ugoodput -p 0 -X stuff $'wait 0 d\r'
MAST_DMA_LAT_END
	sleep 2
done

ssh -T root@${MAST_NODE} << MAST_DMA_LAT_ST
screen -S ugoodput -p 0 -X stuff $'close\r'
echo 'Test run finished ' "$(eval date)" >> $LABEL_LOG
MAST_DMA_LAT_ST

ssh -T root@${SLAVE_NODE} << SLAVE_DMA_LAT_ST
screen -S ugoodput -p 0 -X stuff $'close\r'
SLAVE_DMA_LAT_ST

# Now start analyzing/checking output.

ssh -T root@${MAST_NODE} << MAST_LOGS_CHECK
cd ${HOMEDIR}/logs/mport${MAST_MPNUM}
./summ_thru_logs.sh ulog ures > all_thru.ures
./summ_lat_logs.sh ulog ures > all_lat.ures
./check_uthru_logs.sh all_thru.ures
./check_ulat_logs.sh all_lat.ures
MAST_LOGS_CHECK

# If there was a failure, keep the ugoodput sessions around
# for debug purposes.
if [ -s ${HOMEDIR}/logs/mport${MAST_MPNUM}/thru_fail.out ]; then
	exit;
fi;

if [ -s ${HOMEDIR}/logs/mport${MAST_MPNUM}/lat_fail.out ]; then
	exit;
fi;

for node in ${NODES[@]}; do
	ssh -T root@"$node" <<NODE_STOP
cd ${HOMEDIR}/scripts
echo $node " Stopping ugoodput sessions, if they exist."
screen -S ugoodput -p 0 -X stuff $'quit\r'
sleep 10
NODE_STOP

exit
