#!/bin/bash

#  This script creates all read/write goodput script files for
#  OBDIO Latency measurements.
#
#  This includes individual scripts for 1 byte up to 8 transfers,
#  for both reads and writes, as well as 2 scripts that will invoke
#  all of the individual scripts.
#
#  The "template" files in this directory is the basis of the
#  individiaul scripts.
#  

cd "$(dirname "$0")"
printf "\nCreating OBWIN LATENCY SCRIPTS\n\n"

shopt -s nullglob

DIR_NAME=obwin_lat

# SIZE_NAME is the file name
# SIZE is the hexadecimal representation of SIZE_NAME
#
# The two arrays must match up...

SIZE_NAME=(1B 2B 4B 8B)

SIZE=("1" "2" "4" "8")

BYTES=("400000" "400000" "400000" "400000")

PREFIX=ol

if [ -z "$TX_IBA_ADDR" ]; then
        if [ -n "$1" ]; then
                TX_IBA_ADDR=$1
        else
                TX_IBA_ADDR=20d800000
                LOC_PRINT_HEP=1
        fi
fi

if [ -z "$TX_DID" ]; then
        if [ -n "$2" ]; then
                TX_DID=$2
        else
                TX_DID=1
                LOC_PRINT_HEP=1
        fi
fi

if [ -z "$RX_IBA_ADDR" ]; then
        if [ -n "$3" ]; then
                RX_IBA_ADDR=$3
        else
                RX_IBA_ADDR=20d800000
                LOC_PRINT_HEP=1
        fi
fi

if [ -z "$RX_DID" ]; then
        if [ -n "$4" ]; then
                RX_DID=$4
        else
                RX_DID=0
                LOC_PRINT_HEP=1
        fi
fi

if [ -z "$TRANS" ]; then
        if [ -n "$5" ]; then
                TRANS=$5
        else
                TRANS=0
                LOC_PRINT_HEP=1
        fi
fi

if [ -z "$WAIT_TIME" ]; then
        if [ -n "$6" ]; then
                WAIT_TIME=$6
        else
                WAIT_TIME=60
                LOC_PRINT_HEP=1
        fi
fi

if [ -n "$LOC_PRINT_HEP" ]; then
        echo $'\nScript accepts 6 parameters:'
        echo $'TX_IBA_ADDR: Hex address of target window on TX_DID'
        echo $'TX_DID     : Device ID that this device sends to'
        echo $'RX_IBA_ADDR: Hex address of target window on RX_DID'
        echo $'RX_DID     : Device ID of this device'
        echo $'Trans      : DMA transaction type'
        echo $'Wait       : Time in seconds to wait before showing perf\n'
fi

echo 'OBWIN_LATENCY TX_IBA_ADDR = ' $TX_IBA_ADDR
echo 'OBWIN_LATENCY TX_DID      = ' $TX_DID
echo 'OBWIN_LATENCY RX_IBA_ADDR = ' $RX_IBA_ADDR
echo 'OBWIN_LATENCY RX_DID      = ' $RX_DID
echo 'OBWIN_LATENCY TRANS       = ' $TRANS
echo 'OBWIN_LATENCY WAIT_TIME   = ' $WAIT_TIME

unset LOC_PRINT_HEP

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

function set_t_filename_t {
	set_t_filename "T" $1
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

echo $'\nArrays declared correctly...'

## Create files for DIOT commands, transmit latency

idx=0
while [ "$idx" -lt "$max_name_idx" ]
do
	declare filename
	declare w_filename

	set_t_filename_r ${SIZE_NAME[idx]}
	filename=$t_filename
	set_t_filename_w ${SIZE_NAME[idx]}
	w_filename=$t_filename
	cp tx_template $filename
	sed -i -- 's/acc_size/'${SIZE[idx]}'/g' $filename
	sed -i -- 's/bytes/'${BYTES[idx]}'/g' $filename
	cp $filename $w_filename
	idx=($idx)+1
done

## Create files for DIOR commands, Target of latency

idx=0
while [ "$idx" -lt "$max_name_idx" ]
do
	declare filename
	declare w_filename

	set_t_filename_t ${SIZE_NAME[idx]}
	filename=$t_filename
	cp rx_template $filename
	sed -i -- 's/acc_size/'${SIZE[idx]}'/g' $filename
	sed -i -- 's/bytes/'${BYTES[idx]}'/g' $filename
	idx=($idx)+1
done

sed -i -- 's/tx_iba_addr/'$TX_IBA_ADDR'/g' $PREFIX*.txt
sed -i -- 's/tx_did/'$TX_DID'/g' $PREFIX*.txt
sed -i -- 's/rx_iba_addr/'$RX_IBA_ADDR'/g' $PREFIX*.txt
sed -i -- 's/rx_did/'$RX_DID'/g' $PREFIX*.txt
sed -i -- 's/trans/'$TRANS'/g' $PREFIX*.txt
sed -i -- 's/wait_time/'$WAIT_TIME'/g' $PREFIX*.txt
sed -i -- 's/wr/1/g' ${PREFIX}W*.txt
sed -i -- 's/wr/0/g' ${PREFIX}R*.txt

## now create the "run all scripts" script files...


direction="read"
scriptname="../"$DIR_NAME"_"$direction 

echo "#!/bin/bash" > $scriptname
echo "#  This script runs all "$DIR_NAME $direction" scripts." >> $scriptname
echo "log "$DIR_NAME"_"$direction".log" >> $scriptname
echo "scrp scripts/performance/"$DIR_NAME >> $scriptname

idx=0
while [ "$idx" -lt "$max_name_idx" ]
do
	set_t_filename_t ${SIZE_NAME[idx]}
	echo ". "$t_filename >> $scriptname
	idx=($idx)+1
done
echo "close" >> $scriptname
echo "scrp scripts/performance/" >> $scriptname

ls ../$DIR_NAME*
