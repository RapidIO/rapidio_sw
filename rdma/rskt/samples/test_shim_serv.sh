#!/bin/sh

ulimit -c unlimited
#LD_PRELOAD=./rskt_shim.so exec wget -O /dev/null http://8.8.8.8
#LD_PRELOAD=./rskt_shim.so exec wget -O /dev/null http://ipchicken.com
#LD_PRELOAD=./rskt_shim.so exec lynx -dump http://ipchicken.com
#LD_PRELOAD=./rskt_shim.so exec lynx -dump http://0.0.1.5
LD_PRELOAD=./rskt_shim.so exec gdb --args thttpd -D -h 0.0.1.5 -p 8001
