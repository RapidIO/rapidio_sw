#!/bin/bash

WAIT_TIME=30
DID=0
TRANS=0
IBA_ADDR=20d800000
RX_DID=1
RX_IBA_ADDR=20d800000
ACC_SIZE=40000
BYTES=400000
SYNC=0
SYNC2=$SYNC
SYNC3=$SYNC
PRINT_HELP=0

if [ -n "$1" ]
  then
    WAIT_TIME=$1
else
	PRINT_HELP=1
fi

if [ -n "$2" ]
  then
    TRANS=$2
else
	PRINT_HELP=1
fi

if [ -n "$3" ]
  then
    SYNC=$3
else
	PRINT_HELP=1
fi

if [ -n "$4" ]
  then
    DID=$4
else
	PRINT_HELP=1
fi

if [ -n "$5" ]
  then
    IBA_ADDR=$5
else
	PRINT_HELP=1
fi

if [ -n "$6" ]
  then
    RX_DID=$6
else
	PRINT_HELP=1
fi

if [ -n "$7" ]
  then
    RX_IBA_ADDR=$7
else
	PRINT_HELP=1
fi

if [ -n "$8" ]
  then
    SOCKET_PFX=$8
else
	PRINT_HELP=1
fi

if [ -n "$9" ]
  then
    SYNC2=$9
else
	SYNC2=$SYNC
fi

if [ -n "${10}" ]
  then
    SYNC3=${10}
else
	SYNC3=$SYNC
fi

if [ $PRINT_HELP != "0" ]; then
	echo $'\nScript requires the following parameters:'
	echo $'WAIT       : Time in seconds to wait before perf measurement'
	echo $'DMA_TRANS  : DMA transaction type'
	echo $'             0 NW, 1 SW, 2 NW_R, 3 SW_R 4 NW_R_ALL'
	echo $'DMA_SYNC   : 0 - blocking, 1 - async, 2 - fire and forget'
	echo $'DID        : Device ID of target device for performance scripts'
	echo $'IBA_ADDR   : Hex address of target window on DID'
	echo $'TX_DID     : Device ID of this device'
	echo $'TX_IBA_ADDR: Hex address of local target window (TX_DID)'
	echo $'SOCKET_PFX : First 3 digits of 4 digit socket numbers'
	echo $'\nOptional parameters, if not entered same as DMA_SYNC'
	echo $'DMA_SYNC2  : 0 - blocking, 1 - async, 2 - fire and forget'
	echo $'DMA_SYNC3  : 0 - blocking, 1 - async, 2 - fire and forget'
	exit 1
fi;

INTERP_TRANS=(NW SW NW_R SW_R NW_R_ALL);
INTERP_SYNC=(BLOCK ASYNC FAF);

echo GENERATING SCRIPTS WITH
echo 'WAIT TIME  :' $WAIT_TIME SECONDS
echo 'TRANS      :' $TRANS ${INTERP_TRANS[TRANS]}
echo 'SYNC       :' $SYNC  ${INTERP_SYNC[SYNC]}
echo 'SYNC2      :' $SYNC2 ${INTERP_SYNC[SYNC2]}
echo 'SYNC3      :' $SYNC3 ${INTERP_SYNC[SYNC3]}
echo 'DID        :' $DID
echo 'IBA_ADDR   :' $IBA_ADDR
echo 'RX_DID     :' $RX_DID
echo 'RX_IBA_ADDR:' $RX_IBA_ADDR
echo 'SOCKET_PFX :' $SOCKET_PFX 

## Completed
cd dma_thru
source create_scripts.sh $WAIT_TIME $DID $TRANS $IBA_ADDR $SYNC
cd ..
## Completed
cd pdma_thru
source create_scripts.sh $WAIT_TIME $DID $TRANS $IBA_ADDR $SYNC
cd ..
## Completed Read
## Write???
cd dma_lat
source create_scripts.sh $IBA_ADDR $DID $RX_IBA_ADDR $RX_DID $TRANS $WAIT_TIME
cd ..
## Completed
cd msg_thru
source create_scripts.sh $SOCKET_PFX $DID $WAIT_TIME
cd ..
cd obwin_thru
source create_scripts.sh $IBA_ADDR $DID $TRANS $WAIT_TIME
cd ..
cd obwin_lat
source create_scripts.sh $IBA_ADDR $DID $RX_IBA_ADDR $RX_DID $TRANS $WAIT_TIME
cd ..
cd msg_lat
source create_scripts.sh $SOCKET_PFX $DID $WAIT_TIME
