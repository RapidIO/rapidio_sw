#!/bin/bash

# System process test
# Uses error codes 40-49

PGM_NAME=system_test.sh
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
    exit 40
fi

echo ""
echo "Start $PGM_NAME"
echo "`date`"
echo ""

# Initialize common properties
if [ ! -e ./install_common.sh ]; then
    echo "File install_common.sh not found"
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 41
fi
. ./install_common.sh $NDATA
if [ $? -ne 0 ]; then
	echo "[${BASH_SOURCE[0]}:${LINENO}]"
	exit 42
fi

# Adjust logging directory
LOG_DIR=$LOG_DIR/system
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
    exit 43
fi

# Verify the executables exist
OK=1
touch $LOG_DIR/02_check-files.txt
for node in ${ALLNODES[@]}
do
    echo "Checking for $FMD_PATH on $node" |& tee -a $LOG_DIR/02_check-files.txt
    ssh "$MY_USERID"@"$node" [ ! -e $FMD_PATH ]
    if [ $? -eq 0 ]; then
        echo "  $FMD_PATH does not exist on $node" |& tee -a $LOG_DIR/02_check-files.txt
        OK=0
    fi

    echo "Checking for $RRMAP_PATH on $node" |& tee -a $LOG_DIR/02_check-files.txt
    ssh "$MY_USERID"@"$node" [ ! -e $RRMAP_PATH ]
    if [ $? -eq 0 ]; then
        echo "  $RRMAP_PATH does not exist on $node" |& tee -a $LOG_DIR/02_check-files.txt
        OK=0
    fi

    echo "Checking for $GOODPUT_PATH on $node" |& tee -a $LOG_DIR/02_check-files.txt
    ssh "$MY_USERID"@"$node" [ ! -e $GOODPUT_PATH ]
    if [ $? -eq 0 ]; then
        echo "  $GOODPUT_PATH does not exist on $node" |& tee -a $LOG_DIR/02_check-files.txt
        OK=0
    fi
    echo ""
done

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 44
fi

# Start/Stop the processes
OK=1
touch $LOG_DIR/03_start-stop.txt
echo "Start/Stop the processes"
for node in ${ALLNODES[@]}
do
    for ((i=1;i<=$MAX_ITERATIONS;i++))
    do
        echo "Iteration $i" |& tee -a $LOG_DIR/03_start-stop.txt

        echo "Starting $FMD_NAME on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        ssh "$MY_USERID"@"$node" screen -dmS $FMD_PROC $FMD_PATH
        sleep $LONG_SLEEP_INTERVAL

        echo "Starting $RRMAP_NAME on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        ssh "$MY_USERID"@"$node" screen -dmS $RRMAP_PROC $RRMAP_PATH
        sleep $LONG_SLEEP_INTERVAL

        echo "Starting $GOODPUT_NAME on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        ssh "$MY_USERID"@"$node" screen -dmS $GOODPUT_PROC $GOODPUT_PATH
        sleep $SLEEP_INTERVAL

        echo "Checking $FMD_NAME on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $FMD_PROC)
        if [ $? -ne 0 ]; then
            echo "$FMD_NAME not started on $node" |& tee -a $LOG_DIR/03_start-stop.txt
            OK=0
        fi

        echo "Checking $RRMAP_NAME on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $RRMAP_PROC)
        if [ $? -ne 0 ]; then
            echo "$RRMAP_NAME not started on $node" |& tee -a $LOG_DIR/03_start-stop.txt
            OK=0
        fi

        echo "Checking $GOODPUT_NAME on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $GOODPUT_PROC)
        if [ $? -ne 0 ]; then
            echo "$GOODPUT_NAME not started on $node" |& tee -a $LOG_DIR/03_start-stop.txt
            OK=0
        fi

        echo "Stopping $GOODPUT_NAME on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        ssh -tt "$MY_USERID"@"$node" <<- EOF &> /dev/null
            screen -r $GOODPUT_PROC -X stuff "quit\r"
            sleep $SLEEP_INTERVAL
            exit
EOF

        echo "Stopping $RRMAP_NAME on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        ssh -tt "$MY_USERID"@"$node" <<- EOF &> /dev/null
            screen -r $RRMAP_PROC -X stuff "quit\r"
            sleep $LONG_SLEEP_INTERVAL
            exit
EOF

        echo "Stopping $FMD_NAME on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        ssh -tt "$MY_USERID"@"$node" <<- EOF &> /dev/null
            screen -r $FMD_PROC -X stuff "quit\r"
            sleep $LONG_SLEEP_INTERVAL
            exit
EOF

        echo "Verifying $GOODPUT_NAME stopped on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $GOODPUT_PROC)
        if [ $? -eq 0 ]; then
            echo "$GOODPUT_NAME killed but still alive with PID=$tmp" |& tee -a $LOG_DIR/03_start-stop.txt
            OK=0
        fi

        echo "Verifying $RRMAP_NAME stopped on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $RRMAP_PROC)
        if [ $? -eq 0 ]; then
            echo "$RRMAP_NAME killed but still alive with PID=$tmp" |& tee -a $LOG_DIR/03_start-stop.txt
            OK=0
        fi

        echo "Verifying $FMD_NAME stopped on $node" |& tee -a $LOG_DIR/03_start-stop.txt
        tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $FMD_PROC)
        if [ $? -eq 0 ]; then
            echo "$FMD_NAME killed but still alive with PID=$tmp" |& tee -a $LOG_DIR/03_start-stop.txt
            OK=0
        fi

        if [ $OK -eq 0 ]; then
            echo "[${BASH_SOURCE[0]}:${LINENO}]"
            exit 45
        fi
        echo ""
    done
done

OK=1
touch $LOG_DIR/04_check-master.txt
# verify that all_start exists
ssh "$MY_USERID"@"$MASTER" [ ! -e $INSTALL_ROOT/all_start.sh ]
if [ $? -eq 0 ]; then
    echo "File $INSTALL_ROOT/all_start.sh does not exist" |& tee -a $LOG_DIR/04_check-master.txt
    OK=0
fi

# Verify stop_all exists
ssh "$MY_USERID"@"$MASTER" [ ! -e $INSTALL_ROOT/stop_all.sh ]
if [ $? -eq 0 ]; then
    echo "File $INSTALL_ROOT/stop_all.sh does not exist"  |& tee -a $LOG_DIR/04_check-master.txt
    OK=0
fi

# Verify check script exists
ssh "$MY_USERID"@"$MASTER" [ ! -e $INSTALL_ROOT/check_all.sh ]
if [ $? -eq 0 ]; then
    echo "File $INSTALL_ROOT/check_all.sh does not exist"  |& tee -a $LOG_DIR/04_check-master.txt
    OK=0
fi

echo ""
if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 46
fi

# Verify all_start, check_all, stop_all

OK=1
echo "Verify nothing running"
ssh "$MY_USERID"@"$MASTER" $INSTALL_ROOT/check_all.sh |& tee $LOG_DIR/05_pre-check.txt
tmp=`grep FMD $LOG_DIR/05_pre-check.txt | grep NOT | wc -l`
if [ $tmp -ne  ${#ALLNODES[@]} ]; then
    echo "Processes still running, check log file  $LOG_DIR/05_pre-check.txt"
    OK=0
fi

# Verify check_all reported back correctly
for node in ${ALLNODES[@]}
do
    tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $FMD_PROC)
    if [ $? -eq 0 ]; then
        echo "$FMD_NAME still running on $node"
        OK=0
    fi
done

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 47
fi

echo "Starting processes"
ssh "$MY_USERID"@"$MASTER" $INSTALL_ROOT/all_start.sh |& tee $LOG_DIR/06_start-processes.txt
sleep $SLEEP_INTERVAL

echo "Checking processes"
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
    exit 48
fi

# Stop them
echo "Stopping processes"
ssh "$MY_USERID"@"$MASTER" $INSTALL_ROOT/stop_all.sh |& tee $LOG_DIR/07_stop-processes.txt
sleep $SLEEP_INTERVAL

echo "Checking processes"
ssh "$MY_USERID"@"$MASTER" $INSTALL_ROOT/check_all.sh |& tee $LOG_DIR/08_post-check.txt

# Verify everything is started
OK=1
tmp=`grep FMD $LOG_DIR/08_post-check.txt | grep NOT | wc -l`
if [ $tmp -ne ${#ALLNODES[@]} ]; then
    echo "Processes not stopped on all devices, check log file $LOG_DIR/08_post-check.txt"
    OK=0
fi

# Verify check_all reported back correctly
for node in ${ALLNODES[@]}
do
    tmp=$(ssh "$MY_USERID"@"$node" pgrep -x $FMD_PROC)
    if [ $? -eq 0 ]; then
        echo "$FMD_NAME running on $node" |& tee $LOG_DIR/08_post-check.txt
        OK=0
    fi
done

if [ $OK -eq 0 ]; then
    echo "[${BASH_SOURCE[0]}:${LINENO}]"
    exit 49
fi

echo ""
echo "$PGM_NAME complete"
echo "Logs: $LOG_DIR"
echo "`date`"
echo ""
