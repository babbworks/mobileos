# libzako-padsurl — Makefile
#
# Builds the pads-v1 URL codec for ZAKO OS.
# Encodes/decodes BitLedger records as compact URL strings for SMS.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror \
               -g -O0 -fsanitize=address,undefined

LIB := libzako-padsurl.a
TEST_BIN := test_padsurl

INCLUDES := -I.

.PHONY: all clean test

all: $(LIB)

$(LIB): zako_padsurl.o
	$(AR) rcs $@ $^

zako_padsurl.o: zako_padsurl.c zako_padsurl.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_padsurl.c zako_padsurl.c zako_padsurl.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_padsurl.c zako_padsurl.c

clean:
	rm -f *.o $(LIB) $(TEST_BIN)
