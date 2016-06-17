#!/bin/bash

mport=0

ulimit -c 0

if [ -z "$DMATUN_PATH" ]; then
  dir=/opt/rapidio/rapidio_sw/utils/goodput
else
  dir="$DMATUN_PATH"
fi

cd $dir || { echo "Cannot chdir to $dir" 1>&2; exit 1; }

[ -d scripts ] || { echo "Path $dir/scripts does not exist." 1>&2; exit 2; }

ulimit -c unlimited

GDB='';
[ "$1" = "-g" ] && GDB='gdb --args ';

# dma_method=1 is libmport/kernel
$GDB ./ugoodput $mport \
	buf=100 sts=400 mtu=17000 \
	disable_nread=0 thruput=1 push_rp_thr=16 \
	dma_method=0 \
	--rc mport${mport}/s
