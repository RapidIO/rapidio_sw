#!/bin/sh

killall -9 thttpd
ulimit -c unlimited

#export LD_DEBUG=symbols
export LD_LIBRARY_PATH=.

#LD_PRELOAD=./rskt_shim.so exec wget -O /dev/null http://8.8.8.8
#LD_PRELOAD=./rskt_shim.so exec wget -O /dev/null http://ipchicken.com
#LD_PRELOAD=./rskt_shim.so exec lynx -dump http://ipchicken.com
#LD_PRELOAD=./rskt_shim.so exec lynx -dump http://0.0.1.5
#LD_PRELOAD=./rskt_shim.so exec gdb --args thttpd -D -h 0.0.1.5 -p 8001
#LD_PRELOAD=./rskt_shim.so exec strace -e write=3 thttpd -D -h 0.0.0.6 -p 80 -l /dev/tty
LD_PRELOAD=./rskt_shim.so exec thttpd -D -h 0.0.0.6 -p 80 -l /dev/tty
