#!/bin/bash

#  This script creates all read/write goodput script files for
#  parallel DMA throughput measurements.
#
#  This includes individual scripts for 1 byte up to 4MB transfers,
#  for both reads and writes, as well as 2 scripts that will invoke
#  all of the individual scripts.
#
#  The "template.umd" file in this directory is the basis of the
#  individual scripts.
#  
cd "$(dirname "$0")"
printf "\nCreating UMD DMA THROUGHPUT SCRIPTS\n\n"

shopt -s nullglob

DIR_NAME=udma_thru

PREFIX=udma

unset LOC_PRINT_HEP

if [ -z "$MPORT_DIR" ]; then
	if [ -n "$1" ]; then
		MPORT_DIR=$1
	else
		MPORT_DIR=mport0
		LOC_PRINT_HEP=1
	fi
fi

shift

if [ -z "$IBA_ADDR" ]; then
	if [ -n "$1" ]; then
		IBA_ADDR=$1
	else
		IBA_ADDR=0x200000000
		LOC_PRINT_HEP=2
	fi
fi

shift

if [ -z "$DID" ]; then
	if [ -n "$1" ]; then
		DID=$1
	else
		DID=0
		LOC_PRINT_HEP=3
	fi
fi

shift

if [ -z "$TRANS" ]; then
	if [ -n "$1" ]; then
		TRANS=$1
	else
		TRANS=0
		LOC_PRINT_HEP=4
	fi
fi

shift

if [ -z "$WAIT_TIME" ]; then
	if [ -n "$1" ]; then
		WAIT_TIME=$1
	else
		WAIT_TIME=60
		LOC_PRINT_HEP=5
	fi
fi

shift

if [ -z "$BUFC" ]; then
	if [ -n "$1" ]; then
		BUFC=$1
	else
		BUFC=0x100
		LOC_PRINT_HEP=6
	fi
fi

shift

if [ -z "$STS" ]; then
	if [ -n "$1" ]; then
		STS=$1
	else
		STS=0x100
		LOC_PRINT_HEP=7
	fi
fi

shift

if [ -z "$CHAN" ]; then
        if [ -n "$1" ]; then
                CHAN=$1
        else
                CHAN=7
                LOC_PRINT_HEP=8
        fi
fi

shift

if [ -z "$TX_CPU" ]; then
        if [ -n "$1" ]; then
                TX_CPU=$1
        else
                TX_CPU=2
                LOC_PRINT_HEP=9
        fi
fi

shift

if [ -z "$FIFO_CPU" ]; then
        if [ -n "$1" ]; then
                FIFO_CPU=$1
        else
                FIFO_CPU=3
                LOC_PRINT_HEP=10
        fi
fi

shift

if [ -z "$OVERRIDE" ]; then
	if [ -z $1 ] || [ $1 == "N" ]; then
                OVERRIDE='N'
	else 
		OVERRIDE='Y';
        fi
fi

if [ -n "$LOC_PRINT_HEP" ]; then
	echo $'\nScript accepts the following parameters:'
        echo $'MPORT_DIR: /dev/rio_{MPORT_DIR} device used for test.'
        echo $'IBA_ADDR : Hex address of target window on DID'
        echo $'DID      : Device ID of target device for performance scripts'
        echo $'Trans    : DMA transaction type'
        echo $'Wait     : Seconds to wait before taking performance measurement'
        echo $'Bufc     : Number of TX buffers, in hex'
        echo $'Sts      : size of TX FIFO, in hex'
        echo $'Chan     : HW DMA channel 0..7'
        echo $'TX_CPU   : Processor to run the transmit/receive loop'
        echo $'FIFO_CPU : Processor to run the completion FIFO loop'
        echo $'OVERRIDE : <optional>, default and N allows isolcpus'
        echo $'           Any other value forces TX_CPU and FIFO_CPU\n'
fi

# ensure hex values are correctly prefixed
if [[ $IBA_ADDR != 0x* ]] && [[ $IBA_ADDR != 0X* ]]; then
	IBA_ADDR=0x$IBA_ADDR
fi

if [[ $BUFC != 0x* ]] && [[ $BUFC != 0X* ]]; then
	BUFC=0x$BUFC
fi

if [[ $STS != 0x* ]] && [[ $STS != 0X* ]]; then
	STS=0x$STS
fi

echo 'UDMA_THRUPUT MPORT_DIR= '$MPORT   
echo 'UDMA_THRUPUT IBA_ADDR = '$IBA_ADDR
echo 'UDMA_THRUPUT DID      = '$DID
echo 'UDMA_THRUPUT TRANS    = '$TRANS
echo 'UDMA_THRUPUT WAIT_TIME= '$WAIT_TIME
echo 'UDMA_THRUPUT BUFC     = '$BUFC
echo 'UDMA_THRUPUT STS      = '$STS
echo 'UDMA_THRUPUT DMA_CHAN = '$CHAN
echo 'UDMA_THRUPUT TX_CPU   = '$TX_CPU
echo 'UDMA_THRUPUT STS_CPU  = '$FIFO_CPU
echo 'UDMA_THRUPUT OVERRIDE = '$OVERRIDE

unset LOC_PRINT_HEP

# SIZE_NAME is the file name
# SIZE is the hexadecimal representation of SIZE_NAME
#
# The two arrays must match up...

SIZE_NAME=(1B 2B 4B 8B 16B 32B 64B 128B 256B 512B
	1K 2K 4K 8K 16K 32K 64K 128K 256K 512K 
	1M 2M 4M)

SIZE=(
"0x1" "0x2" "0x4" "0x8"
"0x10" "0x20" "0x40" "0x80"
"0x100" "0x200" "0x400" "0x800"
"0x1000" "0x2000" "0x4000" "0x8000"
"0x10000" "0x20000" "0x40000" "0x80000"
"0x100000" "0x200000" "0x400000")

BYTES=(
"0x10000" "0x10000" "0x10000" "0x10000"
"0x10000" "0x10000" "0x10000" "0x10000"
"0x10000" "0x10000" "0x10000" "0x10000"
"0x10000" "0x10000" "0x10000" "0x10000"
"0x10000" "0x20000" "0x40000" "0x80000"
"0x100000" "0x200000" "0x400000")

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
	echo "Mast name idx "$max_name_idx" not equal to max size idx "$max_size_idx
	exit 1
fi;

if [ "$max_name_idx" != "$max_bytes_idx" ]; then
	echo "Mast name idx "$max_name_idx" not equal to max bytes idx "$max_bytes_idx
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
	cp template.umd $filename
	sed -i -- 's/acc_size/'${SIZE[idx]}'/g' $filename
	sed -i -- 's/bytes/'${BYTES[idx]}'/g' $filename
	cp $filename $w_filename
	idx=($idx)+1
done

sed -i -- 's/iba_addr/'$IBA_ADDR'/g' $PREFIX*.txt
sed -i -- 's/did/'$DID'/g' $PREFIX*.txt
sed -i -- 's/trans/'$TRANS'/g' $PREFIX*.txt
sed -i -- 's/wait_time/'$WAIT_TIME'/g' $PREFIX*.txt
sed -i -- 's/bufc/'$BUFC'/g' $PREFIX*.txt
sed -i -- 's/sts/'$STS'/g' $PREFIX*.txt
sed -i -- 's/chan/'$CHAN'/g' $PREFIX*.txt
sed -i -- 's/TX_CPU/'$TX_CPU'/g' $PREFIX*.txt
sed -i -- 's/FIFO_CPU/'$FIFO_CPU'/g' $PREFIX*.txt

if [ "$OVERRIDE" == "Y" ]; then
	sed -i -- 's/isolcpu//g' $PREFIX*.txt
fi

## now create the "run all scripts" script files...

DIR=('read' 'write')
declare -a file_list

for direction in "${DIR[@]}"
do
	scriptname="../"$DIR_NAME"_"$direction 

	echo "// This script runs all "$DIR_NAME $direction" scripts." > $scriptname
	echo "log logs/${MPORT_DIR}/"$DIR_NAME"_"$direction".ulog" >> $scriptname
	echo "scrp ${MPORT_DIR}/${DIR_NAME}" >> $scriptname

	idx=0
	while [ "$idx" -lt "$max_name_idx" ]
	do
		if [ "$direction" == "${DIR[0]}" ]; then
			set_t_filename_r ${SIZE_NAME[idx]}
		else
			set_t_filename_w ${SIZE_NAME[idx]}
		fi
		
		echo "kill all"          >> $scriptname
		echo "sleep "$WAIT_TIME  >> $scriptname
		echo ". "$t_filename >> $scriptname
		idx=($idx)+1
	done
	echo "close" >> $scriptname
	echo "scrp ${MPORT_DIR}"  >> $scriptname
done

ls ../$DIR_NAME*
