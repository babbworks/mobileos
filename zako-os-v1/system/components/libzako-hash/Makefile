# libzako-hash — Makefile
#
# Builds the BLAKE3 chain hash library for ZAKO OS.
# Targets: static library, unit tests, fuzz harness.

CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror \
          -O2 -fstack-protector-strong \
          -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512
# Note: BLAKE3 SIMD disabled for portability — uses portable C implementation.
# On x86 host, remove these defines for faster test execution.
# On ARM32 target, these are required (no x86 SIMD available).

CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -g -O0 -fsanitize=address,undefined \
               -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512

# Vendor-specific compile flags (C11 for _Atomic support in BLAKE3 dispatch)
CFLAGS_VENDOR := -std=c11 -Wall -Wextra -O2 -fstack-protector-strong \
                 -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512

# Output
LIB := libzako-hash.a
TEST_BIN := test_zako_hash
FUZZ_BIN := fuzz_zako_hash

# Include paths
INCLUDES := -I. -Ivendor

.PHONY: all clean test fuzz

all: $(LIB)

$(LIB): zako_hash.o vendor/blake3.o vendor/blake3_dispatch.o vendor/blake3_portable.o
	$(AR) rcs $@ $^

zako_hash.o: zako_hash.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

vendor/blake3.o: vendor/blake3.c
	$(CC) $(CFLAGS_VENDOR) $(INCLUDES) -c $< -o $@

vendor/blake3_dispatch.o: vendor/blake3_dispatch.c
	$(CC) $(CFLAGS_VENDOR) $(INCLUDES) -c $< -o $@

vendor/blake3_portable.o: vendor/blake3_portable.c
	$(CC) $(CFLAGS_VENDOR) $(INCLUDES) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_zako_hash.c zako_hash.c vendor/blake3.c vendor/blake3_dispatch.c vendor/blake3_portable.c
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_zako_hash.c zako_hash.c vendor/blake3.c vendor/blake3_dispatch.c vendor/blake3_portable.c

fuzz: $(FUZZ_BIN)
	@echo "Run with: afl-fuzz -i fuzz_inputs/ -o fuzz_findings/ ./$(FUZZ_BIN)"

$(FUZZ_BIN): fuzz_zako_hash.c $(LIB_SRC)
	afl-cc $(CFLAGS) $(INCLUDES) -o $@ $^

clean:
	rm -f *.o vendor/*.o $(LIB) $(TEST_BIN) $(FUZZ_BIN)
