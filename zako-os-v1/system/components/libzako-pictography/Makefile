# libzako-pictography — Makefile
#
# Builds the 4-bit symbol codec for ZAKO OS.
# Codebook management, symbol packing, context declaration waves.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror \
               -g -O0 -fsanitize=address,undefined

LIB := libzako-pictography.a
TEST_BIN := test_pictography

INCLUDES := -I.

.PHONY: all clean test

all: $(LIB)

$(LIB): zako_pictography.o
	$(AR) rcs $@ $^

zako_pictography.o: zako_pictography.c zako_pictography.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_pictography.c zako_pictography.c zako_pictography.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_pictography.c zako_pictography.c

clean:
	rm -f *.o $(LIB) $(TEST_BIN)
