#!/bin/bash

if [ $# -ne 1 ]
then
    echo "USAGE: ./script.sh arg"
    echo "arg values:"
    echo "0: Run getTime after compiling"
    echo "1: Run getTime without compile"
    exit -1
fi
if [ $1 -eq 0 ]
then
    gcc -fopenmp -o prerun prerun.c -I ./
    #gcc -g -o latency latency1.c
    gcc -g -o getTime getTime.c
fi
./prerun
./getTime
