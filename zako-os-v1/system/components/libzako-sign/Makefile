# libzako-sign — Makefile
#
# Builds the ed25519 signing library for ZAKO OS.
# Uses TweetNaCl as the cryptographic backend.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -fstack-protector-strong
CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -g -O0 -fsanitize=address,undefined
INCLUDES := -I. -Ivendor

LIB := libzako-sign.a
TEST_BIN := test_zako_sign

ALL_SRC := zako_sign.c vendor/tweetnacl.c

.PHONY: all clean test

all: $(LIB)

$(LIB): zako_sign.o vendor/tweetnacl.o
	$(AR) rcs $@ $^

zako_sign.o: zako_sign.c zako_sign.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

vendor/tweetnacl.o: vendor/tweetnacl.c
	$(CC) -std=c99 -Wall -O2 $(INCLUDES) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_zako_sign.c $(ALL_SRC)
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_zako_sign.c $(ALL_SRC)

clean:
	rm -f *.o vendor/*.o $(LIB) $(TEST_BIN)
