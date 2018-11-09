#!/bin/bash

. /etc/rapidio/nodelist.sh
. /etc/rapidio/riosocket_conf.sh

echo 'Operating on nodes: '$REVNODES

for node in $REVNODES
do
        echo 'First test, should pass...'
	temp=$(grep -e 'managed' /etc/NetworkManager/NetworkManager.conf)
	if [ -z $temp ];
	then
		echo "Failed"
	else
		echo "Passed"
	fi

        echo 'Second test, should fail...'
	temp=$(grep -e 'Not managed' /etc/NetworkManager/NetworkManager.conf)
	if [ -z $temp ];
	then
		echo "Failed"
	else
		echo "Passed"
	fi
done

