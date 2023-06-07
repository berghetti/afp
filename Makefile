
.PHONY: *

all:
	$(MAKE) -C ./src

clean:
	$(MAKE) -C ./src clean

database:
	$(MAKE) -C ./database

submodules: dpdk rocksdb

submodules-clean: rocksdb-clean

dpdk:
	./deps/install_dpdk.sh

dpdk-clean:

rocksdb:
	$(MAKE) -j $(shell nproc) -C ./deps/rocksdb static_lib

rocksdb-clean:
	$(MAKE) -C ./deps/rocksdb clean
