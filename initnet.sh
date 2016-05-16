#!/bin/sh
ovs-vsctl add-br br0
port="ovs-vsctl add-port br0"
bond="ovs-vsctl add-bond br0 bond"
ports=""
cmd=""
i=0
while true
do
    eth="eth$i"
    cmd="ifconfig -a -s | grep $eth | wc -l"
    r=$(eval $cmd)
    if [ $r -ne 1 ]
    then
        break
    fi
    i=$(( $i+1 ))
    ports="$ports $eth"
done
if [ $i -eq 1 ]
then
    cmd="$port$ports"
else
    cmd="$bond$ports"
fi
eval $cmd
/etc/init.d/networking restart
ifup br0
