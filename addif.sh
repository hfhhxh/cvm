#!/bin/sh
id=$1
type=$2
br=$3

#net="/var/cos/cvm/net.sh"
#ifc="/var/cos/cvm/$br"


if [ ${type} -eq 0 ]
then
	brctl delif br0 VM${id}
elif [ ${type} -eq 1 ]
then
	:	
elif [ ${type} -eq 2 ]
then
	brctl delif br0 VM${id}
	brctl addif ${br} VM${id}
fi

