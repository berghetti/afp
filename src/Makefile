
APP = helloworld
BUILD_DIR = build

# all source are stored in SRCS-y
C_SRCS = $(wildcard ./*.c)
H_SRCS = $(wildcard ./*.h)
OBJS = $(C_SRCS:.c=.o)
OBJS += trap.o

PKGCONF ?= pkg-config

ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif

#CFLAGS += -O3 -Wall -Wextra -DNDEBUG
CFLAGS += -O3 -ggdb -Wall -Wextra
CFLAGS += -DNDEBUG
CFLAGS += $(shell $(PKGCONF) --cflags libdpdk)
LDLIBS = $(shell $(PKGCONF) --libs --static libdpdk)

all: $(BUILD_DIR)/$(APP)

$(BUILD_DIR)/$(APP): $(OBJS) $(H_SRCS) Makefile | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/%.o: %.c $(H_SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

trap.o: ../kmod/trap.S
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $@

.PHONY: clean
clean:
	@rm -f $(BUILD_DIR)/*
	@rm *.o
