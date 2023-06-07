# Makefile for rocksdb
# https://github.com/facebook/rocksdb/blob/main/INSTALL.md
# https://github.com/facebook/rocksdb/blob/main/examples/Makefile

APP = create_db

# path to rocksdb
ROOT_PATH = ../deps/rocksdb

C_SRCS = $(wildcard ./*.c)
H_SRCS = $(wildcard ./*.h)
OBJS = $(C_SRCS:.c=.o)

INCLUDE += $(ROOT_PATH)/include
LDLIBS += $(ROOT_PATH)/librocksdb.a -lpthread -lrt -ldl -lz  -ldl

all: $(APP)

$(APP): librocksdb  $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -I $(INCLUDE)

.PHONY: librocksdb clean

librocksdb:
	$(MAKE) -j $(shell nproc) -C $(ROOT_PATH) static_lib

clean:
	rm -f $(OBJS) $(APP)
