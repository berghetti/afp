#!/bin/bash

#path to dpdk
ROOT_PATH=$(dirname $0)"/../deps/dpdk"

if [ "$1" = "clean" ]; then
  sudo rm -rf $ROOT_PATH/build
  exit 0
fi

git submodule update --init $ROOT_PATH

pushd $ROOT_PATH

git checkout v23.03

meson build
meson configure -Dprefix=$PWD/build build
ninja -C build
sudo ninja -C build install
sudo ldconfig

popd
