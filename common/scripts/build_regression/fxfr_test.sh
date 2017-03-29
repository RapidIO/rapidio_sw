#!/bin/bash

# File transfer test
# Uses error codes 80-109

PGM_NAME=fxfr_test.sh
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
    exit 80
fi

echo ""
echo "Start $PGM_NAME"
echo "`date`"
echo ""

# Initialize common properties
if [ ! -e ./install_common.sh ]; then
    echo "File install_common.sh not found"
    exit 81
fi
. ./install_common.sh $NDATA
if [ $? -ne 0 ]; then
	echo "[${BASH_SOURCE[0]}:${LINENO}]"
	exit 82
fi

# Adjust logging directory
LOG_DIR=$LOG_DIR/fxfr
if [ ! -d $LOG_DIR ]; then
    mkdir -p $LOG_DIR
fi

#
CLIENT=${ALLNODES[1]}

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
    exit 83
fi

# Verify the executables exist
OK=1
touch $LOG_DIR/02_check-files.txt
echo "Checking for $FXFR_PATH on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
ssh "$MY_USERID"@"$MASTER" [ ! -e $FXFR_PATH ]
if [ $? -eq 0 ]; then
    echo "  $FXFR_PATH does not exist on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
    OK=0
fi

echo "Checking for $FXFR_ROOT/create_file.sh on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
ssh "$MY_USERID"@"$MASTER" [ ! -e $FXFR_ROOT/create_file.sh ]
if [ $? -eq 0 ]; then
    echo " $FXFR_ROOT/create_file.sh does not exist on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
    OK=0
fi

echo "Checking for $FXFR_ROOT/test.sh on $CLIENT" |& tee -a $LOG_DIR/02_check-files.txt
ssh "$MY_USERID"@"$CLIENT" [ ! -e $FXFR_ROOT/test.sh ]
if [ $? -eq 0 ]; then
    echo "File $FXFR_ROOT/test.sh does not exist" |& tee -a $LOG_DIR/02_check-files.txt
    OK=0
fi

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 84
fi

echo ""
for ((i=1;i<=$MAX_ITERATIONS;i++))
do
    echo "Starting $FXFR_NAME, iteration $i"
    ssh "$MY_USERID"@"$MASTER" screen -dmS $FXFR_PROC $FXFR_PATH
    sleep $LONG_SLEEP_INTERVAL

    echo "Checking $FXFR_NAME"
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $FXFR_PROC)
    if [ $? -ne 0 ]; then
        echo "$FXFR_NAME not started on $node
        echo "[${BASH_SOURCE[0]}:${LINENO}]""
        exit 85
    fi

    echo "Stopping $FXFR_NAME"
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $FXFR_PROC -X stuff "quit\r"
        sleep $LONG_SLEEP_INTERVAL
        exit
EOF

    echo "Checking $FXFR_NAME"
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $FXFR_PROC)
    if [ $? -eq 0 ]; then
        echo "$FXFR_NAME killed but still alive with PID=$tmp"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 86
    fi
    echo ""
done

echo "Getting DestID for $MASTER"
FNAME=/tmp/$$.txt
ssh "$MY_USERID"@"$MASTER" screen -dmS $FXFR_PROC $FXFR_PATH
ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
    screen -r $FXFR_PROC -X stuff "status\r"
    screen -r $FXFR_PROC -X hardcopy $FNAME
    screen -r $FXFR_PROC -X stuff "quit\r"
    sleep $LONG_SLEEP_INTERVAL
    exit
EOF
scp "$MY_USERID"@"$MASTER":$FNAME $LOG_DIR/03_fxfr-status.txt  &> /dev/null
rm -rf $FNAME &> /dev/null

DESTID=`grep destID $LOG_DIR/03_fxfr-status.txt | awk '{ print $4 }'`
if [ $DESTID -eq 0 ]; then
        echo "Could not get destid, check log file $LOG_DIR/03_fxfr-status.txt"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 87
fi

OK=1
mkdir -p $LOG_DIR/out &> /dev/null
if [ $? -ne 0 ]; then
    echo "Could not create output directory $LOG_DIR/out"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 88
fi

FILE_500K=$$_tst_500K.txt
FILE_1500K=$$_tst_1500K.txt
FILE_12M=$$_tst_12M.txt

OK=1
echo "Create test files"
$FXFR_ROOT/create_file.sh /tmp/$FILE_500K 500K |& tee $LOG_DIR/04_create-files.txt
if [ ! -e /tmp/$FILE_500K ]; then
    OK=0
fi

$FXFR_ROOT/create_file.sh /tmp/$FILE_1500K 1500K
if [ ! -e /tmp/$FILE_1500K ]; then
    OK=0
fi

$FXFR_ROOT/create_file.sh /tmp/$FILE_12M 12M
if [ ! -e /tmp/$FILE_12M ]; then
    OK=0
fi

scp /tmp/$$_tst_*.txt "$MY_USERID"@"$CLIENT":/tmp/. &> /dev/null
if [ $OK -eq 0 ]; then
    echo "Could not create test files"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    rm -rf /tmp/$FILE_500K &> /dev/null
    rm -rf /tmp/$FILE_1500K &> /dev/null
    rm -rf /tmp/$FILE_12M &> /dev/null
    rm -rf $LOG_DIR/out &> /dev/null
    exit 89
fi

# BEGIN: Simple test
# Start on server, transfer files from one client
#
sleep $SLEEP_INTERVAL
echo ""
echo "Starting $FXFR_NAME"
ssh "$MY_USERID"@"$MASTER" screen -dmS $FXFR_PROC $FXFR_PATH
ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
    screen -r $FXFR_PROC -X stuff "status\r"
    sleep $LONG_SLEEP_INTERVAL
    exit
EOF

OK=1
NUM_COPIES=1000
echo "Transfer files 500k files"
ssh "$MY_USERID"@"$CLIENT" "cd $FXFR_ROOT; ./test.sh /tmp/$FILE_500K $LOG_DIR/out/$FILE_500K.out $DESTID $NUM_COPIES" |& tee $LOG_DIR/05_fxfr_500k.txt
tmp=`grep FAIL $LOG_DIR/05_fxfr_500k.txt | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Transfer of $FILE_500K failed, check log file $LOG_DIR/05_fxfr_500k.txt"
    OK=0
else
    echo "" |& tee -a $LOG_DIR/05_fxfr_500k.txt
    echo "Verify all files transfered" |& tee -a $LOG_DIR/05_fxfr_500k.txt
    tmp=`find $LOG_DIR/out -type f -name "$FILE_500K.out*" | wc -l`
    if [ $tmp -ne $NUM_COPIES ]; then
        echo "Expected $NUM_COPIES copies of $FILE_500K, received $tmp" |& tee -a $LOG_DIR/05_fxfr_500k.txt
        OK=0
    fi

    echo "Verify file content" |& tee -a $LOG_DIR/05_fxfr_500k.txt
    for file in $LOG_DIR/out/$FILE_500K.out*;
    do
        diff $file /tmp/$FILE_500K |& tee -a $LOG_DIR/05_fxfr_500k.txt
        if [ $? -ne 0 ]; then
            OK=0
        fi
    done
fi
rm -rf $LOG_DIR/out/$FILE_500K.out* |& tee -a $LOG_DIR/05_fxfr_500k.txt

sleep $SLEEP_INTERVAL
NUM_COPIES=500
echo ""
echo "Transfer files 1500k files"
ssh "$MY_USERID"@"$CLIENT" "cd $FXFR_ROOT; ./test.sh /tmp/$FILE_1500K $LOG_DIR/out/$FILE_1500K.out $DESTID $NUM_COPIES" |& tee $LOG_DIR/06_fxfr_1500k.txt
tmp=`grep FAIL $LOG_DIR/06_fxfr_1500k.txt | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Transfer of $FILE_1500K failed, check log file $LOG_DIR/06_fxfr_1500k.txt"
    OK=0
else
    echo "" |& tee -a $LOG_DIR/06_fxfr_1500k.txt
    echo "Verify all files transfered" |& tee -a $LOG_DIR/06_fxfr_1500k.txt
    tmp=`find $LOG_DIR/out -type f -name "$FILE_1500K.out*" | wc -l`
    if [ $tmp -ne $NUM_COPIES ]; then
        echo "Expected $NUM_COPIES copies of $FILE_1500K, received $tmp" |& tee -a $LOG_DIR/06_fxfr_1500k.txt
        OK=0
    fi

    echo "Verify file content" |& tee -a $LOG_DIR/06_fxfr_1500k.txt
    for file in $LOG_DIR/out/$FILE_1500K.out*;
    do
        diff $file /tmp/$FILE_1500K |& tee -a $LOG_DIR/06_fxfr_1500k.txt
        if [ $? -ne 0 ]; then
            OK=0
        fi
    done
fi
rm -rf $LOG_DIR/out/$FILE_1500K.out* |& tee -a $LOG_DIR/06_fxfr_1500k.txt

sleep $SLEEP_INTERVAL
NUM_COPIES=100
echo ""
echo "Transfer files 12M files"
ssh "$MY_USERID"@"$CLIENT" "cd $FXFR_ROOT; ./test.sh /tmp/$FILE_12M $LOG_DIR/out/$FILE_12M.out $DESTID $NUM_COPIES" |& tee $LOG_DIR/07_fxfr_12M.txt
tmp=`grep FAIL $LOG_DIR/07_fxfr_12M.txt | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Transfer of $FILE_12M failed, check log file $LOG_DIR/07_fxfr_12M.txt"
    OK=0
else
    echo "" |& tee -a $LOG_DIR/07_fxfr_12M.txt
    echo "Verify all files transfered" |& tee -a $LOG_DIR/07_fxfr_12M.txt
    tmp=`find $LOG_DIR/out -type f -name "$FILE_12M.out*" | wc -l`
    if [ $tmp -ne $NUM_COPIES ]; then
        echo "Expected $NUM_COPIES copies of $FILE_12M, received $tmp" |& tee -a $LOG_DIR/07_fxfr_12M.txt
        OK=0
    fi

    echo "Verify file content" |& tee -a $LOG_DIR/07_fxfr_12M.txt
    for file in $LOG_DIR/out/$FILE_12M.out*;
    do
       diff $file /tmp/$FILE_12M |& tee -a $LOG_DIR/07_fxfr_12M.txt
       if [ $? -ne 0 ]; then
           OK=0
       fi
    done
fi
rm -rf $LOG_DIR/out/$FILE_12M.out* |& tee -a $LOG_DIR/07_fxfr_12M.txt
sleep $SLEEP_INTERVAL
#
# END: Simple test

echo ""
echo "Cleanup resources"
ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
    screen -r $FXFR_PROC -X stuff "quit\r"
    sleep $LONG_SLEEP_INTERVAL
    exit
EOF

ssh "$MY_USERID"@"$CLIENT" rm -rf /tmp/$FILE_500K &> /dev/null
ssh "$MY_USERID"@"$CLIENT" rm -rf /tmp/$FILE_1500K &> /dev/null
ssh "$MY_USERID"@"$CLIENT" rm -rf /tmp/$FILE_12M &> /dev/null

rm -rf /tmp/$FILE_500K &> /dev/null
rm -rf /tmp/$FILE_1500K &> /dev/null
rm -rf /tmp/$FILE_12M &> /dev/null
rm -rf $LOG_DIR/out &> /dev/null

if [ $OK -eq 0 ] ; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 89
fi

echo ""
echo "$PGM_NAME complete"
echo "Logs: $LOG_DIR"
echo "`date`"
echo ""
