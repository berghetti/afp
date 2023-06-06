#!/bin/bash

pushd dpdk/
meson build
ninja -C build
sudo ninja -C build install
sudo ldconfig
popd
