
CFLAGS += -Wno-unused-variable -Wno-unused-parameter
CFLAGS +=-Wall -Wextra -ggdb -O2

LDFLAGS += -pthread

ROCKS_INCLUDE += ../deps/rocksdb/include
ROCKS_LDLIBS += ../deps/rocksdb/librocksdb.a -lpthread -lrt -ldl -lz  -ldl

PROGS=signal signal_thread rocksdb

.PHONY: all clean

ALL: $(PROGS)

rocksdb: rocksdb.o
	$(CXX) $^ -o $@ $(ROCKS_LDLIBS) $(CFLAGS)

rocksdb.o: rocksdb.c
	$(CC) -c $^ -o $@ $(CFLAGS) -I $(ROCKS_INCLUDE)

%.o: %.c
	$(CC) $^ -c -o $@ $(CFLAGS)

clean:
	@ find . -type f -name '*.o' -delete
	@ find . -type f -executable -delete


