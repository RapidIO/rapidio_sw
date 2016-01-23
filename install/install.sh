#!/bin/bash

SOURCE_PATH="/opt/rapidio/rapidio_sw"
CONFIG_PATH="/etc/rapidio"
SCRIPTS_PATH=$SOURCE_PATH"/install"

MASTER=$1
SLAVES=( $2 $3 $4 )
ALLNODES=( $1 $2 $3 $4 )

GRP=$5

REL=$6

if [ "$#" -lt 5 ]; then
    echo "Correct usage is <NODE1> <NODE2> <NODE3> <NODE4> <group> <release>"
    echo "<NODE1> Name of master, enumerating node"
    echo "<NODE2> Name of slave node conncected to Switch Port 2"
    echo "<NODE3> Name of slave node conncected to Switch Port 3"
    echo "<NODE4> Name of slave node conncected to Switch Port 4"
    echo "<group> Unix file ownership group which should have access to"
    echo "        the RapidIO software"
    echo "<rel> is the software release/version to install."
    echo "        If no release is supplied, the current release is installed."
    exit
fi

echo "Beginning installation..."

for i in "${ALLNODES[@]}"
do
	echo $i" Compilation and documentation generation starting..."
	if [ -z "$6" ]; then
		ssh root@"$i" "cd $SOURCE_PATH; git branch | grep '*' "
	else
		ssh root@"$i" "cd $SOURCE_PATH; git checkout $REL "
	fi
	ssh root@"$i" "$SCRIPTS_PATH/make_install.sh $SOURCE_PATH $GRP"
	echo $i" Compilation and documentation generation COMPLETED.."
done

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

echo "Installing configuration files..."

FILENAME=$CONFIG_PATH/fmd.conf

echo "copying "$1
ssh root@"$1" "cp $SCRIPTS_PATH/node1.conf $FILENAME"
echo "copying "$2
ssh root@"$2" "cp $SCRIPTS_PATH/node2.conf $FILENAME"
echo "copying "$3
ssh root@"$3" "cp $SCRIPTS_PATH/node3.conf $FILENAME"
echo "copying "$4
ssh root@"$4" "cp $SCRIPTS_PATH/node4.conf $FILENAME"


for i in "${ALLNODES[@]}"
do
	echo "Editing "$i
	ssh root@"$i" sed -i -- 's/node1/'$1'/g' $FILENAME""
	ssh root@"$i" sed -i -- 's/node2/'$2'/g' $FILENAME""
	ssh root@"$i" sed -i -- 's/node3/'$3'/g' $FILENAME""
	ssh root@"$i" sed -i -- 's/node4/'$4'/g' $FILENAME""
done

echo "Installation of configuration files COMPLETED..."

echo "Installion complete."
