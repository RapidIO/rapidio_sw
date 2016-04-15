#!/bin/bash

LOC_PRINT_HEP=0
OVERRIDE='N'

if [ -n "$1" ]; then
	MPORT=$1; shift
else
	LOC_PRINT_HEP=1
fi

if [ -n "$1" ]; then
	IBA_ADDR=$1; shift
else
	LOC_PRINT_HEP=1
fi

if [ -n "$1" ]; then
	DID=$1; shift
else
	LOC_PRINT_HEP=1
fi

if [ -n "$1" ]; then
	TRANS=$1; shift
else
	LOC_PRINT_HEP=1
fi

if [ -n "$1" ]; then
	WAIT_TIME=$1; shift
else
	LOC_PRINT_HEP=1
fi

if [ -n "$1" ]; then
	BUFC=$1; shift
else
	LOC_PRINT_HEP=1
fi

if [ -n "$1" ]; then
	STS=$1; shift
else
	LOC_PRINT_HEP=1
fi

if [ -n "$1" ]; then
	CHANNEL=$1; shift
else
	LOC_PRINT_HEP=1
fi
if [ -n "$1" ]; then
	MBOX=$1; shift
else
	LOC_PRINT_HEP=1
fi

if [ -n "$1" ]; then
	TX_CPU=$1; shift
else
	LOC_PRINT_HEP=1
fi

if [ -n "$1" ]; then
	FIFO_CPU=$1; shift
else
	LOC_PRINT_HEP=1
fi

if [ -n "$1" ]; then
	OVERRIDE='Y'; shift
fi

if [ $LOC_PRINT_HEP != "0" ]; then
	echo $'\nScript requires the following parameters:'
        echo $'MPORT    : /dev/rio_mport{MPORT} device to use for test.'
        echo $'IBA_ADDR : Hex address of target window on DID'
        echo $'DID      : Device ID of target device for performance scripts'
        echo $'Wr_TRANS : UDMA Write transaction type'
        echo $'           1 LAST_NWR, 2 NW, 3 NW_R'
        echo $'Wait     : Time in seconds to wait before taking perf measurement'
        echo $'Bufc     : Number of TX buffers'
        echo $'Sts      : size of TX FIFO'
        echo $'CHANNEL  : Tsi721 DMA channel, 2 through 7'
        echo $'MBOX     : Tsi721 MBOX channel, 2 through 3'
        echo $'TX_CPU   : Processor to run the trasnmit/receive loop'
        echo $'FIFO_CPU : Processor to run the completion FIFO loop'
        echo $'OVERRIDE : <optional>, default and 'N' allows isolcpus'
        echo $'           Any other value forces TX_CPU and FIFO_CPU'
	exit 1
fi;

CPU_COUNT=$( grep -c ^processor /proc/cpuinfo );
let MAX_CPU=$CPU_COUNT-1;

if [ $TX_CPU -ge $CPU_COUNT ]; then
	echo "Invalid TX_CPU=$TX_CPU. Valid range is 0..$MAX_CPU" 1>&2
	exit 1;
fi
if [[ "$FIFO_CPU" -ge "$CPU_COUNT" ]]; then
	echo "Invalid FIFO_CPU=$FIFO_CPU. Valid range is 0..$MAX_CPU" 1>&2
	exit 1;
fi

INTERP_WR_TRANS=(LAST_NW NW NW_R);

MPORT_DIR='mport'${MPORT}

echo $'\nGENERATING USER MODE DRIVER PERFORMANCE SCRIPTS WITH\n'
echo $'MPORT    = '$MPORT
echo $'IBA_ADDR = '$IBA_ADDR
echo $'DID      = '$DID
echo $'Wr_TRANS = '$TRANS ${INTERP_WR_TRANS[TRANS]}
echo $'WAIT_TIME= '$WAIT_TIME
echo $'BUFC     = '$BUFC
echo $'STS      = '$STS
echo $'CHANNEL  = '$CHANNEL
echo $'MBOX     = '$MBOX
echo $'TX_CPU   = '$TX_CPU
echo $'FIFO_CPU = '$FIFO_CPU
echo $'OVERRIDE = '$OVERRIDE
echo $'MPORT_DIR= '$MPORT_DIR

# Create mport specific directory for performance scripts,
# and generate the scripts.
cd ..
mkdir -m 777 -p $MPORT_DIR
cp -r scripts/performance/* ${MPORT_DIR}

cd ${MPORT_DIR}/udma_thru
source create_scripts_umd.sh $MPORT_DIR $IBA_ADDR $DID $TRANS $WAIT_TIME $BUFC $STS $CHANNEL $TX_CPU $FIFO_CPU $OVERRIDE 

cd ../udma_lat
source create_scripts_umd.sh $MPORT_DIR $IBA_ADDR $DID $TRANS $WAIT_TIME $BUFC $STS $CHANNEL $TX_CPU $FIFO_CPU $OVERRIDE

cd ../umsg_thru
source create_scripts_umd.sh $MPORT_DIR $DID $MBOX $WAIT_TIME $BUFC $STS $TX_CPU $FIFO_CPU $OVERRIDE
popd &>/dev/null

cd ../umsg_lat
source create_scripts_umd.sh $MPORT_DIR $DID $MBOX $WAIT_TIME $BUFC $STS $TX_CPU $FIFO_CPU $OVERRIDE 

cd ../..

cp scripts/run_all_umd ${MPORT_DIR}
sed -i -- 's/MPORT_DIR/'$MPORT_DIR'/g' ${MPORT_DIR}/run_all_umd

find ${MPORT_DIR} -type f -perm 664 -exec chmod 666 {} \;
find ${MPORT_DIR} -type f -perm 775 -exec chmod 777 {} \;
find ${MPORT_DIR} -type d -perm 775 -exec chmod 777 {} \;
