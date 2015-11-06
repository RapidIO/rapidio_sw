#!/bin/bash

LOC_PRINT_HEP=0
OVERRIDE='N'

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
        echo $'IBA_ADDR : Hex address of target window on DID'
        echo $'DID      : Device ID of target device for performance scripts'
        echo $'Wr TRANS : UDMA Write transaction type'
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
if [ $FIFO_CPU -ge $CPU_COUNT ]; then
	echo "Invalid FIFO_CPU=$FIFO_CPU. Valid range is 0..$MAX_CPU" 1>&2
	exit 1;
fi

INTERP_WR_TRANS=(LAST_NW NW NW_R);

echo $'\nGENERATING USER MODE DRIVER PERFORMANCE SCRIPTS WITH\n'
echo $'IBA_ADDR = '$IBA_ADDR
echo $'DID      = '$DID
echo $'Wr TRANS = '$TRANS ${INTERP_WR_TRANS[TRANS]}
echo $'WAIT_TIME= '$WAIT_TIME
echo $'BUFC     = '$BUFC
echo $'STS      = '$STS
echo $'CHANNEL  = '$CHANNEL
echo $'MBOX     = '$MBOX
echo $'TX_CPU   = '$TX_CPU
echo $'FIFO_CPU = '$FIFO_CPU
echo $'OVERRIDE = '$OVERRIDE

pushd performance/udma_thru &>/dev/null || exit 1
sh create_scripts_umd.sh $IBA_ADDR $DID $TRANS $WAIT_TIME $BUFC $STS $CHANNEL $TX_CPU $FIFO_CPU $OVERRIDE || exit 2
popd &>/dev/null

pushd performance/udma_lat &>/dev/null || exit 1
sh create_scripts_umd.sh $IBA_ADDR $DID $TRANS $WAIT_TIME $BUFC $STS $CHANNEL $TX_CPU $FIFO_CPU $OVERRIDE || exit 2
popd &>/dev/null

pushd performance/umsg_thru &>/dev/null || exit 1
sh create_scripts_umd.sh $DID $MBOX $WAIT_TIME $BUFC $STS $TX_CPU $FIFO_CPU $OVERRIDE || exit 2
popd &>/dev/null

pushd performance/umsg_lat &>/dev/null || exit 1
sh create_scripts_umd.sh $DID $MBOX $WAIT_TIME $BUFC $STS $TX_CPU $FIFO_CPU $OVERRIDE || exit 2
popd &>/dev/null
