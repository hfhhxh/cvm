#!/bin/sh
type=$1
tag=$2
br=$3
eth=$4
ip=$5
mask=$6

net="/var/cos/cvm/net.sh"
ifc="/var/cos/cvm/$br"

ifconfig br0 down
brctl addbr $br
ifconfig $br $ip netmask $mask up
vconfig add $eth $tag
brctl addif $br $eth.$tag
ifconfig $eth.$tag up
ifconfig br0 up

