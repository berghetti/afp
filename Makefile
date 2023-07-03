

#rocksdb database
DB_PATH=/tmp/my_db

.PHONY: *

all:
	$(MAKE) -C ./src

clean:
	$(MAKE) -C ./src clean

database:
	$(MAKE) -C ./database
	rm -rf $(DB_PATH)
	./database/create_db

database-clean:
	$(MAKE) -C ./database clean

submodules: dpdk rocksdb

submodules-clean: rocksdb-clean

dpdk:
	./scripts/install_dpdk.sh

dpdk-clean:

rocksdb:
	$(MAKE) -j $(shell nproc) -C ./deps/rocksdb static_lib

rocksdb-clean:
	$(MAKE) -C ./deps/rocksdb clean
