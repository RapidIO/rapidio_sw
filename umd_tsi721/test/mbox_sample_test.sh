#!/bin/bash

SOURCE_PATH=/opt/rapidio/rapidio_sw
RIO_CLASS_MPORT_DIR=/sys/class/rio_mport/rio_mport0

MBOX=3
MAXIT=2

RND=$RANDOM

. /etc/rapidio/nodelist.sh

MY_DESTID=$(cat $RIO_CLASS_MPORT_DIR/device/port_destid 2>/dev/null);

DID_LIST='';
NODE_LIST='';
for node in $NODES; do
        RIODEVS=$(ssh root@"$node" "ls /sys/bus/rapidio/devices/")
        [ -z "$RIODEVS" ] && continue;
  DESTID=$(ssh root@"$node" "cat $RIO_CLASS_MPORT_DIR/device/port_destid 2>/dev/null")
  [ $MY_DESTID = $DESTID ] && continue;

  DID_LIST="$DID_LIST $DESTID";
  NODE_LIST="$NODE_LIST $node";
done
echo "Valid node list: $NODE_LIST";
echo "Valid did  list: $DID_LIST";

screen -dmS mbx_srv 'bash'; sleep 0.2
screen -r mbx_srv -X stuff "cd $SOURCE_PATH/umd_tsi721; sudo ./mbox_server -c $MBOX | tee -a /tmp/mbxsrv-${RND}.log^M"

for node in $NODE_LIST; do
  DID=$(printf "%d" $MY_DESTID);
  ssh root@"$node" "screen -dmS mbx_cli 'bash'; sleep 0.2; screen -r mbx_cli -X stuff \"cd $SOURCE_PATH/umd_tsi721; ./mbox_client -d $DID -c $MBOX -C $MBOX | tee -a /tmp/mbxcli-${RND}.log^M\""
done

for ((i=0; i<$MAXIT; i++)); do
  echo "====Iteration $i====";

  echo "Server node:";
  sudo killall -USR1 mbox_server; sleep 0.5; cat /tmp/mbxsrv-${RND}.log; echo '' > /tmp/mbxsrv-${RND}.log;
  echo;

  for node in $NODE_LIST; do
    echo "Client node $node:";
    ssh root@"$node" "killall -USR1 mbox_client; sleep 0.5; cat /tmp/mbxcli-${RND}.log; echo '' > /tmp/mbxcli-${RND}.log";
    echo;
  done
  sleep 10;
done

echo -n 'Press ENTER to tear down: '; read FOO

for node in $NODE_LIST; do
  ssh root@"$node" "screen -r mbx_cli -X stuff '^C^C'; sleep 0.2; screen -r mbx_cli -X stuff 'exit^M'; rm -f /tmp/mbxcli-${RND}.log"
done

screen -r mbx_srv -X stuff "^C^C"
screen -r mbx_srv -X stuff "exit^M"
rm -f /tmp/mbxsrv-${RND}.log
