#!/bin/bash

# Files required for installation
# Note the names of these file name (different root) are also used by make_install.sh
#
INSTALL_ROOT="/opt/rapidio/.install"
SCRIPTS_PATH="$(pwd)"/install

NODEDATA_FILE="nodeData.txt"
SRC_TAR="rapidio_sw.tar"
TMPL_FILE="config.tmpl"

PGM_NAME=install_list.sh
PGM_NUM_PARMS=5

# Validate input
#
PRINTHELP=0

if [ "$#" -lt $PGM_NUM_PARMS ]; then
    echo $'\n$PGM_NAME requires $PGM_NUM_PARMS parameters.\n'
    PRINTHELP=1
else
    SERVER=$1

    OK=1
    ALLNODES=();
    # format of input file: <master|slave> <hostname> <rioname> <nodenumber>
    while read -r line || [[ -n "$line" ]]; do
        arr=($line)
        host="${arr[1]}"
        if [ "${arr[0]}" = 'master' ]; then
            if [ -n "$MASTER" ]; then
                echo "Multiple master entries ($line) in $2"
                OK=0
            fi
            MASTER=$host
        fi
        ALLNODES+=("$host")
    done < "$2"

    if [ -z "$MASTER" ]; then
        echo "No master entry in $2"
        OK=0
    fi

    if [ $OK -eq 0 ]; then
        echo "Errors in nodeData file $2, exiting..."
        exit
    fi

    MEMSZ=$3
    SW_TYPE=$4
    GRP=$5
    REL=$6

    if [ $MEMSZ != 'mem34' -a $MEMSZ != 'mem50' -a $MEMSZ != 'mem66' ] ; then
        echo $'\nmemsz parameter must be mem34, mem50, or mem66.\n'
        PRINTHELP=1
    fi

    MASTER_CONFIG_FILE=$SCRIPTS_PATH/$SW_TYPE-master.conf
    MASTER_MAKE_FILE=$SCRIPTS_PATH/$SW_TYPE-master-make.sh

    if [ ! -e "$MASTER_CONFIG_FILE" ] || [ ! -e "$MASTER_MAKE_FILE" ]
    then
        echo Switch type \"$SW_TYPE\" configuration support files do not exist.
        PRINTHELP=1
    fi
fi

if [ $PRINTHELP = 1 ] ; then
    echo "$PGM_NAME <SERVER> <nData> <memsz> <sw> <group> <rel>"
    echo "<SERVER> Name of the node providing the files required by installation"
    echo "<nData>  The file describing the target nodes of the install"
    echo "         The file has the format:"
    echo "         <master|slave> <IP_Name> <RIO_name> <node>"
    echo "         Where:"
    echo "         <IP_name> : IP address or DNS name of the node"
    echo "         <RIO_name>: Fabric management node name."
    echo "         <node>    : String to replace in template file,"
    echo "                     of the form node#."
    echo "         EXAMPLE: master 10.64.15.199 gry37 node1"
    echo "         NOTE: Example nodeData.sh files are create by install.sh"
    echo "<memsz>  RapidIO memory size, one of mem34, mem50, mem66"
    echo "         If any node has more than 8 GB of memory, MUST use mem50"
    echo "<sw>     Type of switch the four nodes are connected to."
    echo "         Files exist for the following switch types:"
    echo "         tor  - Prodrive Technologies Top of Rack Switch"
    echo "         cps  - StarBridge Inc RapidExpress Switch"
    echo "         auto - configuration determined at runtime"
    echo "         rxs  - StarBridge Inc RXS RapidExpress Switch"
    echo "<group>  Unix file ownership group which should have access to"
    echo "         the RapidIO software"
    echo "<rel>    The software release/version to install."
    echo "         If no release is supplied, the current release is installed."
    exit
fi


# Only proceed if all nodes can be reached
#
echo "Prepare for installation..."
echo "Checking connectivity..."
OK=1
ping -c 1 $SERVER > /dev/null
if [ $? -ne 0 ]; then
    echo "    $SERVER not accessible"
    OK=0
else
    echo "    $SERVER accessible."
fi

for host in "${ALLNODES[@]}"
do
    [ "$host" = 'none' ] && continue;
    [ "$host" = "$SERVER" ] && continue;
    ping -c 1 $host > /dev/null
    if [ $? -ne 0 ]; then
        echo "    $host not accessible"
        OK=0
    else
        echo "    $host accessible."
    fi
done

if [ $OK -eq 0 ]; then
    echo "\nCould not connect to all nodes, exiting...
    exit
fi


echo "Creating install files for $SERVER..."
# First create the files that would be available on the server
#
ROOT="/tmp/$$"
rm -rf $ROOT;mkdir -p $ROOT

# Copy nodeData.txt
#
cp $2 $ROOT/$NODEDATA_FILE

# Create the source.tar
#
make clean &>/dev/null
tar -cf $ROOT/$SRC_TAR * .git* &>/dev/null

# Copy the template file
#
cp $MASTER_CONFIG_FILE $ROOT/$TMPL_FILE

# Transfer the files to the server
#
echo "Transferring install files to $SERVER..."
SERVER_ROOT="/opt/rapidio/.server"
ssh root@"$SERVER" "rm -rf $SERVER_ROOT;mkdir -p $SERVER_ROOT"
scp $ROOT/* root@"$SERVER":$SERVER_ROOT/. > /dev/null
ssh root@"$SERVER" "chown -R root.$GRP $SERVER_ROOT"
rm -rf $ROOT

# Transfer the make_install.sh script to a known location on the target machines
#
for host in "${ALLNODES[@]}"; do
    [ "$host" = 'none' ] && continue;
    echo "Transferring install script to $host..."
    ssh root@"$host" "rm -rf $INSTALL_ROOT;mkdir -p $INSTALL_ROOT/script"
    scp $SCRIPTS_PATH/make_install_common.sh root@"$host":$INSTALL_ROOT/script/make_install_common.sh > /dev/null
    if [ "$host" = "$MASTER" ]; then
        scp $MASTER_MAKE_FILE root@"$host":$INSTALL_ROOT/script/make_install.sh > /dev/null
    else
        scp $SCRIPTS_PATH/make_install-slave.sh root@"$host":$INSTALL_ROOT/script/make_install.sh > /dev/null
    fi
    ssh root@"$host" "chown -R root.$GRP $INSTALL_ROOT;chmod 755 $INSTALL_ROOT/script/make_install.sh"
done


# Call out to make_install.sh
echo "Beginning installation..."
for host in  "${ALLNODES[@]}"; do
    [ "$host" = 'none' ] && continue;
    ssh root@"$host" "$INSTALL_ROOT/script/make_install.sh $SERVER $SERVER_ROOT $MEMSZ $GRP"
done

echo "Installation complete."
