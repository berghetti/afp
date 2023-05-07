#!/bin/bash

cd ./dpdk/
meson build
ninja -C build
sudo ninja -C build install
sudo ldconfig
cd ..

