#!/bin/bash

#  This script creates all read/write goodput script files for
#  parallel DMA throughput measurements.
#
#  This includes individual scripts for 1 byte up to 4MB transfers,
#  for both reads and writes, as well as 2 scripts that will invoke
#  all of the individual scripts.
#
#  The "template.umd" file in this directory is the basis of the
#  individiaul scripts.
#  
cd "$(dirname "$0")"
printf "\nCreating USER MODE DMA THROUGHPUT SCRIPTS\n\n"

shopt -s nullglob

DIR_NAME=udma_thru

PREFIX=ud1

if [ -z "$IBA_ADDR" ]; then
	if [ -n "$1" ]; then
		IBA_ADDR=$1
	else
		IBA_ADDR=20d800000
		LOC_PRINT_HEP=1
	fi
fi

if [ -z "$DID" ]; then
	if [ -n "$2" ]; then
		DID=$2
	else
		DID=0
		LOC_PRINT_HEP=1
	fi
fi

if [ -z "$TRANS" ]; then
	if [ -n "$3" ]; then
		TRANS=$3
	else
		TRANS=0
		LOC_PRINT_HEP=1
	fi
fi

if [ -z "$WAIT_TIME" ]; then
	if [ -n "$4" ]; then
		WAIT_TIME=$4
	else
		WAIT_TIME=60
		LOC_PRINT_HEP=1
	fi
fi

if [ -z "$BUFC" ]; then
	if [ -n "$5" ]; then
		BUFC=$5
	else
		BUFC=100
		LOC_PRINT_HEP=1
	fi
fi

if [ -z "$STS" ]; then
	if [ -n "$6" ]; then
		STS=$6
	else
		STS=100
		LOC_PRINT_HEP=1
	fi
fi

if [ -z "$CPU_1" ]; then
	if [ -n "$7" ]; then
		CPU_1=$7
	else
		CPU_1=2
		LOC_PRINT_HEP=1
	fi
fi

if [ -z "$CPU_2" ]; then
	if [ -n "$8" ]; then
		CPU_2=$8
	else
		CPU_2=3
		LOC_PRINT_HEP=1
	fi
fi

if [ -z "$CHANNEL" ]; then
	if [ -n "$9" ]; then
		CHANNEL=$9
	else
		CHANNEL=3
		LOC_PRINT_HEP=1
	fi
fi

if [ -n "$LOC_PRINT_HEP" ]; then
	echo $'\nScript accepts the following parameters:'
        echo $'IBA_ADDR: Hex address of target window on DID'
        echo $'DID     : Device ID of target device for performance scripts'
        echo $'Trans   : UDMA Write transaction type'
	echo $'          1 LAST_NWR, 2 NW, 3 NW_R'
        echo $'Wait    : Time in seconds to wait before taking perf measurement'
        echo $'Bufc    : Number of TX buffers'
        echo $'Sts     : size of TX FIFO'
        echo $'CPU_1   : Core for the DMA transmit thread'
        echo $'CPU_2   : Core for the DMA transmit FIFO thread'
        echo $'CHANNEL : Tsi721 DMA channel, 2 through 7\n'
fi

echo $'UDMA_THRUPUT IBA_ADDR = ' $IBA_ADDR
echo $'UDMA_THRUPUT DID      = ' $DID
echo $'UDMA_THRUPUT Wr TRANS = ' $TRANS
echo $'UDMA_THRUPUT WAIT_TIME= ' $WAIT_TIME
echo $'UDMA_THRUPUT BUFC     = ' $BUFC
echo $'UDMA_THRUPUT STS      = ' $STS
echo $'UDMA_THRUPUT CPU_1    = ' $CPU_1
echo $'UDMA_THRUPUT CPU_2    = ' $CPU_2
echo $'UDMA_THRUPUT CHANNEL  = ' $CHANNEL

unset LOC_PRINT_HEP

# SIZE_NAME is the file name
# SIZE is the hexadecimal representation of SIZE_NAME
#
# The two arrays must match up...

SIZE_NAME=(1B 2B 4B 8B 16B 32B 64B 128B 256B 512B
	1K 2K 4K 8K 16K 32K 64K 128K 256K 512K 
	1M 2M 4M)

SIZE=(
"1" "2" "4" "8" 
"10" "20" "40" "80"
"100" "200" "400" "800"
"1000" "2000" "4000" "8000"
"10000" "20000" "40000" "80000"
"100000" "200000" "400000")

BYTES=(
"10000" "10000" "10000" "10000" 
"10000" "10000" "10000" "10000"
"10000" "10000" "10000" "10000"
"10000" "10000" "10000" "10000"
"10000" "20000" "40000" "80000"
"100000" "200000" "400000")

# Function to format file names.
# Format is xxZss.txt, where
# xx is the prefix
# Z is W for writes or R for reads, parameter 1
# ss is a string selected from the SIZE_NAME array, parameter 2

declare t_filename

function set_t_filename {
	t_filename=$PREFIX$1$2".txt"
}

function set_t_filename_r {
	set_t_filename "R" $1
}

function set_t_filename_w {
	set_t_filename "W" $1
}

declare -i max_name_idx=0
declare -i max_size_idx=0
declare -i max_bytes_idx=0
declare -i idx=0

for name in "${SIZE_NAME[@]}"
do
	max_name_idx=($max_name_idx)+1;
done

for sz in "${SIZE[@]}"
do
	max_size_idx=($max_size_idx)+1;
done

for sz in "${BYTES[@]}"
do
	max_bytes_idx=($max_bytes_idx)+1;
done

if [ "$max_name_idx" != "$max_size_idx" ]; then
	echo "Max name idx "$max_name_idx" not equal to max size idx "$max_size_idx
	exit 1
fi;

if [ "$max_name_idx" != "$max_bytes_idx" ]; then
	echo "Max name idx "$max_name_idx" not equal to max bytes idx "$max_bytes_idx
	exit 1
fi;

echo "Arrays declared correctly..."

idx=0
while [ "$idx" -lt "$max_name_idx" ]
do
	declare filename
	declare w_filename

	set_t_filename_r ${SIZE_NAME[idx]}
	filename=$t_filename
	set_t_filename_w ${SIZE_NAME[idx]}
	w_filename=$t_filename
	cp template $filename
	sed -i -- 's/acc_size/'${SIZE[idx]}'/g' $filename
	sed -i -- 's/bytes/'${BYTES[idx]}'/g' $filename
	cp $filename $w_filename
	idx=($idx)+1
done

sed -i -- 's/iba_addr/'$IBA_ADDR'/g' $PREFIX*.txt
sed -i -- 's/did/'$DID'/g' $PREFIX*.txt
sed -i -- 's/trans/0/g' $PREFIX'R'*.txt
sed -i -- 's/trans/'$TRANS'/g' $PREFIX'W'*.txt
sed -i -- 's/wait_time/'$WAIT_TIME'/g' $PREFIX*.txt
sed -i -- 's/bufc/'$BUFC'/g' $PREFIX*.txt
sed -i -- 's/sts/'$STS'/g' $PREFIX*.txt
sed -i -- 's/cpu1/'$CPU_1'/g' $PREFIX*.txt
sed -i -- 's/cpu2/'$CPU_2'/g' $PREFIX*.txt
sed -i -- 's/channel/'$CHANNEL'/g' $PREFIX*.txt

## now create the "run all scripts" script files...

DIR=('read' 'write')
declare -a file_list

for direction in "${DIR[@]}"
do
	scriptname="../"$DIR_NAME"_"$direction 

	echo "// This script runs all "$DIR_NAME $direction" scripts." > $scriptname
	echo "log logs/"$DIR_NAME"_"$direction".log" >> $scriptname
	echo "scrp scripts/performance/"$DIR_NAME >> $scriptname

	idx=0
	while [ "$idx" -lt "$max_name_idx" ]
	do
		if [ "$direction" == "${DIR[0]}" ]; then
			set_t_filename_r ${SIZE_NAME[idx]}
		else
			set_t_filename_w ${SIZE_NAME[idx]}
		fi
		
		echo ". "$t_filename >> $scriptname
		idx=($idx)+1
	done
	echo "close" >> $scriptname
	echo "scrp scripts/performance/" >> $scriptname
done

ls ../$DIR_NAME*