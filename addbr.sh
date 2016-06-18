#!/bin/sh
type=$1
tag=$2
br=$3
eth=$4
ip=$5
mask=$6

net="/var/cos/cvm/initnet.sh"
ifc="/var/cos/cvm/$br.sh"

echo "#!/bin/sh" > $ifc
chmod +x $ifc

#echo "$ifc" >> $net

#ifconfig br0 down
#	echo "ifconfig br0 down" >> $ifc
brctl addbr $br
	echo "brctl addbr $br" >> $ifc
ifconfig $br $ip netmask $mask up
	echo "ifconfig $br $ip netmask $mask up" >> $ifc
vconfig add $eth $tag
	echo "vconfig add $eth $tag" >> $ifc
brctl addif $br $eth.$tag
	echo "brctl addif $br $eth.$tag" >> $ifc
ifconfig $eth.$tag up
	echo "ifconfig $eth.$tag up" >> $ifc
#ifconfig br0 up
#	echo "ifconfig br0 up" >> $ifc

