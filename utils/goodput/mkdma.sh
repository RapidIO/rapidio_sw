#!/bin/sh

destid=0
cpu=3

# udma : <idx> <cpu> <chan> <buff> <sts> <did> <rio_addr> <bytes> <acc_sz> <trans>

function udma()
{
size=$1
cat <<EOF
levelog 0
kill all
t 0 0 0
wait 0 2
udma 0 $cpu 7 200 800 $destid 40dc00000 400000 $size 2
wait 0 1
g
sleep 0.5
g c
sleep 4
g
kill 0
wait 0 0
quit
EOF
}

for size in 400000 200000 100000 80000 40000 20000 10000 8000 4000 2000 1000 800 400 200 100 80 40 20 10 8 4 2 1
 do
   udma $size > scripts/u.$size
   cat scripts/u.$size
 done
