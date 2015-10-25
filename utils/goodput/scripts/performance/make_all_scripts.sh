#!/bin/bash

WAIT_TIME=30
DID=0
TRANS=0
IBA_ADDR=20d800000
RX_DID=1
RX_IBA_ADDR=20d800000
ACC_SIZE=40000
BYTES=400000
PRINT_HELP=0

if [ -n "$1" ]
  then
    WAIT_TIME=$1
else
	PRINT_HELP=1
fi

if [ -n "$2" ]
  then
    DID=$2
else
	PRINT_HELP=1
fi

if [ -n "$3" ]
  then
    TRANS=$3
else
	PRINT_HELP=1
fi

if [ -n "$4" ]
  then
    IBA_ADDR=$4
else
	PRINT_HELP=1
fi

if [ -n "$5" ]
  then
    RX_DID=$5
else
	PRINT_HELP=1
fi

if [ -n "$6" ]
  then
    RX_IBA_ADDR=$6
else
	PRINT_HELP=1
fi

if [ -n "$7" ]
  then
    SOCKET_PFX=$7
else
	PRINT_HELP=1
fi

if [ $PRINT_HELP != "0" ]; then
	echo $'\nScript requires the following parameters:'
	echo $'Wait: Time in seconds to wait before taking perf measurement'
	echo $'DID : Device ID of target device for performance scripts'
	echo $'Trans: DMA transaction type'
	echo $'IBA_ADDR: Hex address of target window on DID'
	echo $'TX_DID: Device ID of this device'
	echo $'TX_IBA_ADDR: Hex address of local target window (TX_DID)'
	echo $'SOCKET_PFX: First 3 digits of 4 digit socket numbers\n'
	exit 1
fi;

echo GENERATING SCRIPTS WITH
echo WAIT TIME  : $WAIT_TIME SECONDS
echo DID        : $DID
echo TRANS      : $TRANS
echo IBA_ADDR   : $IBA_ADDR
echo RX_DID     : $RX_DID
echo RX_IBA_ADDR: $RX_IBA_ADDR
echo SOCKET_PFX : $SOCKET_PFX 

cd dma_thru
source create_scripts.sh $WAIT_TIME $DID $TRANS $IBA_ADDR
cd ..
cd pdma_thru
source create_scripts.sh $WAIT_TIME $DID $TRANS $IBA_ADDR
cd ..
cd dma_lat
source create_scripts.sh $IBA_ADDR $DID $RX_IBA_ADDR $RX_DID $TRANS $WAIT_TIME
cd ..
cd msg_thru
source create_scripts.sh $SOCKET_PFX $DID $WAIT_TIME
cd ..
cd obwin_thru
source create_scripts.sh $WAIT_TIME
cd ..
cd obwin_lat
source create_scripts.sh $WAIT_TIME
