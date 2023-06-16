
CFLAGS += -Wno-unused-variable -Wno-unused-parameter
CFLAGS +=-Wall -Wextra -ggdb -O3

LDFLAGS += -pthread

ROCKS_INCLUDE += ../deps/rocksdb/include
ROCKS_LDLIBS += ../deps/rocksdb/librocksdb.a -lpthread -lrt -ldl -lz  -ldl

PROGS=signal signal_thread signal_core rocksdb tsc_hz ipi_core

INCLUDES = -I $(ROCKS_INCLUDE)
INCLUDES += -I ../kmod

.PHONY: all clean

ALL: $(PROGS)

rocksdb: rocksdb.o
	$(CXX) $^ -o $@ $(ROCKS_LDLIBS) $(CFLAGS)

ipi_core: ipi_core.o trap.o
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

trap.o: ../kmod/trap.S
	$(CC) -c $^ -o $@

%.o: %.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(INCLUDES)

clean:
	@ find . -type f -name '*.o' -delete
	@ find . -type f -executable -delete


