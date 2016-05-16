#!/bin/sh
qemu-system-x86_64 \
-name lvs -smp 2 -m 1024 \
-drive file=/var/ssd/cvm/lvs,if=virtio,media=disk,index=0 \
-vnc :1 \
-net nic,macaddr=CA-C1-00-00-00-01,model=virtio \
-net tap,script=/var/cos/cvm/ovs-ifup,downscript=/var/cos/cvm/ovs-ifdown,ifname=lvs \
-usb -usbdevice tablet \
-enable-kvm &
