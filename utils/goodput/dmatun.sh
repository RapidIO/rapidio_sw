#!/bin/bash

ulimit -c unlimited

./ugoodput 0 buf=100 sts=400 mtu=17000 disable_nread=0 thruput=1 push_rp_thr=16 --rc scripts/s
