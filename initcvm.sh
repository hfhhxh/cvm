#!/bin/sh
brctl addbr br0
ifaces=`ifconfig -s -a | sed '1d' | sed '/lo/d' | sed '/br0/d' |awk '{print $1}'`
for iface in $ifaces
do
  #echo $iface
  brctl addif br0 $iface
  ifconfig $iface 0.0.0.0
done
/etc/init.d/networking restart

ln -s /usr/bin/qemu-system-x86_64 /usr/bin/qemu-kvm
#ln -s /usr/libexec/qemu-kvm /usr/bin/qemu-kvm

#mkdir -p /var/cos/cvm
#mkdir -p /var/www/cos/cvm
mkdir -p /var/cos/cvm/iso
mkdir -p /var/cos/cvm/sys
mkdir -p /var/cos/cvm/vm
mkdir -p /var/cos/cvm/vm/cl
mkdir -p /var/cos/cvm/vm/qm
mkdir -p /var/cos/cvm/log

chmod +x /var/cos/cvm/cvm
chmod u+s /var/cos/cvm/cvm
chmod g+s /var/cos/cvm/cvm
chmod +x /var/cos/cvm/exec
chmod +x /var/cos/cvm/qemu-ifdown
chmod +x /var/cos/cvm/qemu-ifup
chmod +x /var/cos/cvm/startcvm.sh
chmod +x /var/cos/cvm/startexec.sh
chmod +x /var/cos/cvm/startonline.sh
chmod +x /var/cos/cvm/autostart.php
chmod +x /var/cos/cvm/online.php
chmod +x /var/cos/cvm/online
chmod +x /var/cos/cvm/qmp-shell
chmod +x /var/cos/cvm/qmp.py

/var/cos/cvm/startcvm.sh &
