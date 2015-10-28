#!/bin/bash

cd "$(dirname "$0")"
printf "\nCreating DMA LATENCY SCRIPTS\n\n"

shopt -s nullglob

DIR_NAME=dma_lat
PREFIX=dl

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
        echo $'Wait       : Time in seconds to wait before publish performance'
fi

echo $'\nDMA_LATENCY TX_IBA_ADDR = ' $TX_IBA_ADDR
echo 'DMA_LATENCY TX_DID      = ' $TX_DID
echo 'DMA_LATENCY RX_IBA_ADDR = ' $RX_IBA_ADDR
echo 'DMA_LATENCY RX_DID      = ' $RX_DID
echo 'DMA_LATENCY TRANS       = ' $TRANS
echo 'DMA_LATENCY WAIT_TIME   = ' $WAIT_TIME

unset LOC_PRINT_HEP

# SIZE_NAME is the file name
# SIZE is the hexadecimal representation of SIZE_NAME
# BYTES is the amount of memory to allocate, larger than SIZE
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
"400000" "400000" "400000" "400000" 
"400000" "400000" "400000" "400000"
"400000" "400000" "400000" "400000"
"400000" "400000" "400000" "400000"
"400000" "400000" "400000" "400000"
"400000" "400000" "400000")

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

echo "Arrays declared correctly..."

## First create all of the transmit scripts for reads and writes

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

## Next create all of the target scripts for receiver to loop back writes

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

## update variable values in all created files...

sed -i -- 's/tx_iba_addr/'$TX_IBA_ADDR'/g' $PREFIX*.txt
sed -i -- 's/tx_did/'$TX_DID'/g' $PREFIX*.txt
sed -i -- 's/rx_iba_addr/'$RX_IBA_ADDR'/g' $PREFIX*.txt
sed -i -- 's/rx_did/'$RX_DID'/g' $PREFIX*.txt
sed -i -- 's/trans/'$TRANS'/g' $PREFIX*.txt
sed -i -- 's/wait_time/'$WAIT_TIME'/g' $PREFIX*.txt
sed -i -- 's/wr/1/g' ${PREFIX}W*.txt
sed -i -- 's/wr/0/g' ${PREFIX}R*.txt

## now create the "run all scripts" script files, for read lantency files only

DIR=(read)
declare -a file_list

for direction in "${DIR[@]}"
do
	scriptname="../"$DIR_NAME"_"$direction 

	echo "#!/bin/bash" > $scriptname
	echo "#  This script runs all "$DIR_NAME $direction" scripts." >> $scriptname
	echo "log "$DIR_NAME"_"$direction".log" >> $scriptname
	echo "scrp scripts/performance/"$DIR_NAME >> $scriptname

	idx=0
	while [ "$idx" -lt "$max_name_idx" ]
	do
		if [$direction=="read"]; then
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
