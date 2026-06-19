# exchange-engine — Makefile
#
# Builds the bilateral exchange engine for ZAKO OS.
# SEND/RECEIVE cycle, conservation enforcement, atomic posting.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror \
               -g -O0 -fsanitize=address,undefined

LIB := libexchange-engine.a
TEST_BIN := test_exchange

INCLUDES := -I.

.PHONY: all clean test

all: $(LIB)

$(LIB): exchange_engine.o
	$(AR) rcs $@ $^

exchange_engine.o: exchange_engine.c exchange_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_exchange.c exchange_engine.c exchange_engine.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_exchange.c exchange_engine.c

clean:
	rm -f *.o $(LIB) $(TEST_BIN)
