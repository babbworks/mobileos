# libzako-c0 — Makefile
#
# Builds the C0 Enhancement Grammar library for ZAKO OS.
# Enhanced C0 byte codec, signal slot presence, lookup tables.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror \
               -g -O0 -fsanitize=address,undefined

LIB := libzako-c0.a
TEST_BIN := test_c0

INCLUDES := -I.

.PHONY: all clean test

all: $(LIB)

$(LIB): zako_c0.o
	$(AR) rcs $@ $^

zako_c0.o: zako_c0.c zako_c0.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_c0.c zako_c0.c zako_c0.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_c0.c zako_c0.c

clean:
	rm -f *.o $(LIB) $(TEST_BIN)
