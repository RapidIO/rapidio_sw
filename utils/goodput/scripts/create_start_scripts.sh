#!/bin/bash

PRINT_HELP=0
SKT_PREFIX=234
IBA_ADDR=200000000

if [ -n "$1" ]
  then
    MPORT=$1
else
        PRINT_HELP=1
fi

if [ -n "$2" ]
  then
    SKT_PREFIX=$2
else
        PRINT_HELP=1
fi

if [ -n "$3" ]
  then
    IBA_ADDR=$3
else
        PRINT_HELP=1
fi

if [ $PRINT_HELP != "0" ]; then
        echo $'\nScript to create Goodput setup scripts.'
        echo $'\nScript requires the following parameters:'
        echo $'MPORT      : Mport number usually 0'
        echo $'SOCKET_PFX : First 3 digits of 4 digit socket numbers i.e. 123'
        echo $'IBA_ADDR   : RapidIO address of inbound window on DID'
	echo $'\nNo parameters entered, scripts not generated...'
        exit 1
fi;

MPORT_DIR=mport${MPORT}

echo GENERATING GOODPUT AND UGOODPUT START SCRIPTS WITH
echo 'MPORT      : ' $MPORT       
echo 'MPORT_DIR  : ' $MPORT_DIR
echo 'SOCKET_PFX : ' $SKT_PREFIX
echo 'IBA_ADDR   : ' $IBA_ADDR

cd ..

# Create mport specific directory for log files, include analysis scripts
mkdir -m 777 -p logs
chmod 777 logs
mkdir -m 777 -p logs/${MPORT_DIR}
chmod 777 logs/${MPORT_DIR}

cp logs/*.sh logs/${MPORT_DIR}/

find logs/${MPORT_DIR} -type f -perm 664 -exec chmod 666 {} \;
find logs/${MPORT_DIR} -type f -perm 775 -exec chmod 777 {} \;
find logs/${MPORT_DIR} -type d -perm 775 -exec chmod 777 {} \;

mkdir -m 777 -p $MPORT_DIR

cp 'scripts/afu' $MPORT_DIR/afu
cp 'scripts/dmav' $MPORT_DIR/dmav
cp 'scripts/epdel' $MPORT_DIR/epdel
cp 'scripts/epwatch' $MPORT_DIR/epwatch
cp 'scripts/lumsg' $MPORT_DIR/lumsg
cp 'scripts/mboxwatch' $MPORT_DIR/mboxwatch
cp 'scripts/s' $MPORT_DIR/s
cp 'scripts/t' $MPORT_DIR/t
cp 'scripts/template_st_targ' $MPORT_DIR/start_target
cp 'scripts/template_st_src' $MPORT_DIR/start_source
cp 'scripts/udmatun' $MPORT_DIR/udmatun
cp 'scripts/ugoodput_info' $MPORT_DIR/ugoodput_info
cp 'scripts/udmatun' $MPORT_DIR/udmatun

find ${MPORT_DIR} -type f -perm 664 -exec chmod 666 {} \;
find ${MPORT_DIR} -type f -perm 644 -exec chmod 666 {} \;
find ${MPORT_DIR} -type f -perm 775 -exec chmod 777 {} \;
find ${MPORT_DIR} -type d -perm 775 -exec chmod 777 {} \;

cd $MPORT_DIR

sed -i -- 's/MPORT_DIR/'$MPORT_DIR'/g' 'start_target'
sed -i -- 's/skt_prefix/'$SKT_PREFIX'/g' 'start_target'
sed -i -- 's/iba_addr/'$IBA_ADDR'/g' 'start_target'

sed -i -- 's/MPORT_DIR/'$MPORT_DIR'/g' 'start_source'
sed -i -- 's/iba_addr/'$IBA_ADDR'/g' 'start_source'

sed -i -- 's/iba_addr/'$IBA_ADDR'/g' 'ugoodput_info'
