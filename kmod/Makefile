
obj-m += kmod_ipi.o
ccflags-y := -O2

PWD := $(CURDIR)


all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

test_kmod: test_kmod.o trap.o
	$(CC) $(CFLAGS) -o $@ $^ -pthread -O2 -Wall -Wextra

trap.o: trap.S
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	find ./ -iname *.o -delete
