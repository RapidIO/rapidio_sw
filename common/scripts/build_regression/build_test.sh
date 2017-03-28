#!/bin/bash

# Wrapper for build regression testing
#     Installs a new software image
#     Runs basic regression
#

# Store each build using a unique postfix
BUILD_ID=`date +%Y%m%d-%H%M`

PGM_NAME=build_test.sh
PGM_NUM_PARMS=5

# Parse options
PRINTHELP=0
if [ "$#" -lt $PGM_NUM_PARMS ]; then
    echo $'\n$PGM_NAME requires $PGM_NUM_PARMS parameters.\n'
    PRINTHELP=1
else
    BT_ARG_SERVER=$1
    BT_ARG_NDATA=$2
    BT_ARG_MEMSZ=$3
    BT_ARG_SW=$4
    BT_ARG_GRP=$5
    BT_ARG_REL=$6
fi

if [ $PRINTHELP -eq 1 ]; then
    echo "$PGM_NAME <SERVER> <nData> <memsz> <sw> <group> <rel>"
    echo "<SERVER> Name of the node providing the files required by installation"
    echo "<nData>  The file describing the target nodes of the install"
    echo "         The file has the format:"
    echo "         <master|slave> <IP_Name> <RIO_name> <node>"
    echo "         Where:"
    echo "         <IP_name> : IP address or DNS name of the node"
    echo "         <RIO_name>: Fabric management node name."
    echo "         <node>    : String to replace in template file,"
    echo "                     of the form node#."
    echo "         EXAMPLE: master 10.64.15.199 gry37 node1"
    echo "         NOTE: Example nodeData.sh files are create by install.sh"
    echo "<memsz>  RapidIO memory size, one of mem34, mem50, mem66"
    echo "         If any node has more than 8 GB of memory, MUST use mem50"
    echo "<sw>     Type of switch the four nodes are connected to."
    echo "         Files exist for the following switch types:"
    echo "         tor  - Prodrive Technologies Top of Rack Switch"
    echo "         cps  - StarBridge Inc RapidExpress Switch"
    echo "         auto - configuration determined at runtime"
    echo "         rxs  - StarBridge Inc RXS RapidExpress Switch"
    echo "<group>  Unix file ownership group which should have access to"
    echo "         the RapidIO software"
    echo "<rel>    The software release/version to install."
    echo "         If no release is supplied, the current release is installed."
    exit 1
fi

echo ""
echo "Start $PGM_NAME"
echo "`date`"
echo ""

# Initialize common properties 
# Uses error codes 10-19
if [ ! -e ./install_common.sh ]; then
    echo "File install_common.sh not found"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 2
fi

. ./install_common.sh $BT_ARG_NDATA
if [ $? -ne 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 3
fi

# Cleanup old builds, limit consumed disk space
if [ ! -d $OUTPUT_DIR ]; then
    mkdir -p $OUTPUT_DIR
else
    find $OUTPUT_DIR/* -type d &> /dev/null
    if [ $? -eq 0 ]; then
        find $OUTPUT_DIR/* -maxdepth 0 -type d -printf '%Ts\t%p\n' | sort -nr | cut -f2 | tail -n +$MAX_BUILDS | xargs rm -rf
    fi
fi

# Uses error codes 140-159
mkdir -p $LOG_DIR/compile
. ./compile_test.sh $BT_ARG_SERVER $BT_ARG_NDATA |& tee $LOG_DIR/compile/000_all.txt
if [ $? -ne 0 ]; then
    echo "Compile test failed"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 4
fi

# Uses error codes 20-39
mkdir -p $LOG_DIR/install
. ./install_test.sh $BT_ARG_SERVER $BT_ARG_NDATA $BT_ARG_MEMSZ $BT_ARG_SW $BT_ARG_GRP $BT_ARG_REL |& tee $LOG_DIR/install/00_all.txt
if [ $? -ne 0 ]; then
    echo "Basic installation failed"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 5
fi

# Uses error codes 40-49
mkdir -p $LOG_DIR/system
. ./system_start_test.sh $BT_ARG_NDATA |& tee $LOG_DIR/system/00_all.txt
if [ $? -ne 0 ]; then
    echo "System start test failed"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 6
fi

# Uses error codes 50-79
mkdir -p $LOG_DIR/fmd
. ./fmd_test.sh $BT_ARG_NDATA |& tee $LOG_DIR/fmd/00_all.txt
if [ $? -ne 0 ]; then
    echo "FMD test failed"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 7
fi

# Uses error codes 80-109
mkdir -p $LOG_DIR/file_xfer
. ./fxfr_test.sh $BT_ARG_NDATA |& tee $LOG_DIR/fxfr/00_all.txt
if [ $? -ne 0 ]; then
    echo "File transfer test failed"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 8
fi

# Uses error codes 110-139
mkdir -p $LOG_DIR/goodput
. ./goodput_test.sh $BT_ARG_NDATA |& tee $LOG_DIR/goodput/00_all.txt
if [ $? -ne 0 ]; then
    echo "Goodput test failed"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 9
fi

echo ""
echo "$PGM_NAME complete"
echo "Logs: $LOG_DIR"
echo "`date`"
