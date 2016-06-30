#!/bin/sh

LD_PRELOAD=./rskt_shim.so exec /bin/ls
