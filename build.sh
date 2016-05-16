#!/bin/bash
rm cvm
g++ cvm.c -o cvm
chmod u+s cvm
chmod g+s cvm
rm exec
g++ exec.c -o exec
rm online
g++ online.c -o online
#rsync -avzP cvm 172.20.12.1:/var/cos/cvm/
#rsync -avzP exec 172.20.12.1:/var/cos/cvm/
