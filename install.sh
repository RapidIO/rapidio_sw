#!/bin/bash

# SOURCE_PATH="/opt/rapidio/rapidio_sw"
SOURCE_PATH="/home/barryw/fmd/rapidio_sw"
CONFIG_PATH="/etc/rapidio"
SCRIPTS_PATH=$SOURCE_PATH"/install"

PRINTHELP=0

if [ "$#" -lt 6 ]; then
	echo $'\ninstall.sh requires 6 parameters.\n'
	PRINTHELP=1
elif [ $5 != 'mem34' -a $5 != 'mem50' -a $5 != 'mem66' ] ; then
	echo $'\nmemsz parameter must be mem34, mem50, or mem66.\n'
	PRINTHELP=1
fi

if [ $PRINTHELP = 1 ] ; then
    echo "install.sh <NODE1> <NODE2> <NODE3> <NODE4> <memsz> <group> <rel>"
    echo "<NODE1> Name of master, enumerating node"
    echo "<NODE2> Name of slave node conncected to Switch Port 2"
    echo "<NODE3> Name of slave node conncected to Switch Port 3"
    echo "<NODE4> Name of slave node conncected to Switch Port 4"
    echo "If any of <NODE2> <NODE3> <NODE4> is \"none\", the node is ignored."
    echo "<memsz> RapidIO memory size, one of mem34, mem50, mem66"
    echo "        If any node has more than 8 GB of memory, MUST use mem50"
    echo "<group> Unix file ownership group which should have access to"
    echo "        the RapidIO software"
    echo "<rel> is the software release/version to install."
    echo "        If no release is supplied, the current release is installed."
    exit
fi

MASTER=$1
SLAVES=( )
ALLNODES=( $1 )
NODE2=""
NODE3=""
NODE4=""

if [ $2 != 'none' ]; then
	NODE2=$2
	SLAVES[2]=$2
	ALLNODES[2]=$2
fi

if [ $3 != 'none' ]; then
	NODE3=$3
	SLAVES[3]=$3
	ALLNODES[3]=$3
fi

if [ $4 != 'none' ]; then
	NODE4=$4
	SLAVES[4]=$4
	ALLNODES[4]=$4
fi

MEMSZ=$5
GRP=$6
REL=$7

for i in "${ALLNODES[@]}"
do
	ping -c 1 $i > /dev/null
	if [ $? -ne 0 ]; then
		echo $i " Not accessible, aborting..."
		exit
	else
		echo $i "accessible."
	fi
done


echo "Beginning installation..."

for i in "${ALLNODES[@]}"
do
	echo $i" Compilation and documentation generation starting..."
	if [ -z "$7" ]; then
		ssh root@"$i" "cd $SOURCE_PATH; git branch | grep '*' "
	else
		ssh root@"$i" "cd $SOURCE_PATH; git checkout $REL "
	fi
	ssh root@"$i" "$SCRIPTS_PATH/make_install.sh $SOURCE_PATH $GRP"
	echo $i" Compilation and documentation generation COMPLETED.."
done

echo "Installing configuration files..."

FILENAME=$CONFIG_PATH/fmd.conf

echo "copying "$1
ssh root@"$1" "mkdir -p $CONFIG_PATH"
ssh root@"$1" "cp $SCRIPTS_PATH/node1.conf $FILENAME"

if [ $2 != 'none' ]; then
	echo "copying "$2
	ssh root@"$2" "mkdir -p $CONFIG_PATH"
	ssh root@"$2" "cp $SCRIPTS_PATH/node2.conf $FILENAME"
fi
if [ $3 != 'none' ]; then
	echo "copying "$3
	ssh root@"$3" "mkdir -p $CONFIG_PATH"
	ssh root@"$3" "cp $SCRIPTS_PATH/node3.conf $FILENAME"
fi
if [ $4 != 'none' ]; then
	echo "copying "$4
	ssh root@"$4" "mkdir -p $CONFIG_PATH"
	ssh root@"$4" "cp $SCRIPTS_PATH/node4.conf $FILENAME"
fi

for i in "${ALLNODES[@]}"
do
	echo "Editing "$i
	ssh root@"$i" sed -i -- 's/node1/'$1'/g' $FILENAME""
	if [ -n "$NODE2" ]; then
		ssh root@"$i" sed -i -- 's/node2/'$NODE2'/g' $FILENAME""
	else
		ssh root@"$i" sed -i '/node2/d' $FILENAME
	fi
	if [ -n "$NODE3" ]; then
		ssh root@"$i" sed -i -- 's/node3/'$NODE3'/g' $FILENAME""
	else
		ssh root@"$i" sed -i '/node3/d' $FILENAME
	fi
	if [ -n "$NODE4" ]; then
		ssh root@"$i" sed -i -- 's/node4/'$NODE4'/g' $FILENAME""
	else
		ssh root@"$i" sed -i '/node4/d' $FILENAME
	fi
	ssh root@"$i" sed -i -- 's/MEMSZ/'$MEMSZ'/g' $FILENAME""
done

echo "Installation of configuration files COMPLETED..."

FILES=( rio_start.sh stop_rio.sh all_start.sh stop_all.sh check_all.sh )

for f in "${FILES[@]}"
do
	FILENAME=$SOURCE_PATH/$f

	cp $SCRIPTS_PATH/$f $FILENAME
	sed -i -- 's/node1/'$1'/g' $FILENAME
	sed -i -- 's/node2/'$NODE2'/g' $FILENAME
	sed -i -- 's/node3/'$NODE3'/g' $FILENAME
	sed -i -- 's/node4/'$NODE4'/g' $FILENAME
done

echo "Installion complete."
