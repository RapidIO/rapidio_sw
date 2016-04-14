#!/bin/bash

SOURCE_PATH="/opt/rapidio/rapidio_sw"
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

make clean &>/dev/null; git pull

for host in  "${ALLNODES[@]}"; do
  [ "$host" = 'none' ] && continue;
  tar cf - * .git* | \
  ssh -C root@"$host" "mkdir -p $SOURCE_PATH; pushd $SOURCE_PATH; tar xf -; popd; chown -R root.$GRP $SOURCE_PATH"
done

for host in  "${ALLNODES[@]}"; do
  [ "$host" = 'none' ] && continue;
  ssh root@"$host" "$SCRIPTS_PATH/make_install.sh $SOURCE_PATH $GRP"
done

echo "Installing configuration files..."

FILENAME=$CONFIG_PATH/fmd.conf

# FMD slaves go first
let c=0;
for host in  "${ALLNODES[@]}"; do
  let c=c+1;
  [ $c -eq 1 ] && continue;
  [ "$host" = 'none' ] && continue;
  sed s/NODE_VAR/$host/g install/node-slave.conf | \
    sed s/MEMSZ/$MEMSZ/g | \
    ssh root@"$host" "mkdir -p $CONFIG_PATH; cd $CONFIG_PATH; cat > $FILENAME";
done

HOSTL='';
let c=0;
for host in  "${ALLNODES[@]}"; do
  let c=c+1;
  # We allow none for sake of awk substitution
  HOSTL="$HOSTL -vH$c=$host";
done

# And now the master FMD
awk -vM=$MEMSZ $HOSTL '
	/MEMSZ/{gsub(/MEMSZ/, M);}
	/node1/{gsub(/node1/, H1);}
	/node2/{if(H2 != "") {gsub(/node2/, H2);}}
	/node3/{if(H3 != "") {gsub(/node3/, H3);}}
	/node4/{if(H4 != "") {gsub(/node4/, H4);}}
	{print}' install/node-master.conf | \
    ssh root@"$MASTER" "mkdir -p $CONFIG_PATH; cd $CONFIG_PATH; cat > $FILENAME";

UMDD_CONF=$CONFIG_PATH/umdd.conf
for host in  "${ALLNODES[@]}"; do
  [ "$host" = 'none' ] && continue;
  ssh root@"$host" "mkdir -p $CONFIG_PATH; cp $SCRIPTS_PATH/umdd.conf $UMDD_CONF"
done

exit 1
echo "Installation of configuration files COMPLETED..."

FILES=( rio_start.sh stop_rio.sh all_start.sh stop_all.sh check_all.sh 
	all_down.sh )

for f in "${FILES[@]}"
do
	FILENAME=$SOURCE_PATH/$f

	cp $SCRIPTS_PATH/$f $FILENAME
	sed -i -- 's/node1/'$MASTER'/g' $FILENAME
	sed -i -- 's/node2/'$NODE2'/g' $FILENAME
	sed -i -- 's/node3/'$NODE3'/g' $FILENAME
	sed -i -- 's/node4/'$NODE4'/g' $FILENAME
done

echo "Installion complete."
