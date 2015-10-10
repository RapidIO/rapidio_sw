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

TX_IBA_ADDR=20d800000
TX_DID=1
RX_IBA_ADDR=20d800000
RX_DID=0
TRANS=0
WAIT_TIME=30

PREFIX=ol

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
