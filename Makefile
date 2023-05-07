
.PHONY: all dpdk dpdk-clean clean

all:
	make -C ./src

clean:
	make -C ./src clean

dpdk:
	./scripts/install_dpdk.sh

dpdk-clean:
	sudo rm -rf ./dpdk/build/

