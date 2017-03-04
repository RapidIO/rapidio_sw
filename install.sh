#!/bin/bash

# Files required for installation
# Note the names of these file name (different root) are also used by make_install.sh
#
INSTALL_ROOT="/opt/rapidio/.install"
SCRIPTS_PATH="$(pwd)"/install

NODEDATA_FILE="nodeData.txt"
SRC_TAR="rapidio_sw.tar"
TMPL_FILE="config.tmpl"


# Validate input
#
PRINTHELP=0

if [ "$#" -lt 8 ]; then
    echo $'\ninstall.sh requires 8 parameters.\n'
    PRINTHELP=1
else
    SERVER=$1
    MASTER=$2
    ALLNODES=( $2 $3 $4 $5 )
    MEMSZ=$6
    SW_TYPE=$7
    GRP=$8
    REL=$9

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
    echo "install.sh <SERVER> <NODE1> <NODE2> <NODE3> <NODE4> <memsz> <sw> <group> <rel>"
    echo "<SERVER> Name of the node providing the files required by installation"
    echo "<NODE1>  Name of master, enumerating node"
    echo "<NODE2>  Name of slave node connected to Switch Port 2"
    echo "<NODE3>  Name of slave node connected to Switch Port 3"
    echo "<NODE4>  Name of slave node connected to Switch Port 4"
    echo "If any of <NODE2> <NODE3> <NODE4> is \"none\", the node is ignored."
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
ping -c 1 $SERVER > /dev/null
if [ $? -ne 0 ]; then
    echo "    $SERVER not accessible, exiting..."
    exit
else
    echo "    $SERVER accessible."
fi

for host in "${ALLNODES[@]}"
do
    [ "$host" = 'none' ] && continue;
    [ "$host" = "$SERVER" ] && continue;
    ping -c 1 $host > /dev/null
    if [ $? -ne 0 ]; then
        echo "    $host not accessible, exiting..."
        exit
    else
        echo "    $host accessible."
    fi
done


echo "Creating install files for $SERVER..."
# First create the files that would be available on the server
#
ROOT="/tmp/$$"
rm -rf $ROOT;mkdir -p $ROOT

# Create nodeData.txt
#
let c=0;
for host in "${ALLNODES[@]}"; do
    let c=c+1;
    [ "$host" = 'none' ] && continue
    LINE="$host $host node$c"
    if [ $c -eq 1 ] ; then
        echo "master $LINE" >> $ROOT/$NODEDATA_FILE
        MASTER=$host
    else
        echo "slave $LINE" >> $ROOT/$NODEDATA_FILE
    fi
done

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
