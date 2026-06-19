# libzako-did — Makefile

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -fstack-protector-strong
CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -g -O0 -fsanitize=address,undefined
INCLUDES := -I.

LIB := libzako-did.a
TEST_BIN := test_zako_did

.PHONY: all clean test

all: $(LIB)

$(LIB): zako_did.o
	$(AR) rcs $@ $^

zako_did.o: zako_did.c zako_did.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_zako_did.c zako_did.c
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_zako_did.c zako_did.c

clean:
	rm -f *.o $(LIB) $(TEST_BIN)
