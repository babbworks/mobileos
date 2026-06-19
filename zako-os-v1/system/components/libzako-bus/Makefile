# libzako-bus — Makefile
#
# Builds the ZAKO system bus library.
# Unix domain socket IPC, poll-based event loop, frame routing.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong -D_POSIX_C_SOURCE=200809L

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror \
               -g -O0 -fsanitize=address,undefined -D_POSIX_C_SOURCE=200809L

LIB := libzako-bus.a
TEST_BIN := test_bus

INCLUDES := -I.

.PHONY: all clean test

all: $(LIB)

$(LIB): zako_bus.o
	$(AR) rcs $@ $^

zako_bus.o: zako_bus.c zako_bus.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_bus.c zako_bus.c zako_bus.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_bus.c zako_bus.c

clean:
	rm -f *.o $(LIB) $(TEST_BIN) /tmp/zako_bus_test.sock
