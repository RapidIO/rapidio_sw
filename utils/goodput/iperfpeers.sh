#!/bin/bash

trap "atexit" 0 1 2 15

declare -a JOBS;
let exitflag=0

function atexit()
{
  let n=${#JOBS[@]}
  for((i=0; i<n; i++)) do
    pid=${JOBS[$i]}
    kill -9 $pid &>/dev/null
  done
  let exitflag=1
}

if [ -f IPERFBARR ]; then
  echo Please remove IPERFBARR barrier file. The folder $PWD should be exported via sshfs too all nodes of cluster.
  while [ -f IPERFBARR ]; do
    sleep 0.1
    [ $exitflag -ne 0 ] && exit 0;
  done
fi

PEERS=$(/sbin/ip ro sh |awk '/dev tun/{print $1}');

echo Barrier met. Blasting $PEERS

let c=0;
for p in $PEERS; do
  L=${HOSTNAME}-to-${p}.log
  rm $L &>/dev/null
  LP="-t 3600"
  iperf -fMB $LP -c $p $@ &> $L &
  JOBS[$c]=$!
  let c=c+1
done

for p in $PEERS; do
  wait
done
