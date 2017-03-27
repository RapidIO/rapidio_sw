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

LOGNAME=$LOG_DIR/001_make-clean-default.txt
echo ""
echo "BEGIN CMD: make -s clean" |& tee $LOGNAME
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s clean" |& tee -a $LOGNAME
tmp=`grep ^make $LOGNAME | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $$LOGNAME"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 145
fi

LOGNAME=$LOG_DIR/002_make-compile-default.txt
echo ""
echo "BEGIN CMD: make -s" |& tee $LOGNAME
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s" |& tee -a $LOGNAME
tmp=`grep ^make $LOGNAME | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOGNAME"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 146
fi
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s clean"

# Start LOGNUM at current value as it is incremented prior to creating any new files
LOGNUM=02

# will compile the various permutations of these defines
SRC_CFG_LIBRIO_DEFINES=(-DTSI57X_DAR_WANTED -DCPS_DAR_WANTED -DRXS_DAR_WANTED -DTSI721_DAR_WANTED -DTSI721_EXCLUDE_EM_INTERRUPT_SUPPORT)
SRC_CFG_LIBRIO_NAMES=(tsi57x cps rxs tsi721 em)

n=${#SRC_CFG_LIBRIO_DEFINES[@]}
powersize=$((1 << $n))

OK=1
i=0
while [ $i -lt $powersize ]
do
    subset=()
    names=()
    j=0
    while [ $j -lt $n ]
    do
        if [ $(((1 << $j) & $i)) -gt 0 ]
        then
            subset+=("${SRC_CFG_LIBRIO_DEFINES[$j]}")
            names+=("-${SRC_CFG_LIBRIO_NAMES[$j]}")
        fi
        j=$(($j + 1))
    done

    POSTFIX=""
    for name in "${names[@]}"
    do
       POSTFIX+=$name 
    done
    POSTFIX+=".txt"

    ((LOGNUM++))
    DISP_LOGNUM=$(printf "%03d" $LOGNUM)
    LOGNAME=$LOG_DIR/$DISP_LOGNUM
    LOGNAME+="_make-compile"
    LOGNAME+=$POSTFIX
    echo ""
    echo "BEGIN CMD: make SRC_CFG_LIBRIO='${subset[@]}' -s" |& tee $LOGNAME
    ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='${subset[@]}' -s" |& tee -a $LOGNAME
    tmp=`grep ^make $LOGNAME | grep Error | wc -l`
    if [ $tmp -ne 0 ]; then
        echo "Compile failed, check log file $LOGNAME.txt"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        OK=0
    fi

    ((LOGNUM++))
    DISP_LOGNUM=$(printf "%03d" $LOGNUM)
    LOGNAME=$LOG_DIR/$DISP_LOGNUM
    LOGNAME+="_make-clean"
    LOGNAME+=$POSTFIX
    echo ""
    echo "BEGIN CMD: make SRC_CFG_LIBRIO='${subset[@]}' -s clean" |& tee $LOGNAME
    ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make SRC_CFG_LIBRIO='${subset[@]}' -s clean" |& tee -a $LOGNAME
    tmp=`grep ^make $LOGNAME | grep Error | wc -l`
    if [ $tmp -ne 0 ]; then
        echo "Clean failed, check log file $LOGNAME.txt"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        OK=0
    fi

    ((LOGNUM++))
    DISP_LOGNUM=$(printf "%03d" $LOGNUM)
    LOGNAME=$LOG_DIR/$DISP_LOGNUM
    LOGNAME+="_make-compile-test"
    LOGNAME+=$POSTFIX
    echo ""
    echo "BEGIN CMD: make TEST=1 SRC_CFG_LIBRIO='${subset[@]}' -s" |& tee $LOGNAME
    ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='${subset[@]}' -s" |& tee -a $LOGNAME
    tmp=`grep ^make $LOGNAME | grep Error | wc -l`
    if [ $tmp -ne 0 ]; then
        echo "Compile (test) failed, check log file $LOGNAME.txt"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        OK=0
    fi

    ((LOGNUM++))
    DISP_LOGNUM=$(printf "%03d" $LOGNUM)
    LOGNAME=$LOG_DIR/$DISP_LOGNUM
    LOGNAME+="_make-clean-test"
    LOGNAME+=$POSTFIX
    echo ""
    echo "BEGIN CMD: make TEST=1 SRC_CFG_LIBRIO='${subset[@]}' -s clean" |& tee $LOGNAME
    ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make TEST=1 SRC_CFG_LIBRIO='${subset[@]}' -s clean" |& tee -a $LOGNAME
    tmp=`grep ^make $LOGNAME | grep Error | wc -l`
    if [ $tmp -ne 0 ]; then
        echo "Clean (test) failed, check log file $LOGNAME.txt"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        OK=0
    fi

    if [ $OK -eq 0 ]; then
        exit 147
    fi
    i=$(($i + 1))
done

ssh "$MY_USERID"@"$MASTER" "rm -rf $REMOTE_SOURCE_DIR"
if [ $? -ne 0 ]; then
    echo "Could not delete $REMOTE_SOURCE_DIR from $MASTER"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 148
fi

echo ""
echo "$PGM_NAME complete"
echo "Logs: $LOG_DIR"
echo "`date`"
echo ""
