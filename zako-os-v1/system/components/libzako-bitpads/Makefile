# libzako-bitpads — Makefile
#
# Builds the BitPads v2.0 codec library for ZAKO OS.
# Sub-libraries:
#   libzako-bitpads-core.a    — Meta Byte 1, Wave mode, Pure Signal
#   libzako-bitpads-session.a — Layer 1, CRC-15, component blocks (future)
#
# Targets: static libraries, unit tests, fuzz harnesses.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror \
               -g -O0 -fsanitize=address,undefined

# Output
LIB_CORE := libzako-bitpads-core.a
LIB_SESSION := libzako-bitpads-session.a
TEST_CORE := test_bitpads_core
TEST_SESSION := test_bitpads_session

# Include paths
INCLUDES := -I.

.PHONY: all clean test test-core test-session

all: $(LIB_CORE) $(LIB_SESSION)

# ---- Core library ----
$(LIB_CORE): zako_bitpads_core.o
	$(AR) rcs $@ $^

zako_bitpads_core.o: zako_bitpads_core.c zako_bitpads_core.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ---- Session library ----
$(LIB_SESSION): zako_bitpads_session.o zako_bitpads_core.o
	$(AR) rcs $@ $^

zako_bitpads_session.o: zako_bitpads_session.c zako_bitpads_session.h zako_bitpads_core.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ---- Tests ----
test: test-core test-session

test-core: $(TEST_CORE)
	./$(TEST_CORE)

test-session: $(TEST_SESSION)
	./$(TEST_SESSION)

$(TEST_CORE): test_bitpads_core.c zako_bitpads_core.c zako_bitpads_core.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_bitpads_core.c zako_bitpads_core.c

$(TEST_SESSION): test_bitpads_session.c zako_bitpads_session.c zako_bitpads_core.c zako_bitpads_session.h zako_bitpads_core.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_bitpads_session.c zako_bitpads_session.c zako_bitpads_core.c

# ---- Clean ----
clean:
	rm -f *.o $(LIB_CORE) $(LIB_SESSION) $(TEST_CORE) $(TEST_SESSION)
