#!/bin/bash

#gcc -pthread -o victim victim.c
#gcc -pthread -o spy spy.c
rm -rf ~/.synfile
./victim &
./spy
