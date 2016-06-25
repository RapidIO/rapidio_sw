#!/bin/sh

ulimit -c unlimited
#LD_PRELOAD=./rskt_shim.so exec wget -vO /dev/null http://0.0.0.6
LD_PRELOAD=./rskt_shim.so exec lynx -dump http://0.0.0.6
