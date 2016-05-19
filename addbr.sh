#!/bin/sh
tag=$1
br=$2
eth=$3
ip=$4
mask=$5

brctl addbr $br
ifconfig $br $ip netmask $mask up
vconfig add $eth $tag
brctl addif $br $eth.$tag
ifconfig $eth.$tag up
