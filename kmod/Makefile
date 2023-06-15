obj-m += hello.o

PWD := $(CURDIR)

CFLAGS += -O2 -Wall -Wextra

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

test: test.o trap.o
	$(CC) $(CFLAGS) -o $@ $^ -pthread

trap.o: trap.S
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	find ./ -iname *.o -delete
