#!/bin/sh
sleep 60
killall exec
killall online
killall onoff
/var/cos/cvm/initnet.sh &
sleep 10
/var/cos/cvm/startexec.sh &
sleep 10
/var/cos/cvm/autostart.php &
/var/cos/cvm/startonoff.sh &
/var/cos/cvm/startonline.sh &

