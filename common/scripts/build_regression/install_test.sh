#!/bin/bash

# Installation test
# Uses error codes 20-39

REMOTE_ROOT="/opt/rapidio/.install"
LOCAL_SOURCE_ROOT="$(pwd)"/../../..
SCRIPTS_PATH=$LOCAL_SOURCE_ROOT/install

NODEDATA_FILE="nodeData.txt"
SRC_TAR="rapidio_sw.tar"
TMPL_FILE="config.tmpl"

PGM_NAME=install_test.sh
PGM_NUM_PARMS=5

# Validate input
#
PRINTHELP=0

if [ "$#" -lt $PGM_NUM_PARMS ]; then
    echo $'\n$PGM_NAME requires $PGM_NUM_PARMS parameters.\n'
    PRINTHELP=1
else
    SERVER=$1
    NDATA=$2
    MEMSZ=$3
    SW_TYPE=$4
    GRP=$5
    REL=$6

    if [ $MEMSZ != 'mem34' -a $MEMSZ != 'mem50' -a $MEMSZ != 'mem66' ] ; then
        echo $'\nmemsz parameter must be mem34, mem50, or mem66.\n'
        PRINTHELP=1
    fi

    MASTER_CONFIG_FILE=$SCRIPTS_PATH/$SW_TYPE-master.conf
    MASTER_MAKE_FILE=$SCRIPTS_PATH/$SW_TYPE-master-make.sh

    if [ ! -e "$MASTER_CONFIG_FILE" ] || [ ! -e "$MASTER_MAKE_FILE" ]
    then
        echo $'\nSwitch type \"$SW_TYPE\" configuration support files do not exist.\n'
        PRINTHELP=1
    fi
fi

if [ $PRINTHELP = 1 ] ; then
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
    exit 20
fi

echo ""
echo "Start $PGM_NAME"
echo "`date`"
echo ""

# Initialize common properties
if [ ! -e ./install_common.sh ]; then
    echo "File install_common.sh not found"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 21
fi
. ./install_common.sh $NDATA
if [ $? -ne 0 ]; then
	echo "[${BASH_SOURCE[0]}:${LINENO}]"
	exit 22
fi

# Adjust logging directory
LOG_DIR=$LOG_DIR/install
if [ ! -d $LOG_DIR ]; then
    mkdir -p $LOG_DIR
fi

# Only proceed if all nodes can be reached
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

for host in "${ALLNODES[@]}"
do
    [ "$host" = "$SERVER" ] && continue;
    ping -c 1 $host > /dev/null
    if [ $? -ne 0 ]; then
        echo "    $host not accessible"
        OK=0
    else
        echo "    $host accessible."
    fi
done

if [ $OK -eq 0 ]; then
    echo "\nCould not connect to all nodes, exiting..."
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 23
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

# Copy the template file
#
cp $MASTER_CONFIG_FILE $TMP_DIR/$TMPL_FILE

# Transfer the files to the server
#
echo "Transferring install files to $SERVER..."
SERVER_ROOT="/opt/rapidio/.server"
ssh "$MY_USERID"@"$SERVER" "rm -rf $SERVER_ROOT;mkdir -p $SERVER_ROOT"
scp $TMP_DIR/* "$MY_USERID"@"$SERVER":$SERVER_ROOT/. > /dev/null
ssh "$MY_USERID"@"$SERVER" "chown -R root.$GRP $SERVER_ROOT"

# Transfer the make_install.sh script to a known location on the target machines
#
for host in "${ALLNODES[@]}"; do
    echo "Transferring install script to $host..."
    ssh "$MY_USERID"@"$host" "rm -rf $REMOTE_ROOT;mkdir -p $REMOTE_ROOT/script"
    scp $SCRIPTS_PATH/make_install_common.sh "$MY_USERID"@"$host":$REMOTE_ROOT/script/make_install_common.sh > /dev/null
    if [ "$host" = "$MASTER" ]; then
        scp $MASTER_MAKE_FILE "$MY_USERID"@"$host":$REMOTE_ROOT/script/make_install.sh > /dev/null
    else
        scp $SCRIPTS_PATH/make_install-slave.sh "$MY_USERID"@"$host":$REMOTE_ROOT/script/make_install.sh > /dev/null
    fi
    ssh "$MY_USERID"@"$host" "chown -R root.$GRP $REMOTE_ROOT;chmod 755 $REMOTE_ROOT/script/make_install.sh"
done


# Transfer source to master for test compiles and unit tests runs
REMOTE_SOURCE_DIR=$REMOTE_ROOT/testCompile
ssh "$MY_USERID"@"$MASTER" "mkdir -p $REMOTE_SOURCE_DIR"
if [ $? -ne 0 ]; then
    rm -rf $TMP_DIR &> /dev/null
    echo "Could not create $REMOTE_SOURCE_DIR on $MASTER"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 24
fi

scp $TMP_DIR/$SRC_TAR "$MY_USERID"@"$MASTER":$REMOTE_SOURCE_DIR/$SRC_TAR &> /dev/null
ssh "$MY_USERID"@"$MASTER" "pushd $REMOTE_SOURCE_DIR; tar -xomf $SRC_TAR > /dev/null"
rm -rf $TMP_DIR &> /dev/null

# ensure make works
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s" |& tee $LOG_DIR/01_make-compile.txt
tmp=`grep ^make $LOG_DIR/01_make-compile.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Compile failed, check log file $LOG_DIR/01_make-compile.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 25
fi

ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s clean" |& tee $LOG_DIR/02_make-clean.txt
tmp=`grep ^make $LOG_DIR/02_make-clean.txt | grep Error | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Clean failed, check log file $LOG_DIR/02_make-clean.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 26
fi

# execute unit tests on master
ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s TEST=1" |& tee $LOG_DIR/03_make-test-compile.txt
tmp=`grep ^make $LOG_DIR/03_make-test-compile.txt | grep Error | wc -l`
if [ $? -ne 0 ]; then
    echo "Compile (test) failed, check log file $LOG_DIR/03_make-test-compile.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 27
fi

ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s TEST=1 runtests" |& tee $LOG_DIR/04_make-runtests.txt
tmp=`grep ^make $LOG_DIR/04_make-runtests.txt | grep Error | wc -l`
if [ $? -ne 0 ]; then
    echo "Unit tests failed, check log file $LOG_DIR/04_make-runtests.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 28
fi

ssh "$MY_USERID"@"$MASTER" "cd $REMOTE_SOURCE_DIR ; make -s TEST=1 clean" |& tee $LOG_DIR/05_make-clean.txt
tmp=`grep ^make $LOG_DIR/05_make-clean.txt | grep Error | wc -l`
if [ $? -ne 0 ]; then
    echo "Clean (tests) failed, check log file $LOG_DIR/05_make-clean.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 29
fi

ssh "$MY_USERID"@"$MASTER" "rm -rf $REMOTE_SOURCE_DIR"
if [ $? -ne 0 ]; then
    echo "Could not delete $REMOTE_SOURCE_DIR from $MASTER"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 30
fi

# Put target machines into a steady state
OK=1
touch $LOG_DIR/06_process-kill.txt
for node in ${REVNODES[@]}
do
    # Kill application processes
    for name in ${PROCESS_NAMES[@]}
    do
        THE_PID=$(ssh "$MY_USERID"@"$node" pgrep -x $name)
        echo "Killing $name on $node PID=$THE_PID" |& tee -a $LOG_DIR/06_process-kill.txt
        for proc in $THE_PID
        do
            ssh "$MY_USERID"@"$node" "kill -s 2 $proc" |& tee -a $LOG_DIR/06_process-kill.txt
        done
    done

    sleep 3

    # Check they were successfully killed
    for name in ${PROCESS_NAMES[@]}
    do
        THE_PID=$(ssh "$MY_USERID"@"$node" pgrep -x $name)
        if [ ! -z "$THE_PID" ]
        then
            echo "$name killed but still alive with PID=$THE_PID!" |& tee -a $LOG_DIR/06_process-kill.txt
            OK=0
        fi
    done
    echo ""
done

if [ $OK -eq 0 ]; then
    echo "Error killing running processes, check log file $LOG_DIR/06_process-kill.txt"
     echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 31
fi


# Call out to make_install.sh
echo "Beginning installation..."
for host in "${ALLNODES[@]}"; do
    ssh "$MY_USERID"@"$host" "$REMOTE_ROOT/script/make_install.sh $SERVER $SERVER_ROOT $MEMSZ $GRP" |& tee $LOG_DIR/07_install.txt
done


# Check for the install
OK=1
touch $LOG_DIR/08_install-check.txt
echo ""
for host in "${ALLNODES[@]}"; do
    echo "Verifying $INSTALL_ROOT exists on $host" |& tee -a $LOG_DIR/08_install-check.txt
    ssh "$MY_USERID"@"$host" [ ! -d $INSTALL_ROOT ]
    if [ $? -eq 0 ]; then
        echo "  Install directory $INSTALL_ROOT does not exist on $host" |& tee -a $LOG_DIR/08_install-check.txt
        OK=0
    fi

    ssh "$MY_USERID"@"$host" [ ! -e $INSTALL_ROOT/LICENSE ]
    if [ $? -eq 0 ]; then
        echo "  File $INSTALL_ROOT/LICENSE does not exist on $host" |& tee -a $LOG_DIR/08_install-check.txt
        OK=0
    fi
done

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 32
fi

OK=1
echo "Verifying all_start.sh exists on $MASTER" |& tee -a $LOG_DIR/08_install-check.txt
ssh "$MY_USERID"@"$MASTER" [ ! -e $INSTALL_ROOT/all_start.sh ]
if [ $? -eq 0 ]; then
    echo "File $INSTALL_ROOT/all_start.sh does not exist"  |& tee -a $LOG_DIR/08_install-check.txt
    OK=0
fi

echo "Verifying stop_all.sh exists on $MASTER" |& tee -a $LOG_DIR/08_install-check.txt
ssh "$MY_USERID"@"$MASTER" [ ! -e $INSTALL_ROOT/stop_all.sh ]
if [ $? -eq 0 ]; then
    echo "File $INSTALL_ROOT/stop_all.sh does not exist" |& tee -a $LOG_DIR/08_install-check.txt
    OK=0
fi

echo "Verifying check_all.sh exists on $MASTER" |& tee -a $LOG_DIR/08_install-check.txt
ssh "$MY_USERID"@"$MASTER" [ ! -e $INSTALL_ROOT/check_all.sh ]
if [ $? -eq 0 ]; then
    echo "File $INSTALL_ROOT/check_all.sh does not exist" |& tee -a $LOG_DIR/08_install-check.txt
    OK=0
fi

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 33
fi

echo ""
echo "$PGM_NAME complete"
echo "Logs: $LOG_DIR"
echo "`date`"
echo ""
