# libzako-bitledger — Makefile
#
# Builds the BitLedger v3.0 codec library for ZAKO OS.
# Layer 2 (batch header), Layer 3 (40-bit transaction record),
# control records, conservation invariant.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror \
               -g -O0 -fsanitize=address,undefined

LIB := libzako-bitledger.a
TEST_BIN := test_bitledger

INCLUDES := -I.

.PHONY: all clean test

all: $(LIB)

$(LIB): zako_bitledger.o
	$(AR) rcs $@ $^

zako_bitledger.o: zako_bitledger.c zako_bitledger.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_bitledger.c zako_bitledger.c zako_bitledger.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_bitledger.c zako_bitledger.c

clean:
	rm -f *.o $(LIB) $(TEST_BIN)
