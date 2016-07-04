#!/bin/sh

export LD_LIBRARY_PATH=.
LD_PRELOAD=./rskt_shim.so exec /bin/ls
