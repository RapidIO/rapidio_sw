#!/bin/bash

WAIT_TIME=30

if [ -z "$1" ]
  then
    WAIT_TIME=$1
fi

echo GENERATING SCRIPTS WITH WAIT TIME OF $WAIT_TIME SECONDS

/bin/bash dma_thru/create_scripts.sh $WAIT_TIME
/bin/bash pdma_thru/create_scripts.sh $WAIT_TIME
/bin/bash dma_lat/create_scripts.sh $WAIT_TIME
/bin/bash msg_thru/create_scripts.sh $WAIT_TIME
/bin/bash obwin_thru/create_scripts.sh $WAIT_TIME
/bin/bash obwin_lat/create_scripts.sh $WAIT_TIME
