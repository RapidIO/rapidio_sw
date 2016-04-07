#!/bin/bash

## This script creates two files:
## lat_pass.txt indicates success counts for all lines in the summary file
## lat_fail.txt indicates information about failures
##
## The script only passes if lat_fail.txt does not exist, and 
## lat_pass.txt does exist and is non-empty.
##
## This script checks that the throughput numbers computed are expected.
#3 There are four kinds of lines:
##
## CHECK input file list: Processing latency for file :
## - 4 lines, one line for each group of performance figures, 
##
## CHECK output file list: Output filename is             :
## - 4 lines, one line for each group of performance figures, 
##
## CHECK Header: SIZE .. Mbps...
## - 11 lines, one line for each group of performance figures, 
##
## CHECK DATA :size mbps gbps linkocc availcpu usercpu kernelcpu cpu OCC %
## Totals for each SIZE value add up
## There should never be a line with 
## - UserCPU, KernCPU, CPU OCC all 0
## - MBps/Gbps/LinkOcc all 0 
## 
## Also check individual log files for the following keywords,
##   which indicate failure:
## Unknown
## FAILED

PRINT_HELP=0

if [ -n "$1" ]
  then
    FILENAME=$1
else
        PRINT_HELP=1
fi

if [ $PRINT_HELP != "0" ]; then
        echo $'\nScript requires the following parameters:'
        echo $'FILENAME : Name of file in local directory containing'
        echo $'           output of summ_lat_logs.sh for all latency'
        echo $'           scripts.\n'
        echo $'Once complete, two files will be present in the directory'
        echo $'lat_pass.txt: File with all counts which passed.'
        echo $'lat_fail.txt: File with all counts and lines which failed'
        exit 1
fi;

SIZE_STRINGS=( XOutput XProcessing XSIZE
	"X0x18"  
	"X0x20"
	"X0x40"  
	"X0x80"
	"X0x100" 
	"X0x200"
	"X0x400"
	"X0x800"
	"X0x1000"
	"X1"
	"X2"
	"X4"
	"X8"
	"X10"
	"X20"
	"X40"
	"X80"
	"X100"
	"X200"
	"X400"
	"X800"
	"X1000"
	"X2000"
	"X4000"
	"X8000" 
	"X10000"
	"X20000"
	"X40000"
	"X80000"
	"X100000"
	"X200000"
	"X400000" )

SIZE_COUNT=( 11 11 11
	1 1 1 1
	1 1 1 1
	1 
	12 12 12 12
	8  8  8  8
	8  8  8  8
	8  8  8  8
	8  8  8  8
	8  8  8  )


IDX=0

PASS=lat_pass.txt
FAIL=lat_fail.txt

rm -f $PASS
rm -f $FAIL

if [ ! -s ${FILENAME} ]; then
        echo "${FILENAME} is empty or does not exist!" > $FAIL
        exit
fi

sed -e 's/^ *//' ${FILENAME} > ${FILENAME}.temp
sed -i 's/^/X/' ${FILENAME}.temp 

# Check that expected number of lines is present
for SIZE in ${SIZE_STRINGS[@]}; do
	CNT="$( grep "${SIZE} " ${FILENAME}.temp | wc -l )"
	if [ ${CNT} == ${SIZE_COUNT[IDX]} ]; then
		echo "$SIZE $CNT ${SIZE_COUNT[IDX]}" >> ${PASS}
	else 
		echo "SIZE ${SIZE//X/} GOT $CNT EXP ${SIZE_COUNT[IDX]} FAIL" >> ${FAIL}
	fi;
	let "IDX = $IDX + 1"
done 

# Check that there are no lines with illegal values

CNT="$( grep " 0 " ${FILENAME} | wc -l )"
if [ "$CNT" -ne "0" ]; then
	echo $'\nFAIL: ZERO Transfers\n' >> ${FAIL}
	grep " 0 " ${FILENAME} >> ${FAIL}
fi;

CNT="$( grep " 0\.0" ${FILENAME} | wc -l )"
if [ "$CNT" -ne "0" ]; then
	echo $'\nFAIL: ZERO Min, Avg, or Max latency\n' >> ${FAIL}
	grep " 0\.0" ${FILENAME} >> ${FAIL}
fi

CNT="$( grep -E 'FAILED|Unknown|CRIT|ERR|WARN' *.log | wc -l )"
if [ "$CNT" -ne "0" ]; then
	echo $'\nFAIL: Keywords indicating errors exist in log files\n' >> ${FAIL}
fi

# ONLY PASS IF THE PASS FILE EXISTS AND THE FAIL FILE DOESN'T

if [ ! -s ${PASS} ]; then
	echo "FAILED, ${PASS} either does not exist or is empty!"
else
	if [ -f ${FAIL} ]; then
		echo "FAILED!"
		cat ${FAIL}
	else
		echo "PASSED!"
	fi
fi
