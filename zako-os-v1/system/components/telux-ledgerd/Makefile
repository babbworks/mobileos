# telux-ledgerd — Makefile
#
# Builds the ZAKO ledger daemon: storage layer, daemon shell, and tests.
# Links against libzako-hash (BLAKE3), libzako-bus, libzako-bitledger, SQLite3.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong -D_POSIX_C_SOURCE=200809L

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror \
               -g -O0 -fsanitize=address,undefined -D_POSIX_C_SOURCE=200809L

# Paths to sibling libraries
HASH_DIR := ../libzako-hash
BUS_DIR := ../libzako-bus
BITLEDGER_DIR := ../libzako-bitledger

# SQLite (system)
SQLITE_CFLAGS := $(shell pkg-config --cflags sqlite3 2>/dev/null)
SQLITE_LIBS := $(shell pkg-config --libs sqlite3 2>/dev/null || echo "-lsqlite3")

LIB := libtelux-ledgerd-store.a
TEST_BIN := test_ledgerd
TEST_DAEMON_BIN := test_daemon

INCLUDES := -I. -I$(HASH_DIR) -I$(HASH_DIR)/vendor -I$(BUS_DIR) -I$(BITLEDGER_DIR) $(SQLITE_CFLAGS)
LIBS := $(SQLITE_LIBS)

# Hash library sources (compiled in for simplicity)
HASH_SRCS := $(HASH_DIR)/zako_hash.c \
             $(HASH_DIR)/vendor/blake3.c \
             $(HASH_DIR)/vendor/blake3_dispatch.c \
             $(HASH_DIR)/vendor/blake3_portable.c
HASH_CFLAGS := -std=c11 -Wall -Wextra -O2 \
               -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512

# Bus library source
BUS_SRC := $(BUS_DIR)/zako_bus.c

# BitLedger library source
BITLEDGER_SRC := $(BITLEDGER_DIR)/zako_bitledger.c

.PHONY: all clean test test-store test-daemon

all: $(LIB)

$(LIB): ledgerd_store.o
	$(AR) rcs $@ $^

ledgerd_store.o: ledgerd_store.c ledgerd_store.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Run all tests
test: test-store test-daemon

# Storage layer tests (existing)
test-store: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_ledgerd.c ledgerd_store.c ledgerd_store.h $(HASH_SRCS)
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c test_ledgerd.c -o test_ledgerd.o
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c ledgerd_store.c -o ledgerd_store_t.o
	$(CC) -std=c99 -Wall -Wextra -g -O0 -fsanitize=address,undefined \
		$(INCLUDES) -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 \
		-c $(HASH_DIR)/zako_hash.c -o zako_hash_t.o
	$(CC) -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined \
		$(INCLUDES) -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 \
		-c $(HASH_DIR)/vendor/blake3.c -o blake3_t.o
	$(CC) -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined \
		$(INCLUDES) -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 \
		-c $(HASH_DIR)/vendor/blake3_dispatch.c -o blake3_dispatch_t.o
	$(CC) -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined \
		$(INCLUDES) -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 \
		-c $(HASH_DIR)/vendor/blake3_portable.c -o blake3_portable_t.o
	$(CC) -fsanitize=address,undefined -o $@ \
		test_ledgerd.o ledgerd_store_t.o zako_hash_t.o \
		blake3_t.o blake3_dispatch_t.o blake3_portable_t.o $(LIBS)

# Daemon shell tests (new)
test-daemon: $(TEST_DAEMON_BIN)
	./$(TEST_DAEMON_BIN)

$(TEST_DAEMON_BIN): test_daemon.c ledgerd_daemon.c ledgerd_daemon.h \
                    ledgerd_store.c ledgerd_store.h \
                    $(BUS_SRC) $(BITLEDGER_SRC) $(HASH_SRCS)
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c test_daemon.c -o test_daemon.o
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c ledgerd_daemon.c -o ledgerd_daemon_t.o
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c ledgerd_store.c -o ledgerd_store_d.o
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c $(BUS_SRC) -o zako_bus_t.o
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c $(BITLEDGER_SRC) -o zako_bitledger_t.o
	$(CC) -std=c99 -Wall -Wextra -g -O0 -fsanitize=address,undefined \
		$(INCLUDES) -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 \
		-c $(HASH_DIR)/zako_hash.c -o zako_hash_d.o
	$(CC) -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined \
		$(INCLUDES) -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 \
		-c $(HASH_DIR)/vendor/blake3.c -o blake3_d.o
	$(CC) -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined \
		$(INCLUDES) -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 \
		-c $(HASH_DIR)/vendor/blake3_dispatch.c -o blake3_dispatch_d.o
	$(CC) -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined \
		$(INCLUDES) -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 \
		-c $(HASH_DIR)/vendor/blake3_portable.c -o blake3_portable_d.o
	$(CC) -fsanitize=address,undefined -o $@ \
		test_daemon.o ledgerd_daemon_t.o ledgerd_store_d.o \
		zako_bus_t.o zako_bitledger_t.o zako_hash_d.o \
		blake3_d.o blake3_dispatch_d.o blake3_portable_d.o $(LIBS)

clean:
	rm -f *.o $(LIB) $(TEST_BIN) $(TEST_DAEMON_BIN) \
		/tmp/test_ledgerd.db* /tmp/zako_ledgerd_test.sock
