#!/bin/bash

#  This script creates all read/write goodput script files for
#  parallel DMA throughput measurements.
#
#  This includes individual scripts for 1 byte up to 4MB transfers,
#  for both reads and writes, as well as 2 scripts that will invoke
#  all of the individual scripts.
#
#  The "template" file in this directory is the basis of the
#  individiaul scripts.
#  

cd "$(dirname "$0")"
printf "\nCreating OBWIN GOODPUT SCRIPTS\n\n"

shopt -s nullglob

declare DIR_NAME=obwin_thru

declare PREFIX

# SIZE_NAME is the file name
# SIZE is the hexadecimal representation of SIZE_NAME
#
# The two arrays must match up...

SIZE_NAME=(1B 2B 4B 8B)

SIZE=( "1" "2" "4" "8")

BYTES=("400000" "400000" "400000" "400000")


IBA_ADDR=20d800000
DID=0
TRANS=0
WAIT_TIME=35

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

PREFIXES=(o1 o8)

for pfx in ${PREFIXES[@]}
do
	PREFIX=($pfx)
	idx=0
	while [ "$idx" -lt "$max_name_idx" ]
	do
		declare filename
		declare w_filename

		set_t_filename_r ${SIZE_NAME[idx]}
		filename=$t_filename
		set_t_filename_w ${SIZE_NAME[idx]}
		w_filename=$t_filename
		cp $pfx'_template' $filename
		sed -i -- 's/acc_size/'${SIZE[idx]}'/g' $filename
		sed -i -- 's/bytes/'${BYTES[idx]}'/g' $filename
		cp $filename $w_filename
		idx=($idx)+1
	done

	sed -i -- 's/iba_addr/'$IBA_ADDR'/g' $PREFIX*.txt
	sed -i -- 's/did/'$DID'/g' $PREFIX*.txt
	sed -i -- 's/trans/'$TRANS'/g' $PREFIX*.txt
	sed -i -- 's/wait_time/'$WAIT_TIME'/g' $PREFIX*.txt
	sed -i -- 's/wr/1/g' ${PREFIX}W*.txt
	sed -i -- 's/wr/0/g' ${PREFIX}R*.txt
done

## now create the "run all scripts" script files...

DIR=(read write)
declare -a file_list

for direction in "${DIR[@]}"
do
	scriptname="../"$DIR_NAME"_"$direction 

	echo "#!/bin/bash" > $scriptname
	echo "#  This script runs all "$DIR_NAME $direction" scripts." >> $scriptname
	echo "log "$scriptname".log" >> $scriptname
	echo "scrp scripts/performance/"$DIR_NAME >> $scriptname

	for pfx in ${PREFIXES[@]}
	do
		PREFIX=($pfx)
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
	done
	echo "close" >> $scriptname
done

ls ../$DIR_NAME*
