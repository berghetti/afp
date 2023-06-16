
obj-m += kmod_ipi.o
ccflags-y := -O2

PWD := $(CURDIR)

CFLAGS += -O2 -Wall -Wextra

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

test_kmod: test_kmod.o trap.o
	$(CC) $(CFLAGS) -o $@ $^ -pthread

trap.o: trap.S
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	find ./ -iname *.o -delete
