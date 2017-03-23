#!/bin/bash

# FMD process test
# Uses error codes 50-79

PGM_NAME=fmd_test.sh
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
    exit 50
fi

echo ""
echo "Start $PGM_NAME"
echo "`date`"
echo ""

# Initialize common properties
if [ ! -e ./install_common.sh ]; then
    echo "File install_common.sh not found"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 51
fi
. ./install_common.sh $NDATA
if [ $? -ne 0 ]; then
	echo "[${BASH_SOURCE[0]}:${LINENO}]"
	exit 52
fi

# Adjust logging directory
LOG_DIR=$LOG_DIR/fmd
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
    exit 53
fi

# Verify the executables exist
OK=1
touch $LOG_DIR/02_check-files.txt
echo "Checking for $FMD_PATH on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
ssh "$MY_USERID"@"$MASTER" [ ! -e $FMD_PATH ]
if [ $? -eq 0 ]; then
    echo "  $FMD_PATH does not exist on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
    OK=0
fi

echo "Checking for $RRMAP_PATH on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
ssh "$MY_USERID"@"$MASTER" [ ! -e $RRMAP_PATH ]
if [ $? -eq 0 ]; then
    echo "  $RRMAP_PATH does not exist on $MASTER" |& tee -a $LOG_DIR/02_check-files.txt
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
    exit 54
fi

# Start/Stop the processes
OK=1
touch $LOG_DIR/03_start-stop.txt
echo ""
for ((i=1;i<=$MAX_ITERATIONS;i++))
do
    echo "Iteration $i" |& tee -a $LOG_DIR/03_start-stop.txt

    echo "Starting $FMD_NAME on $MASTER" |& tee -a $LOG_DIR/03_start-stop.txt
    ssh "$MY_USERID"@"$MASTER" screen -dmS $FMD_PROC $FMD_PATH 
    sleep $LONG_SLEEP_INTERVAL

    echo "Starting $RRMAP_NAME on $MASTER" |& tee -a $LOG_DIR/03_start-stop.txt
    ssh "$MY_USERID"@"$MASTER" screen -dmS $RRMAP_PROC $RRMAP_PATH 
    sleep $SLEEP_INTERVAL

    echo "Checking $RRMAP_NAME on $MASTER" |& tee -a $LOG_DIR/03_start-stop.txt
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $RRMAP_PROC)
    if [ $? -ne 0 ]; then
        echo "$RRMAP_NAME not started on $MASTER" |& tee -a $LOG_DIR/03_start-stop.txt
        OK=0
    fi

    echo "Checking $FMD_NAME on $MASTER" |& tee -a $LOG_DIR/03_start-stop.txt
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $FMD_PROC)
    if [ $? -ne 0 ]; then
        echo "$FMD_NAME not started on $MASTER" |& tee -a $LOG_DIR/03_start-stop.txt
        OK=0
    fi

    echo "Stopping $RRMAP_NAME on $MASTER" |& tee -a $LOG_DIR/03_start-stop.txt
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $RRMAP_PROC -X stuff "quit\r"
        sleep $LONG_SLEEP_INTERVAL
        exit
EOF

    echo "Stopping $FMD_NAME on $MASTER" |& tee -a $LOG_DIR/03_start-stop.txt
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $FMD_PROC -X stuff "quit\r"
        sleep $LONG_SLEEP_INTERVAL
        exit
EOF

    echo "Verifying $RRMAP_NAME stopped on $MASTER" |& tee -a $LOG_DIR/03_start-stop.txt
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $RRMAP_PROC)
    if [ $? -eq 0 ]; then
        echo "$RRMAP_NAME killed but still alive with PID=$tmp" |& tee -a $LOG_DIR/03_start-stop.txt
        OK=0
    fi

    echo "Verifying $FMD_NAME stopped on $MASTER" |& tee -a $LOG_DIR/03_start-stop.txt
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $FMD_PROC)
    if [ $? -eq 0 ]; then
        echo "$FMD_NAME killed but still alive with PID=$tmp" |& tee -a $LOG_DIR/03_start-stop.txt
        OK=0
    fi

    if [ $OK -eq 0 ]; then
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 55
    fi
    echo ""
done

echo "Starting $FMD_NAME for output verification"
# Verify all_start, check_all, stop_all

OK=1
echo "Verify nothing running"
ssh "$MY_USERID"@"$MASTER" $INSTALL_ROOT/check_all.sh |& tee $LOG_DIR/04_pre-check.txt
tmp=`grep FMD $LOG_DIR/04_pre-check.txt | grep NOT | wc -l`
if [ $tmp -ne  ${#ALLNODES[@]} ]; then
    echo "Processes still running, check log file $LOG_DIR/04_pre-check.txt"
    OK=0
fi

# Verify check_all reported back correctly
for node in ${ALLNODES[@]}
do
    tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $FMD_PROC)
    if [ $? -eq 0 ]; then
        echo "$FMD_NAME still running on $node" |& tee $LOG_DIR/04_pre-check.txt
        OK=0
    fi
done

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 56
fi

echo ""
echo "Starting processes with all_start.sh"
ssh "$MY_USERID"@"$MASTER" $INSTALL_ROOT/all_start.sh |& tee $LOG_DIR/05_start-processes.txt
sleep $SLEEP_INTERVAL

echo ""
echo "Checking processes with check_all.sh"
ssh "$MY_USERID"@"$MASTER" $INSTALL_ROOT/check_all.sh |& tee $LOG_DIR/06_post-check.txt

# Verify everything is started
OK=1
tmp=`grep FMD $LOG_DIR/06_post-check.txt | grep NOT | wc -l`
if [ $tmp -ne 0 ]; then
    echo "Processes not started on all devices, check log file $LOG_DIR/06_post-check.txt"
    OK=0
fi

# Verify check_all reported back correctly
for node in ${ALLNODES[@]}
do
    tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $FMD_PROC)
    if [ $? -ne 0 ]; then
        echo "$FMD_NAME not started on $node" |& tee $LOG_DIR/06_post-check.txt
        OK=0
    fi
done

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 57
fi

echo ""
echo "Checking $FMD_NAME output"
FNAME=/tmp/$$.txt
ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
    screen -r $FMD_PROC -X stuff "?\r"
    screen -r $FMD_PROC -X stuff "devs\r"
    screen -r $FMD_PROC -X hardcopy $FNAME
    sleep $SLEEP_INTERVAL
    exit
EOF
scp "$MY_USERID"@"$MASTER":$FNAME $LOG_DIR/07_fmd-devs.txt  &> /dev/null

ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
    screen -r $FMD_PROC -X stuff "?\r"
    screen -r $FMD_PROC -X stuff "status\r"
    screen -r $FMD_PROC -X hardcopy $FNAME
    sleep $LONG_SLEEP_INTERVAL
    exit
EOF
scp "$MY_USERID"@"$MASTER":$FNAME $LOG_DIR/08_fmd-status.txt &> /dev/null

ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
    screen -r $FMD_PROC -X stuff "?\r"
    screen -r $FMD_PROC -X stuff "dddump\r"
    screen -r $FMD_PROC -X hardcopy $FNAME
    sleep $SLEEP_INTERVAL
    exit
EOF
scp "$MY_USERID"@"$MASTER":$FNAME $LOG_DIR/09_fmd-dddump.txt &> /dev/null


OK=1
tmp=`wc -l $LOG_DIR/07_fmd-devs.txt | awk '{print $1}'`
if [ $tmp -eq 0 ]; then
    echo "Error, no output from $FMD_NAME devs command"
    OK=0
fi
tmp=`wc -l $LOG_DIR/08_fmd-status.txt | awk '{print $1}'`
if [ $tmp -eq 0 ]; then
    echo "Error, no output from $FMD_NAME status command"
    OK=0
fi
tmp=`wc -l $LOG_DIR/09_fmd-dddump.txt | awk '{print $1}'`
if [ $tmp -eq 0 ]; then
    echo "Error, no output from $FMD_NAME dddump command"
    OK=0
fi

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 58
fi

tmp=`grep endpoints $LOG_DIR/07_fmd-devs.txt | awk '{ print $4 }'`
if [ $tmp -ne ${#ALLNODES[@]} ]; then
    echo "$FMD_NAME devs failed. Expected ${#ALLNODES[@]} endpoints, observed $tmp"
    OK=0
fi

tmp=`grep PeerCnt $LOG_DIR/08_fmd-status.txt | awk '{ print$ 7 }'`
if [ $tmp -ne $((${#ALLNODES[@]}-1)) ]; then
    echo "$FMD_NAME status failed. Expected ${#ALLNODES[@]} peers, observed $tmp"
    OK=0
fi

tmp=`grep num_devs $LOG_DIR/09_fmd-dddump.txt | awk '{ print $5 }'`
if [ $tmp -ne ${#ALLNODES[@]} ]; then
    echo "$FMD_NAME dddump failed. Expected ${#ALLNODES[@]} num_devs , observed $tmp"
    OK=0
fi

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 59
fi

echo ""
echo "Shutdown processes via stop_all.sh"
ssh "$MY_USERID"@"$MASTER" $INSTALL_ROOT/stop_all.sh |& tee $LOG_DIR/10_fmd-shutdown.txt
sleep $LONG_SLEEP_INTERVAL

echo "Checking $FMD_NAME is stopped"
tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $FMD_PROC)
if [ $? -eq 0 ]; then
    echo "$FMD_NAME killed but still alive with PID=$tmp" |& tee $LOG_DIR/10_fmd-shutdown.txt
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 60
fi

echo "Starting $FMD_NAME on $MASTER for connectivity test"
ssh "$MY_USERID"@"$MASTER" screen -dmS $FMD_PROC $FMD_PATH
sleep $LONG_SLEEP_INTERVAL

echo "Checking $FMD_NAME"
tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $FMD_PROC)
if [ $? -ne 0 ]; then
   echo "$FMD_NAME not started on $MASTER"
   echo "[${BASH_SOURCE[0]}:${LINENO}]"
   exit 61
fi

echo ""
echo "$FMD_NAME running, start/stop $RRMAP_NAME connectivity test"
for ((i=1;i<=$MAX_ITERATIONS;i++))
do
    echo "Starting $RRMAP_NAME, iteration $i"
    ssh "$MY_USERID"@"$MASTER" screen -dmS $RRMAP_PROC $RRMAP_PATH
    sleep $LONG_SLEEP_INTERVAL

    echo "Checking $RRMAP_NAME"
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $RRMAP_PROC)
    if [ $? -ne 0 ]; then
        echo "$RRMAP_NAME not started on $MASTER"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 62
    fi

    echo "Checking $FMD_NAME for connectivity"
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $FMD_PROC -X stuff "?\r"
        screen -r $FMD_PROC -X stuff "status\r"
        screen -r $FMD_PROC -X hardcopy $FNAME
        sleep $LONG_SLEEP_INTERVAL
        exit
EOF

    OK=1
    scp "$MY_USERID"@"$MASTER":$FNAME $LOG_DIR/tmp.txt &> /dev/null
    tmp=`grep NumApps $LOG_DIR/tmp.txt | awk '{ print $7 }'`
    if [ $tmp -ne 1 ]; then
        echo "$FMD_NAME does not report $RRMAP_NAME as connection"
        OK=0
    fi

    echo "Checking $RRMAP_NAME for connectivity"
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $RRMAP_PROC -X stuff "?\r"
        screen -r $RRMAP_PROC -X stuff "ddstatus\r"
        screen -r $RRMAP_PROC -X hardcopy $FNAME
        sleep $LONG_SLEEP_INTERVAL
        exit
EOF

    scp "$MY_USERID"@"$MASTER":$FNAME $LOG_DIR/tmp.txt &> /dev/null
    tmp=`grep "Device Dir PTR" $LOG_DIR/tmp.txt | grep "(nil)" |wc -l`
    if [ $tmp -ne 0 ]; then
        echo "$RRMAP_NAME Device Dir PTR is nil"
        OK=0
    fi

    tmp=`grep "Device Dir Mtx PTR" $LOG_DIR/tmp.txt | grep "(nil)" | wc -l`
    if [ $tmp -ne 0 ]; then
        echo "$RRMAP_NAME Device Dir Mtx PTR is nil"
        OK=0
    fi

    if [ $OK -eq 0 ]; then
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 63
    fi

    echo "Stopping $RRMAP_NAME"
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $RRMAP_PROC -X stuff "quit\r"
        sleep $LONG_SLEEP_INTERVAL
        exit
EOF

    echo "Checking $RRMAP_NAME"
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $RRMAP_PROC)
    if [ ! -z $tmp ]; then
        echo "$RRMAP_NAME killed but still alive with PID=$tmp"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 64
    fi

    echo "Checking $FMD_NAME for connectivity"
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $FMD_PROC -X stuff "?\r"
        screen -r $FMD_PROC -X stuff "status\r"
        screen -r $FMD_PROC -X hardcopy $FNAME
        sleep $LONGSLEEP_INTERVAL
        exit
EOF
    scp "$MY_USERID"@"$MASTER":$FNAME $LOG_DIR/tmp.txt &> /dev/null

    tmp=`grep NumApps $LOG_DIR/tmp.txt | awk '{ print $7 }'`
    if [ $tmp -ne 0 ]; then
        echo "$FMD_NAME still sees $RRMAP_NAME as connected"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 65
    fi

    echo ""
done

echo "Stopping $FMD_NAME"
ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
    screen -r $FMD_PROC -X stuff "quit\r"
    sleep $LONG_SLEEP_INTERVAL
    exit
EOF

echo "Checking $FMD_NAME"
tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $FMD_PROC)
if [ ! -z $tmp ]; then
    echo "$FMD_NAME killed but still alive with PID=$tmp"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 66
fi

echo "Starting $RRMAP_NAME on $MASTER for connectivity test"
ssh "$MY_USERID"@"$MASTER" screen -dmS $RRMAP_PROC $RRMAP_PATH
sleep $LONG_SLEEP_INTERVAL

echo "Checking $RRMAP_NAME"
tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $RRMAP_PROC)
if [ $? -ne 0 ]; then
   echo "$RRMAP_NAME not started on $MASTER"
   echo "[${BASH_SOURCE[0]}:${LINENO}]"
   exit 67
fi

echo ""
echo "$RRMAP_NAME running, start/stop $FMD_NAME connectivity test"
for ((i=1;i<=$MAX_ITERATIONS;i++))
do
    echo "Starting $FMD_NAME, iteration $i"
    ssh "$MY_USERID"@"$MASTER" screen -dmS $FMD_PROC $FMD_PATH
    sleep $LONG_SLEEP_INTERVAL

    echo "Checking $FMD_NAME"
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $FMD_PROC)
    if [ $? -ne 0 ]; then
        echo "$FMD_NAME not started on $MASTER"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 68
    fi

    echo "Checking $FMD_NAME for connectivity"
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $FMD_PROC -X stuff "?\r"
        screen -r $FMD_PROC -X stuff "status\r"
        screen -r $FMD_PROC -X hardcopy $FNAME
        sleep $LONG_SLEEP_INTERVAL
        exit
EOF

    OK=1
    scp "$MY_USERID"@"$MASTER":$FNAME $LOG_DIR/tmp.txt &> /dev/null
    tmp=`grep NumApps $LOG_DIR/tmp.txt | awk '{ print $7 }'`
    if [ $tmp -ne 1 ]; then
        echo "$FMD_NAME does not report $RRMAP_NAME as connection"
        OK=0
    fi

    echo "Checking $RRMAP_NAME for connectivity"
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $RRMAP_PROC -X stuff "?\r"
        screen -r $RRMAP_PROC -X stuff "ddstatus\r"
        screen -r $RRMAP_PROC -X hardcopy $FNAME
        sleep $LONG_SLEEP_INTERVAL
        exit
EOF

    scp "$MY_USERID"@"$MASTER":$FNAME $LOG_DIR/tmp.txt &> /dev/null
    tmp=`grep "Device Dir PTR" $LOG_DIR/tmp.txt | grep "(nil)" | wc -l`
    if [ $tmp -ne 0 ]; then
        echo "$RRMAP_NAME Device Dir PTR is nil"
        OK=0
    fi

    tmp=`grep "Device Dir Mtx PTR" $LOG_DIR/tmp.txt | grep "(nil)" | wc -l`
    if [ $tmp -ne 0 ]; then
        echo "$RRMAP_NAME Device Dir Mtx PTR is nil"
        OK=0
    fi

    if [ $OK -eq 0 ]; then
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 69
    fi

    echo "Stopping $FMD_NAME"
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $FMD_PROC -X stuff "quit\r"
        sleep $LONG_SLEEP_INTERVAL
        exit
EOF

    echo "Checking $FMD_NAME"
    tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $FMD_PROC)
    if [ ! -z $tmp ]; then
        echo "$FMD_NAME killed but still alive with PID=$tmp"
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 70
    fi

    echo "Checking $RRMAP_NAME for connectivity"
    ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
        screen -r $RRMAP_PROC -X stuff "?\r"
        screen -r $RRMAP_PROC -X stuff "ddstatus\r"
        screen -r $RRMAP_PROC -X hardcopy $FNAME
        sleep $LONG_SLEEP_INTERVAL
        exit
EOF

    scp "$MY_USERID"@"$MASTER":$FNAME $LOG_DIR/tmp.txt &> /dev/null
    tmp=`grep "Device Dir PTR" $LOG_DIR/tmp.txt | grep "(nil)" | wc -l`
    if [ $tmp -ne 1 ]; then
        echo "$RRMAP_NAME Device Dir PTR is not nil"
        OK=0
    fi

    tmp=`grep "Device Dir Mtx PTR" $LOG_DIR/tmp.txt | grep "(nil)" | wc -l`
    if [ $tmp -ne 1 ]; then
        echo "$RRMAP_NAME Device Dir Mtx PTR is not nil"
        OK=0
    fi

    if [ $OK -eq 0 ]; then
        echo "[${BASH_SOURCE[0]}:${LINENO}]"
        exit 71
    fi

    echo ""
done

ssh "$MY_USERID"@"$MASTER" rm -rf $FNAME
rm -rf $LOG_DIR/tmp.txt
ssh "$MY_USERID"@"$MASTER" rm -rf /tmp/$$.txt


echo "Stopping $RRMAP_NAME"
ssh -tt "$MY_USERID"@"$MASTER" <<- EOF &> /dev/null
    screen -r $RRMAP_PROC -X stuff "quit\r"
    sleep $LONG_SLEEP_INTERVAL
    exit
EOF

echo "Checking $RRMAP_NAME"
tmp=$(ssh "$MY_USERID"@"$MASTER" pgrep -x $RRMAP_PROC)
if [ ! -z $tmp ]; then
    echo "$RRMAP_NAME killed but still alive with PID=$tmp"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 72
fi

echo ""
echo "$PGM_NAME complete"
echo "Logs: $LOG_DIR"
echo "`date`"
echo ""
