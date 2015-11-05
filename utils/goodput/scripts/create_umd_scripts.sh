#!/bin/bash

LOC_PRINT_HEP=0

if [ -n "$1" ]; then
	IBA_ADDR=$1
else
	LOC_PRINT_HEP=1
fi

if [ -n "$2" ]; then
	DID=$2
else
	LOC_PRINT_HEP=1
fi

if [ -n "$3" ]; then
	TRANS=$3
else
	LOC_PRINT_HEP=1
fi

if [ -n "$4" ]; then
	WAIT_TIME=$4
else
	LOC_PRINT_HEP=1
fi

if [ -n "$5" ]; then
	BUFC=$5
else
	LOC_PRINT_HEP=1
fi

if [ -n "$6" ]; then
	STS=$6
else
	LOC_PRINT_HEP=1
fi

if [ -n "$7" ]; then
	CPU_1=$7
else
	LOC_PRINT_HEP=1
fi

if [ -n "$8" ]; then
	CPU_2=$8
else
	LOC_PRINT_HEP=1
fi

if [ -n "$9" ]; then
	CHANNEL=$9
else
	LOC_PRINT_HEP=1
fi

if [ $LOC_PRINT_HEP != "0" ]; then
	echo $'\nScript requires the following parameters:'
        echo $'IBA_ADDR: Hex address of target window on DID'
        echo $'DID     : Device ID of target device for performance scripts'
        echo $'Wr TRANS: UDMA Write transaction type'
        echo $'          1 LAST_NWR, 2 NW, 3 NW_R'
        echo $'Wait    : Time in seconds to wait before taking perf measurement'
        echo $'Bufc    : Number of TX buffers'
        echo $'Sts     : size of TX FIFO'
        echo $'CPU_1   : Core for the DMA transmit thread'
        echo $'CPU_2   : Core for the DMA transmit FIFO thread'
        echo $'CHANNEL : Tsi721 DMA channel, 2 through 7'
	exit 1
fi;

INTERP_WR_TRANS=(LAST_NW NW NW_R);

echo $'\nGENERATING USER MODE DRIVER PERFORMANCE SCRIPTS WITH\n'
echo $'IBA_ADDR = '$IBA_ADDR
echo $'DID      = '$DID
echo $'Wr TRANS = '$TRANS ${INTERP_WR_TRANS[TRANS]}
echo $'WAIT_TIME= '$WAIT_TIME
echo $'BUFC     = '$BUFC
echo $'STS      = '$STS
echo $'CPU_1    = '$CPU_1
echo $'CPU_2    = '$CPU_2
echo $'CHANNEL  = '$CHANNEL


cd performance/udma_thru
source create_scripts.sh $IBA_ADDR $DID $TRANS $WAIT_TIME $BUFC $STS $CPU_1 $CPU_2 $CHANNEL
cd ../..
