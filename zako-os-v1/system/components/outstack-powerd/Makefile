# outstack-powerd — Makefile
#
# Builds the ZAKO power governor: state machine library, daemon shell, tests.
# Links against libzako-bus and libzako-c0 for bus integration.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong -D_POSIX_C_SOURCE=200809L

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror \
               -g -O0 -fsanitize=address,undefined -D_POSIX_C_SOURCE=200809L

# Sibling library paths
BUS_DIR := ../libzako-bus
C0_DIR := ../libzako-c0

LIB := liboutstack-power.a
TEST_SM_BIN := test_outstack
TEST_DAEMON_BIN := test_daemon

INCLUDES := -I. -I$(BUS_DIR) -I$(C0_DIR)

# Sibling sources
BUS_SRC := $(BUS_DIR)/zako_bus.c
C0_SRC := $(C0_DIR)/zako_c0.c

.PHONY: all clean test test-sm test-daemon

all: $(LIB)

$(LIB): outstack_power.o
	$(AR) rcs $@ $^

outstack_power.o: outstack_power.c outstack_power.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Run all tests
test: test-sm test-daemon

# State machine tests (no bus dependency)
test-sm: $(TEST_SM_BIN)
	./$(TEST_SM_BIN)

$(TEST_SM_BIN): test_outstack.c outstack_power.c outstack_power.h
	$(CC) $(CFLAGS_DEBUG) -I. -o $@ test_outstack.c outstack_power.c

# Daemon shell tests (bus + c0 integration)
test-daemon: $(TEST_DAEMON_BIN)
	./$(TEST_DAEMON_BIN)

$(TEST_DAEMON_BIN): test_daemon.c outstack_daemon.c outstack_daemon.h \
                    outstack_power.c outstack_power.h \
                    $(BUS_SRC) $(C0_SRC)
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ \
		test_daemon.c outstack_daemon.c outstack_power.c \
		$(BUS_SRC) $(C0_SRC)

clean:
	rm -f *.o $(LIB) $(TEST_SM_BIN) $(TEST_DAEMON_BIN) /tmp/zako_outstack_test.sock
