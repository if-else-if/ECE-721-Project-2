#!/bin/bash

clear
make clean && make clobber && make all
cd val1
make cleangdb SIM_FLAGS_EXTRA='--disambig=0,1,0 --perf=0,0,0,0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=4 --dw=4 --iw=8 --rw=4 -e10000000'