
obj-m += kmod_ipi.o
obj-m += kmod_hrtimer.o
ccflags-y := -O2

PWD := $(CURDIR)

.PHONY: all tests clean

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

tests: test_kmod_ipi test_kmod_hrtimer

test_kmod_ipi: test_kmod_ipi.o trap.o
	$(CC) $(CFLAGS) -o $@ $^ -pthread -O2 -Wall -Wextra

test_kmod_hrtimer: test_kmod_hrtimer.o trap.o
	$(CC) $(CFLAGS) -o $@ $^ -pthread -O2 -Wall -Wextra

trap.o: trap.S
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	find ./ -iname *.o -delete
