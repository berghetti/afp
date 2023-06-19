#!/bin/bash

ROOT_PATH=$(dirname $0)"/../kmod"

# module name
MOD=kmod_hrtimer

pushd $ROOT_PATH
sudo rmmod $MOD; make && sudo insmod $MOD.ko && sudo chmod 666 /dev/$MOD
popd
