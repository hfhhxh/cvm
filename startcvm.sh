#!/bin/sh
sleep 5
/var/cos/cvm/exec &
sleep 5
/var/cos/cvm/autostart.php &
/var/cos/cvm/online &
