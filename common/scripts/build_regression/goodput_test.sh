#!/bin/bash

# Goodput test
# Uses error codes 110-139

GP_WAIT=30

PGM_NAME=goodput_test.sh
PGM_NUM_PARMS=1

# Validate input
#
PRINTHELP=0

if [ "$#" -lt $PGM_NUM_PARMS ]; then
    echo $'\n$PGM_NAME requires $PGM_NUM_PARMS parameters.\n'
    PRINTHELP=1
else
    NDATA=$1
fi

if [ $PRINTHELP = 1 ] ; then
    echo "$PGM_NAME <nData>"
    echo "<nData>  The file describing the target nodes of the install"
    echo "         The file has the format:"
    echo "         <master|slave> <IP_Name> <RIO_name> <node>"
    echo "         Where:"
    echo "         <IP_name> : IP address or DNS name of the node"
    echo "         <RIO_name>: Fabric management node name."
    echo "         <node>    : String to replace in template file,"
    echo "                     of the form node#."
    echo "         EXAMPLE: master 10.64.15.199 gry37 node1"
    exit 110
fi

echo ""
echo "Start $PGM_NAME"
echo "`date`"
echo ""

# Initialize common properties
if [ ! -e ./install_common.sh ]; then
    echo "File install_common.sh not found"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 111
fi
. ./install_common.sh $NDATA
if [ $? -ne 0 ]; then
	echo "[${BASH_SOURCE[0]}:${LINENO}]"
	exit 112
fi

# Adjust logging directory
LOG_DIR=$LOG_DIR/goodput
if [ ! -d $LOG_DIR ]; then
    mkdir -p $LOG_DIR
fi

# Put target machines into a steady state
OK=1
touch $LOG_DIR/01_process-kill.txt
for node in ${REVNODES[@]}
do
    # Kill application processes
    for name in ${PROCESS_NAMES[@]}
    do
        THE_PID=$(ssh "$MY_USERID"@"$node" pgrep -x $name)
        echo "Killing $name on $node" |& tee -a $LOG_DIR/01_process-kill.txt
        for proc in $THE_PID
        do
            ssh "$MY_USERID"@"$node" "kill -s 2 $proc" |& tee -a $LOG_DIR/01_process-kill.txt
        done
    done

    sleep $SLEEP_INTERVAL

    # Check they were successfully killed
    for name in ${PROCESS_NAMES[@]}
    do
        THE_PID=$(ssh "$MY_USERID"@"$node" pgrep -x $name)
        if [ ! -z "$THE_PID" ]
        then
            for proc in $THE_PID
            do
                echo "$name killed but still alive with PID=$proc!" |& tee -a $LOG_DIR/01_process-kill.txt
            done
            OK=0
        fi
    done
    echo ""
done

if [ $OK -eq 0 ]; then
    echo "Error killing running processes, check log file $LOG_DIR/01_process-kill.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 113
fi

# Verify the executables exist
OK=1
touch $LOG_DIR/02_check-files.txt
echo "Checking for $GOODPUT_PATH on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
ssh "$MY_USERID"@"$MASTER" [ ! -e $GOODPUT_PATH ]
if [ $? -eq 0 ]; then
    echo "  $GOODPUT_PATH does not exist on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
    OK=0
fi

echo "Checking for all_start.sh on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
ssh "$MY_USERID"@"$MASTER" [ ! -e $INSTALL_ROOT/all_start.sh ]
if [ $? -eq 0 ]; then
    echo "File $INSTALL_ROOT/all_start.sh does not exist" |& tee -a $LOG_DIR/02_check-files.txt
    OK=0
fi

echo "Checking for stop_all.sh on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
ssh "$MY_USERID"@"$MASTER" [ ! -e $INSTALL_ROOT/stop_all.sh ]
if [ $? -eq 0 ]; then
    echo "File $INSTALL_ROOT/stop_all.sh does not exist" |& tee -a $LOG_DIR/02_check-files.txt
    OK=0
fi

echo "Checking for check_all.sh on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
ssh "$MY_USERID"@"$MASTER" [ ! -e $INSTALL_ROOT/check_all.sh ]
if [ $? -eq 0 ]; then
    echo "File $INSTALL_ROOT/check_all.sh does not exist" |& tee -a $LOG_DIR/02_check-files.txt
    OK=0
fi

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 114
fi

echo "Starting FMD processes"
ssh "$MY_USERID"@"$MASTER" $INSTALL_ROOT/all_start.sh |& tee $LOG_DIR/03_start-processes.txt
sleep $SLEEP_INTERVAL

echo "Checking processes"
ssh "$MY_USERID"@"$MASTER" $INSTALL_ROOT/check_all.sh |& tee $LOG_DIR/04_post-check.txt

# Verify everything is started
OK=1
tmp=`grep FMD $LOG_DIR/04_post-check.txt | grep NOT | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Processes not started on all devices, check log file $LOG_DIR/04_post-check.txt"
    OK=0
fi

# Verify check_all reported back correctly
for node in ${ALLNODES[@]}
do
    tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $FMD_PROC)
    if [ $? -ne 0 ]; then
        echo "$FMD_NAME not started on $node" |& tee $LOG_DIR/04_post-check.txt
        OK=0
    fi
done

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 115
fi

echo ""
for ((i=1;i<=$MAX_ITERATIONS;i++))
do
    echo "Starting $GOODPUT_NAME, iteration $i"
    ssh "$MY_USERID"@"$MASTER" screen -dmS $GOODPUT_PROC $GOODPUT_PATH
    sleep $SLEEP_INTERVAL

    echo "Checking $GOODPUT_NAME"
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $GOODPUT_PROC)
    if [ $? -ne 0 ]; then
        echo "$GOODPUT_NAME not started on $node"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 116
    fi

    echo "Stopping $GOODPUT_NAME"
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $GOODPUT_PROC -X stuff "quit\r"
        sleep $SLEEP_INTERVAL
        exit
EOF

    echo "Checking $GOODPUT_NAME"
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $GOODPUT_PROC)
    if [ $? -eq 0 ]; then
        echo "$GOODPUT_NAME killed but still alive with PID=$tmp"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 117
    fi
    echo ""
done

# Run long running regression test
OK=1
ssh "$MY_USERID"@"$MASTER" $INSTALL_ROOT/utils/goodput/test/regression.sh $MASTER ${ALLNODES[1]} 0 0 $INSTALL_ROOT/utils/goodput $GP_WAIT |& tee $LOG_DIR/05_goodput.txt
tmp=`grep "FAIL: Keywords" $LOG_DIR/05_goodput.txt | wc -l`
if [ $tmp -ne 0 ]; then
    OK=0
fi

tmp=`grep FAILED $LOG_DIR/05_goodput.txt | wc -l`
if [ $tmp -ne 0 ]; then
    OK=0
fi

if [ $OK -eq 0 ]; then
    echo "$GOODPUT_NAME regression failed, check log file $LOG_DIR/05_goodput.txt"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 118
fi

echo ""
echo "$PGM_NAME complete"
echo "Logs: $LOG_DIR"
echo "`date`"
echo ""
