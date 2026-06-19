# telux-sharedb — Makefile
#
# Builds the channel abstraction daemon for ZAKO OS.
# Queue management, carrier selection, transport dispatch.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror \
               -g -O0 -fsanitize=address,undefined

LIB := libtelux-sharedb.a
TEST_BIN := test_sharedb

INCLUDES := -I.

.PHONY: all clean test

all: $(LIB)

$(LIB): sharedb.o
	$(AR) rcs $@ $^

sharedb.o: sharedb.c sharedb.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_sharedb.c sharedb.c sharedb.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_sharedb.c sharedb.c

clean:
	rm -f *.o $(LIB) $(TEST_BIN)
