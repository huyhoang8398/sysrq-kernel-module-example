#!/bin/bash

rm /dev/lkm_a_key
make clean
make all

MAJOR=$(make test | awk '{print $NF}' | tail -1)

mknod /dev/lkm_a_key c $MAJOR 0