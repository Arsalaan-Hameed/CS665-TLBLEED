#!/bin/bash

gcc -pthread -o victim victim.c
gcc -pthread -o spy spy.c
#/prerun
rm -f /home/arsalaan/.syncfile
rm -f set1.file
dd if=/dev/zero of=set1.file bs=4096 count=1
./victim &
sleep 1
./spy
sleep 1
rm -f set1.file
