CFLAGS += -Wno-unused-parameter
CFLAGS +=-Wall -Wextra -ggdb -O2

LDFLAGS += -pthread

PROGS= signal signal_thread signal_core ipi_core hrtimer tsc_hz
PROGS+= rocksdb
PROGS+= dune_core

ROCKS_DIR=../deps/rocksdb
ROCKS_INCLUDE += $(ROCKS_DIR)/include
ROCKS_LDLIBS += $(ROCKS_DIR)/librocksdb.a -lpthread -lrt -ldl -lz  -ldl

DUNE_DIR=../deps/dune
DUNE_INCLUDE= $(DUNE_DIR)
DUNE_LDLIBS= -static -L $(DUNE_DIR)/libdune -ldune

KMOD_INCLUDE=../kmod

INCLUDES = -I $(ROCKS_INCLUDE)
INCLUDES += -I $(DUNE_INCLUDE)
INCLUDES += -I $(KMOD_INCLUDE)

.PHONY: all clean

ALL: $(PROGS)

rocksdb: rocksdb.o
	$(CXX) $^ -o $@ $(ROCKS_LDLIBS) $(CFLAGS)

ipi_core: ipi_core.o trap.o
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

hrtimer: hrtimer.o trap.o
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

dune_core: dune_core.o
	$(CC) $^ -o $@ $(CFLAGS) $(DUNE_LDLIBS) $(DUNE_LDLIBS) -pthread

trap.o: ../kmod/trap.S
	$(CC) -c $^ -o $@

%.o: %.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(INCLUDES)

clean:
	@ find . -type f -name '*.o' -delete
	@ find . -type f -executable -delete
