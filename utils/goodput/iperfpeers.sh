#!/bin/bash

trap "atexit" 0 1 2 15

declare -a JOBS;

function atexit()
{
  let n=${#JOBS[@]}
  for((i=0; i<n; i++)) do
    pid=${JOBS[$i]}
    kill -9 $pid &>/dev/null
  done
}

PEERS=$(/sbin/ip ro sh |awk '/dev tun/{print $1}');

while [ -f IPERFBARR ]; do
  sleep 0.1
done

let c=0;
for p in $PEERS; do
  L=${HOSTNAME}-to-${p}.log
  rm $L &>/dev/null
  iperf -fMB -c $p $1 &> $L &
  JOBS[$c]=$!
  let c=c+1
done

for p in $PEERS; do
  wait
done
