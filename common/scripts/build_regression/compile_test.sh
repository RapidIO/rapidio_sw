#!/bin/bash

# Installation test
# Uses error codes 140-159

REMOTE_ROOT="/opt/rapidio/.install"
LOCAL_SOURCE_ROOT="$(pwd)"/../../..

NODEDATA_FILE="nodeData.txt"
SRC_TAR="rapidio_sw.tar"

PGM_NAME=compile_test.sh
PGM_NUM_PARMS=2

# Validate input
#
PRINTHELP=0

if [ "$#" -lt $PGM_NUM_PARMS ]; then
    echo $'\n$PGM_NAME requires $PGM_NUM_PARMS parameters.\n'
    PRINTHELP=1
else
    SERVER=$1
    NDATA=$2
fi

if [ $PRINTHELP = 1 ] ; then
    echo "$PGM_NAME <SERVER> <nData> <memsz> <sw> <group> <rel>"
    echo "<SERVER> Name of the node providing the files required by compile"
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
    exit 140
fi

echo ""
echo "Start $PGM_NAME"
echo "`date`"
echo ""

# Initialize common properties
if [ ! -e ./install_common.sh ]; then
    echo "File install_common.sh not found"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 141
fi
. ./install_common.sh $NDATA
if [ $? -ne 0 ]; then
	echo "[${BASH_SOURCE[0]}:${LINENO}]"
	exit 142
fi

# Adjust logging directory
LOG_DIR=$LOG_DIR/compile
if [ ! -d $LOG_DIR ]; then
    mkdir -p $LOG_DIR
fi

# Only proceed if SERVER and MASTER can be reached
#
echo "Prepare for installation..."
echo "Checking connectivity..."
OK=1
ping -c 1 $SERVER > /dev/null
if [ $? -ne 0 ]; then
    echo "    $SERVER not accessible"
    OK=0
else
    echo "    $SERVER accessible."
fi

ping -c 1 $MASTER > /dev/null
if [ $? -ne 0 ]; then
    echo "    $MASTER not accessible"
    OK=0
else
    echo "    $MASTER accessible."
fi

if [ $OK -eq 0 ]; then
    echo "\nCould not connect to $SERVER or $MASTER, exiting..."
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 143
fi


echo "Creating install files for $SERVER..."
# First create the files that would be available on the server
#
TMP_DIR="/tmp/$$"
rm -rf $TMP_DIR;mkdir -p $TMP_DIR

# Copy nodeData.txt
#
cp $NDATA $TMP_DIR/$NODEDATA_FILE

# Create the source.tar
#
pushd $LOCAL_SOURCE_ROOT &> /dev/null
make clean &>/dev/null
tar -cf $TMP_DIR/$SRC_TAR * .git* &>/dev/null
popd &> /dev/null

# Transfer the files to the server
#
echo "Transferring install files to $SERVER..."
SERVER_ROOT="/opt/rapidio/.server"
ssh "$MY_USERID"@"$SERVER" "rm -rf $SERVER_ROOT;mkdir -p $SERVER_ROOT"
scp $TMP_DIR/* "$MY_USERID"@"$SERVER":$SERVER_ROOT/. > /dev/null
ssh "$MY_USERID"@"$SERVER" "chown -R root.$GRP $SERVER_ROOT"

# Transfer source to master for test compiles
REMOTE_SOURCE_DIR=$REMOTE_ROOT/testCompile
ssh "$MY_USERID"@"$MASTER" "mkdir -p $REMOTE_SOURCE_DIR"
if [ $? -ne 0 ]; then
    rm -rf $TMP_DIR &> /dev/null
    echo "Could not create $REMOTE_SOURCE_DIR on $MASTER"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 144
fi

scp $TMP_DIR/$SRC_TAR "$MY_USERID"@"$MASTER":$REMOTE_SOURCE_DIR/$SRC_TAR &> /dev/null
ssh "$MY_USERID"@"$MASTER" "pushd $REMOTE_SOURCE_DIR; tar -xomf $SRC_TAR > /dev/null"
rm -rf $TMP_DIR &> /dev/null

echo ""
echo "BEGIN TEST: make -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s clean" |& tee $LOG_DIR/01_make-clean.txt
tmp=`grep ^make $LOG_DIR//01_make-clean.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/01_make-clean.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 145
fi

OK=1
echo ""
echo "BEGIN TEST: make -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s" |& tee $LOG_DIR/02_make-compile-default.txt
tmp=`grep ^make $LOG_DIR/02_make-compile-default.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/02_make-compile-default.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s clean" |& tee $LOG_DIR/03_make-clean-default.txt
tmp=`grep ^make $LOG_DIR/03_make-clean-default.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/03_make-clean-default.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 146
fi

echo ""
echo "BEGIN TEST: make -s TEST=1"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s TEST=1" |& tee $LOG_DIR/04_make-compile-test-default.txt
tmp=`grep ^make $LOG_DIR/04_make-compile-test-default.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile (test), check log file $LOG_DIR/04_make-compile-test-default.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make -s TEST=1 clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s TEST=1 clean" |& tee $LOG_DIR/05_make-clean-test-default.txt
tmp=`grep ^make $LOG_DIR/05_make-clean-test-default.txt | grep Error | wc -l`
if [ $? -ne 0 ]; then
    echo "Clean (test) failed, check log file $LOG_DIR/05_make-clean-test-default.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 147
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='' -s" |& tee $LOG_DIR/06_make-compile-librio-none.txt
tmp=`grep ^make $LOG_DIR/06_make-compile-librio-none.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/06_make-compile-librio-none.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='' -s clean" |& tee $LOG_DIR/07_make-clean-librio-none.txt
tmp=`grep ^make $LOG_DIR/07_make-clean-librio-none.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/07_make-clean-librio-none.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='' -s" |& tee $LOG_DIR/08_make-compile-test-librio-none.txt
tmp=`grep ^make $LOG_DIR/08_make-compile-test-librio-none.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile (test) failed, check log file $LOG_DIR/08_make-compile-test-librio-none.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='' -s clean" |& tee $LOG_DIR/09_make-clean-test-librio-none.txt
tmp=`grep ^make $LOG_DIR/09_make-clean-test-librio-none.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean (test) failed, check log file $LOG_DIR/09_make-clean-test-librio-none.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 148
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED' -s" |& tee $LOG_DIR/10_make-compile-librio-tsi57x.txt
tmp=`grep ^make $LOG_DIR/10_make-compile-librio-tsi57x.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/10_make-compile-librio-tsi57x.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED' -s clean" |& tee $LOG_DIR/11_make-clean-librio-tsi57x.txt
tmp=`grep ^make $LOG_DIR/11_make-clean-librio-tsi57x.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/11_make-clean-librio-tsi57x.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED' -s" |& tee $LOG_DIR/12_make-compile-test-librio-tsi57x.txt
tmp=`grep ^make $LOG_DIR/12_make-compile-test-librio-tsi57x.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile (test) failed, check log file $LOG_DIR/12_make-compile-test-librio-tsi57x.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED' -s clean" |& tee $LOG_DIR/13_make-clean-test-librio-tsi57x.txt
tmp=`grep ^make $LOG_DIR/13_make-clean-test-librio-tsi57x.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean (test) failed, check log file $LOG_DIR/13_make-clean-test-librio-tsi57x.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 149
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DCPS_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DCPS_DAR_WANTED' -s" |& tee $LOG_DIR/14_make-compile-librio-cps.txt
tmp=`grep ^make $LOG_DIR/14_make-compile-librio-cps.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/14_make-compile-librio-cps.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DCPS_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DCPS_DAR_WANTED' -s clean" |& tee $LOG_DIR/15_make-clean-librio-cps.txt
tmp=`grep ^make $LOG_DIR/15_make-clean-librio-cps.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/15_make-clean-librio-cps.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DCPS_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DCPS_DAR_WANTED' -s" |& tee $LOG_DIR/16_make-compile-test-librio-cps.txt
tmp=`grep ^make $LOG_DIR/16_make-compile-test-librio-cps.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile (test) failed, check log file $LOG_DIR/16_make-compile-test-librio-cps.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DCPS_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DCPS_DAR_WANTED' -s clean" |& tee $LOG_DIR/17_make-clean-test-librio-cps.txt
tmp=`grep ^make $LOG_DIR/17_make-clean-test-librio-cps.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean (test) failed, check log file $LOG_DIR/17_make-clean-test-librio-cps.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 150
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DRXS_DAR_WANTED' -s"
# ensure make works
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DRXS_DAR_WANTED' -s" |& tee $LOG_DIR/18_make-compile-librio-rxs.txt
tmp=`grep ^make $LOG_DIR/18_make-compile-librio-rxs.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/18_make-compile-librio-rxs.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DRXS_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DRXS_DAR_WANTED' -s clean" |& tee $LOG_DIR/19_make-clean-librio-rxs.txt
tmp=`grep ^make $LOG_DIR/19_make-clean-librio-rxs.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/19_make-clean-librio-rxs.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DRXS_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DRXS_DAR_WANTED' -s" |& tee $LOG_DIR/20_make-compile-test-librio-rxs.txt
tmp=`grep ^make $LOG_DIR/20_make-compile-test-librio-rxs.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/20_make-compile-test-librio-rxs.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DRXS_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DRXS_DAR_WANTED' -s clean" |& tee $LOG_DIR/21_make-clean-test-librio-rxs.txt
tmp=`grep ^make $LOG_DIR/21_make-clean-test-librio-rxs.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/21_make-clean-test-librio-rxs.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 151
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI721_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI721_DAR_WANTED' -s" |& tee $LOG_DIR/22_make-compile-librio-tsi721.txt
tmp=`grep ^make $LOG_DIR/22_make-compile-librio-tsi721.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/22_make-compile-librio-tsi721.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI721_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI721_DAR_WANTED' -s clean" |& tee $LOG_DIR/23_make-clean-librio-tsi721.txt
tmp=`grep ^make $LOG_DIR/23_make-clean-librio-tsi721.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/23_make-clean-librio-tsi721.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI721_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI721_DAR_WANTED' -s" |& tee $LOG_DIR/24_make-compile-test-librio-tsi721.txt
tmp=`grep ^make $LOG_DIR/24_make-compile-test-librio-tsi721.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/24_make-compile-test-librio-tsi721.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI721_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI721_DAR_WANTED' -s clean" |& tee $LOG_DIR/25_make-clean-test-librio-tsi721.txt
tmp=`grep ^make $LOG_DIR/25_make-clean-test-librio-tsi721.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/25_make-clean-test-librio-tsi721.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 152
fi


echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s" |& tee $LOG_DIR/26_make-compile-librio-tsi721-int.txt
tmp=`grep ^make $LOG_DIR/26_make-compile-librio-tsi721-int.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/26_make-compile-librio-tsi721-int.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-TSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-TSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s clean" |& tee $LOG_DIR/27_make-clean-librio-tsi721-int.txt
tmp=`grep ^make $LOG_DIR/27_make-clean-librio-tsi721-int.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/27_make-clean-librio-tsi721-int.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s" |& tee $LOG_DIR/28_make-compile-test-librio-tsi721-int.txt
tmp=`grep ^make $LOG_DIR/28_make-compile-test-librio-tsi721-int.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile (test) failed, check log file $LOG_DIR/28_make-compile-test-librio-tsi721-int.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-TSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-TSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s clean" |& tee $LOG_DIR/29_make-clean-test-librio-tsi721-int.txt
tmp=`grep ^make $LOG_DIR/29_make-clean-test-librio-tsi721-int.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean (test) failed, check log file $LOG_DIR/29_make-clean-test-librio-tsi721-int.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 153
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DCPS_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DCPS_DAR_WANTED' -s" |& tee $LOG_DIR/30_make-compile-librio-tsi57x-cps.txt
tmp=`grep ^make $LOG_DIR/30_make-compile-librio-tsi57x-cps.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/30_make-compile-librio-tsi57x-cps.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DCPS_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DCPS_DAR_WANTED' -s clean" |& tee $LOG_DIR/31_make-clean-librio-tsi57x-cps.txt
tmp=`grep ^make $LOG_DIR/31_make-clean-librio-tsi57x-cps.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/31_make-clean-librio-tsi57x-cps.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DCPS_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DCPS_DAR_WANTED' -s" |& tee $LOG_DIR/32_make-compile-test-librio-tsi57x-cps.txt
tmp=`grep ^make $LOG_DIR/32_make-compile-test-librio-tsi57x-cps.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile (test) failed, check log file $LOG_DIR/32_make-compile-test-librio-tsi57x-cps.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DCPS_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DCPS_DAR_WANTED' -s clean" |& tee $LOG_DIR/33_make-clean-test-librio-tsi57x-cps.txt
tmp=`grep ^make $LOG_DIR/33_make-clean-test-librio-tsi57x-cps.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean (test) failed, check log file $LOG_DIR/33_make-clean-test-librio-tsi57x-cps.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 154
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DRXS_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DRXS_DAR_WANTED' -s" |& tee $LOG_DIR/34_make-compile-librio-tsi57x-rxs.txt
tmp=`grep ^make $LOG_DIR/34_make-compile-librio-tsi57x-rxs.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/34_make-compile-librio-tsi57x-rxs.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DRXS_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DRXS_DAR_WANTED' -s clean" |& tee $LOG_DIR/35_make-clean-librio-tsi57x-rxs.txt
tmp=`grep ^make $LOG_DIR/35_make-clean-librio-tsi57x-rxs.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/35_make-clean-librio-tsi57x-rxs.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DRXS_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DRXS_DAR_WANTED' -s" |& tee $LOG_DIR/36_make-compile-test-librio-tsi57x-rxs.txt
tmp=`grep ^make $LOG_DIR/36_make-compile-test-librio-tsi57x-rxs.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile (test) failed, check log file $LOG_DIR/36_make-compile-test-librio-tsi57x-rxs.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DRXS_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DRXS_DAR_WANTED' -s clean" |& tee $LOG_DIR/37_make-clean-test-librio-tsi57x-rxs.txt
tmp=`grep ^make $LOG_DIR/37_make-clean-test-librio-tsi57x-rxs.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean (test) failed, check log file $LOG_DIR/37_make-clean-test-librio-tsi57x-rxs.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 155
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_DAR_WANTED' -s" |& tee $LOG_DIR/38_make-compile-librio-tsi57x-tsi721.txt
tmp=`grep ^make $LOG_DIR/38_make-compile-librio-tsi57x-tsi721.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/38_make-compile-librio-tsi57x-tsi721.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_DAR_WANTED' -s clean" |& tee $LOG_DIR/39_make-clean-librio-tsi57x-tsi721.txt
tmp=`grep ^make $LOG_DIR/39_make-clean-librio-tsi57x-tsi721.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/39_make-clean-librio-tsi57x-tsi721.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_DAR_WANTED' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_DAR_WANTED' -s" |& tee $LOG_DIR/40_make-compile-test-librio-tsi57x-tsi721.txt
tmp=`grep ^make $LOG_DIR/40_make-compile-test-librio-tsi57x-tsi721.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile (test) failed, check log file $LOG_DIR/40_make-compile-test-librio-tsi57x-tsi721.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_DAR_WANTED' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_DAR_WANTED' -s clean" |& tee $LOG_DIR/41_make-clean-test-librio-tsi57x-tsi721.txt
tmp=`grep ^make $LOG_DIR/41_make-clean-test-librio-tsi57x-tsi721.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean (test) failed, check log file $LOG_DIR/41_make-clean-test-librio-tsi57x-tsi721.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 156
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s" |& tee $LOG_DIR/42_make-compile-librio-tsi57x-tsi721-int.txt
tmp=`grep ^make $LOG_DIR/42_make-compile-librio-tsi57x-tsi721-int.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/42_make-compile-librio-tsi57x-tsi721-int.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s clean"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s clean" |& tee $LOG_DIR/43_make-clean-librio-tsi57x-tsi721-int.txt
tmp=`grep ^make $LOG_DIR/43_make-clean-librio-tsi57x-tsi721-int.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/43_make-clean-librio-tsi57x-tsi721-int.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s" |& tee $LOG_DIR/44_make-compile-test-librio-tsi57x-tsi721-int.txt
tmp=`grep ^make $LOG_DIR/44_make-compile-test-librio-tsi57x-tsi721-int.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile (test) failed, check log file $LOG_DIR/44_make-compile-test-librio-tsi57x-tsi721-int.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

echo ""
echo "BEGIN TEST: make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT -s clean'"
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='-DTSI57X_DAR_WANTED -DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT' -s clean" |& tee $LOG_DIR/45_make-clean-test-librio-tsi57x-tsi721-int.txt
tmp=`grep ^make $LOG_DIR/45_make-clean-test-librio-tsi57x-tsi721-int.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean (test) failed, check log file $LOG_DIR/45_make-clean-test-librio-tsi57x-tsi721-int.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    OK=0
fi

if [ $OK -eq 0 ]; then
    exit 157
fi

# We could keep going ...

ssh "$MY_USERID"@"$MASTER" "rm -rf $REMOTE_SOURCE_DIR"
if [ $? -ne 0 ]; then
    echo "Could not delete $REMOTE_SOURCE_DIR from $MASTER"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 159
fi

echo ""
echo "$PGM_NAME complete"
echo "Logs: $LOG_DIR"
echo "`date`"
echo ""
