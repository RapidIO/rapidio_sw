#!/bin/bash

#  This script creates all transmit/receive goodput script files for
#  Messaging goodput measurment
#
#  This includes individual scripts for 1 byte up to 4K message transmission,
#  as well as a script that will invoke all of the individual
#  transmission scripts.
#
#  The "template" files in this directory is the basis of the
#  individiaul scripts.
#  

cd "$(dirname "$0")"
printf "\nCreating MESSAGING THROUGHPUT SCRIPTS\n\n"

shopt -s nullglob

DIR_NAME=msg_thru

PREFIX=m

if [ -z "$DID" ]; then
        if [ -n "$1" ]; then
                DID=$1
        else
                DID=0
                LOC_PRINT_HEP=1
        fi
fi

if [ -z "$MBOX" ]; then
        if [ -n "$2" ]; then
                MBOX=$2
        else
                MBOX=3
                LOC_PRINT_HEP=1
        fi
fi

if [ -z "$WAIT_TIME" ]; then
        if [ -n "$3" ]; then
                WAIT_TIME=$3
        else
                WAIT_TIME=60
                LOC_PRINT_HEP=1
        fi
fi

if [ -z "$BUFC" ]; then
        if [ -n "$4" ]; then
                BUFC=$4
        else
                BUFC=100
                LOC_PRINT_HEP=1
        fi
fi

if [ -z "$STS" ]; then
        if [ -n "$5" ]; then
                STS=$5
        else
                STS=100
                LOC_PRINT_HEP=1
        fi
fi

if [ -n "$LOC_PRINT_HEP" ]; then
        echo $'\nScript accepts the following parameters:'
        echo $'DID : Device ID of target device for performance scripts'
        echo $'MBOX: Channel 2 or 3'
        echo $'Wait: Time in seconds to wait before taking perf measurement\n'
        echo $'Bufc: Number of TX buffers'
        echo $'Sts: size of TX FIFO\n'
fi

echo 'MSG_THRUPUT DID        = ' $DID
echo 'MSG_THRUPUT MBOX       = ' $MBOX
echo 'MSG_THRUPUT WAIT_TIME  = ' $WAIT_TIME

unset LOC_PRINT_HEP

# SIZE_NAME is the file name
# SIZE is the hexadecimal representation of SIZE_NAME
#
# The two arrays must match up...

SIZE_NAME=(24B 32B 64B 128B 256B 512B 1K 2K 4K)

SIZE=( "18" "20" "40" "80" "100" "200" "400" "800" "1000")

# Function to format file names.
# Format is xxZss.txt, where
# xx is the prefix
# Z is W for writes or R for reads, parameter 1
# ss is a string selected from the SIZE_NAME array, parameter 2

declare t_filename

function set_t_filename {
	t_filename=$PREFIX$1$2".txt"
}

function set_t_filename_w {
	set_t_filename "T" $1
}

declare -i max_name_idx=0
declare -i max_size_idx=0
declare -i idx=0

for name in "${SIZE_NAME[@]}"
do
	max_name_idx=($max_name_idx)+1;
done

for sz in "${SIZE[@]}"
do
	max_size_idx=($max_size_idx)+1;
done

if [ "$max_name_idx" != "$max_size_idx" ]; then
	echo "Max name idx "$max_name_idx" not equal to max size idx "$max_size_idx
	exit 1
fi;

echo "Arrays declared correctly..."

idx=0
while [ "$idx" -lt "$max_name_idx" ]
do
	declare filename

	set_t_filename_w ${SIZE_NAME[idx]}
	filename=$t_filename
	sed 's/master/1/g' < template.umd > $filename # TX
	sed -i -- 's/msg_size/'${SIZE[idx]}'/g' $filename
	idx=($idx)+1
done

sed 's/master/0/g' < template.umd | sed 's/msg_size/0/g' > $PREFIX'_rx.txt' # RX

sed -i -- 's/did/'$DID'/g' $PREFIX*.txt
sed -i -- 's/mbox/'$MBOX'/g' $PREFIX*.txt
sed -i -- 's/bufc/'$BUFC'/g' $PREFIX*.txt
sed -i -- 's/sts/'$STS'/g' $PREFIX*.txt
sed -i -- 's/wait_time/'$WAIT_TIME'/g' $PREFIX*.txt

## now create the "run all scripts" script files...

DIR=(tx)
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
		set_t_filename_w ${SIZE_NAME[idx]}

		echo ". "$t_filename >> $scriptname
		idx=($idx)+1
	done
	echo "close" >> $scriptname
	echo "scrp scripts/performance/" >> $scriptname
done

ls ../$DIR_NAME*
