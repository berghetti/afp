#!/bin/bash

#path to dpdk
ROOT_PATH=$(dirname $0)"/../deps/dpdk"

pushd $ROOT_PATH
meson build
ninja -C build
sudo ninja -C build install
sudo ldconfig
popd
