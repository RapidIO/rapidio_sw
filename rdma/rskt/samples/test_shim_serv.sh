#!/bin/sh

killall -9 thttpd
ulimit -c unlimited

#export LD_DEBUG=symbols
export LD_LIBRARY_PATH=.

LD_PRELOAD=./rskt_shim.so exec ./thttpd -D -h 0.0.0.6 -p 80 -l /dev/tty -d $(pwd)
