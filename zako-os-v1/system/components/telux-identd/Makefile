# telux-identd — Makefile
#
# Builds the ZAKO identity daemon: store layer, daemon shell, tests.
# Links against libzako-sign (TweetNaCl), libzako-did, libzako-bus, SQLite3.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong -D_POSIX_C_SOURCE=200809L

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror \
               -g -O0 -fsanitize=address,undefined -D_POSIX_C_SOURCE=200809L

# Sibling library paths
SIGN_DIR := ../libzako-sign
DID_DIR := ../libzako-did
BUS_DIR := ../libzako-bus

# SQLite (system)
SQLITE_CFLAGS := $(shell pkg-config --cflags sqlite3 2>/dev/null)
SQLITE_LIBS := $(shell pkg-config --libs sqlite3 2>/dev/null || echo "-lsqlite3")

LIB := libtelux-identd-store.a
TEST_BIN := test_identd

INCLUDES := -I. -I$(SIGN_DIR) -I$(SIGN_DIR)/vendor -I$(DID_DIR) -I$(BUS_DIR) $(SQLITE_CFLAGS)
LIBS := $(SQLITE_LIBS)

# Source files from sibling libraries
SIGN_SRCS := $(SIGN_DIR)/zako_sign.c $(SIGN_DIR)/vendor/tweetnacl.c
DID_SRC := $(DID_DIR)/zako_did.c
BUS_SRC := $(BUS_DIR)/zako_bus.c

.PHONY: all clean test

all: $(LIB)

$(LIB): identd_store.o
	$(AR) rcs $@ $^

identd_store.o: identd_store.c identd_store.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_identd.c identd_daemon.c identd_daemon.h \
             identd_store.c identd_store.h \
             $(SIGN_SRCS) $(DID_SRC) $(BUS_SRC)
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c test_identd.c -o test_identd.o
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c identd_daemon.c -o identd_daemon.o
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c identd_store.c -o identd_store.o
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c $(BUS_SRC) -o zako_bus.o
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c $(DID_SRC) -o zako_did.o
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c $(SIGN_DIR)/zako_sign.c -o zako_sign.o
	$(CC) -std=c99 -Wall -O2 -g -fsanitize=address,undefined \
		-I$(SIGN_DIR)/vendor -c $(SIGN_DIR)/vendor/tweetnacl.c -o tweetnacl.o
	$(CC) -fsanitize=address,undefined -o $@ \
		test_identd.o identd_daemon.o identd_store.o \
		zako_bus.o zako_did.o zako_sign.o tweetnacl.o $(LIBS)

clean:
	rm -f *.o $(LIB) $(TEST_BIN) /tmp/zako_identd_test.sock
