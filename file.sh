#!/bin/sh
if [ -e "/var/cos/cvm/vm/$1-$2" ]; then :; else cp /var/cos/cvm/diskk/$2 /var/cos/cvm/vm/$1-$2; fi
