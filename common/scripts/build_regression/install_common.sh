#!/bin/bash

# Default common properties for install regression testing

L_PGM_NAME=install_common.sh
L_PGM_NUM_PARMS=1

# Validate input
#
PRINTHELP=0

if [ "$#" -lt $L_PGM_NUM_PARMS ]; then
    echo $'\n$L_PGM_NAME requires $L_PGM_NUM_PARMS parameters.\n'
    PRINTHELP=1
else
    NDATA=$1
fi

if [ $PRINTHELP = 1 ] ; then
    echo "$L_PGM_NAME <nData>"
    echo "<nData>  The file describing the target nodes of the install"
    echo "         The file has the format:"
    echo "         <master|slave> <IP_Name> <RIO_name> <node>"
    echo "         Where:"
    echo "         <IP_name> : IP address or DNS name of the node"
    echo "         <RIO_name>: Fabric management node name."
    echo "         <node>    : String to replace in template file,"
    echo "                     of the form node#."
    echo "         EXAMPLE: master 10.64.15.199 gry37 node1"
    exit 10
fi

# Do not call multiple times, Searching for $MASTER in the nodeData file ($NDATA) will fail
if [ -n "$INSTALL_COMMON_COMPLETE" ]; then
    return 0
fi

# Used to uniquely identify the build, must be set by calling script
if [ ! -v BUILD_ID ]; then
    echo "The variable BUILD_ID must be set"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 11
elif [ -z "$BUILD_ID" ]; then
    echo "The variable BUILD_ID must not be the empty string"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 12
fi

# User performing test
MY_USERID=root

# Location of build results on host running install
OUTPUT_DIR=/tmp/buildTest

# Location of log files on host running install
LOG_DIR=$OUTPUT_DIR/$BUILD_ID

# Number of builds to retain (n-1)
MAX_BUILDS=11

# Location of install on remote server
INSTALL_ROOT=/opt/rapidio/rapidio_sw

# fmd process info
FMD_PROC=fmd
FMD_NAME=FMD
FMD_PATH=$INSTALL_ROOT/fabric_management/daemon/fmd

# rrmapcli process info
RRMAP_PROC=rrmapcli
RRMAP_NAME=RRMAPCLI
RRMAP_PATH=$INSTALL_ROOT/utils/rrmapcli/rrmapcli

# goodput process info
GOODPUT_PROC=goodput
GOODPUT_NAME=GOODPUT
GOODPUT_PATH=$INSTALL_ROOT/utils/goodput/goodput

# File transfer process info
FXFR_PROC=fxfr_server
FXFR_NAME=FXFR_SERVER
FXFR_ROOT=$INSTALL_ROOT/utils/file_transfer
FXFR_PATH=$FXFR_ROOT/fxfr_server

# system processes
PROCESS_NAMES=( $RRMAP_PROC $GOODPUT_PROC $FMD_PROC fxfr_server rftp )

# when an operation may need some time to complete
SLEEP_INTERVAL=1
LONG_SLEEP_INTERVAL=3

MAX_ITERATIONS=3

# Nodes to operate on
OK=1
tmp=();
# format of input file: <master|slave> <hostname> <rioname> <nodenumber>
while read -r line || [[ -n "$line" ]]; do
    arr=($line)
    host="${arr[1]}"
    if [ "${arr[0]}" = 'master' ]; then
        if [ -n "$MASTER" ]; then
            echo "Multiple master entries ($line) in $NDATA"
            OK=0
        fi
        MASTER=$host
    else
        tmp+=("$host")
    fi
done < "$NDATA"

if [ -z "$MASTER" ]; then
    echo "No master entry in $NDATA"
    OK=0
fi

if [ $OK -eq 0 ]; then
    echo "Errors in nodeData file $NDATA, exiting... [${BASH_SOURCE[0]}:${LINENO}]"
    exit 13
fi

# array with master node as first entry
ALLNODES=("$MASTER")
ALLNODES+=("${tmp[@]}")

# array with master node as last entry
REVNODES=("${tmp[@]}")
REVNODES+=("$MASTER")

# flag to avoid repeated initialization
INSTALL_COMMON_COMPLETE=1