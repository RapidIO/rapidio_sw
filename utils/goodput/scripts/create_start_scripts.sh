#!/bin/bash

PRINT_HELP=0
SKT_PREFIX=234
IBA_ADDR=200000000

if [ -n "$1" ]
  then
    SKT_PREFIX=$1
else
        PRINT_HELP=1
fi

if [ -n "$2" ]
  then
    IBA_ADDR=$2
else
        PRINT_HELP=1
fi

if [ $PRINT_HELP != "0" ]; then
        echo $'\nScript to create Goodput setup scripts.'
        echo $'\nScript requires the following parameters:'
        echo $'SOCKET_PFX : First 3 digits of 4 digit socket numbers i.e. 123'
        echo $'IBA_ADDR   : RapidIO address of inbound window on DID'
	echo $'\nNo parameters entered, scripts not generated...'
        exit 1
fi;

echo GENERATING GOODPUT SETUP SCRIPTS WITH
echo 'SOCKET_PFX : ' $SKT_PREFIX
echo 'IBA_ADDR   : ' $IBA_ADDR

cp 'template_st_targ' 'start_target'
sed -i -- 's/skt_prefix/'$SKT_PREFIX'/g' 'start_target'
sed -i -- 's/iba_addr/'$IBA_ADDR'/g' 'start_target'

cp 'template_st_src' 'start_source'
sed -i -- 's/iba_addr/'$IBA_ADDR'/g' 'start_source'
