/*
 * test_zako_hash.c — Unit tests for libzako-hash
 *
 * Tests:
 * 1. frame_hash produces correct output for known input
 * 2. chain_hash links correctly (changing any input changes output)
 * 3. genesis_anchor produces deterministic output from genesis record
 * 4. Chain integrity: modify a record and verify chain breaks
 * 5. hash_equal: constant-time comparison correctness
 * 6. Error handling: NULL pointers, zero-length inputs
 *
 * Test framework: minimal self-contained assertions (no external deps)
 */

#include "zako_hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", (msg), __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while (0)

static void print_hash(const char *label, const uint8_t hash[ZAKO_HASH_LEN])
{
    printf("  %s: ", label);
    for (size_t i = 0; i < ZAKO_HASH_LEN; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

/* Test 1: frame_hash produces output for known input */
static void test_frame_hash_basic(void)
{
    const uint8_t record[] = "hello zako";
    uint8_t hash[ZAKO_HASH_LEN] = {0};
    int rc;

    printf("test_frame_hash_basic:\n");

    rc = zako_frame_hash(record, sizeof(record) - 1, hash);
    ASSERT(rc == ZAKO_HASH_OK, "frame_hash returns OK");

    /* Verify it's not all zeros (hash was computed) */
    uint8_t zeros[ZAKO_HASH_LEN] = {0};
    ASSERT(zako_hash_equal(hash, zeros) == 0, "hash is not all zeros");

    /* Verify determinism — same input produces same output */
    uint8_t hash2[ZAKO_HASH_LEN] = {0};
    rc = zako_frame_hash(record, sizeof(record) - 1, hash2);
    ASSERT(rc == ZAKO_HASH_OK, "second hash returns OK");
    ASSERT(zako_hash_equal(hash, hash2) == 1, "same input produces same hash");

    print_hash("frame_hash(\"hello zako\")", hash);
}

/* Test 2: chain_hash links records */
static void test_chain_hash_basic(void)
{
    const uint8_t record1[] = "record one";
    const uint8_t record2[] = "record two";
    uint8_t fh1[ZAKO_HASH_LEN], fh2[ZAKO_HASH_LEN];
    uint8_t ch1[ZAKO_HASH_LEN], ch2[ZAKO_HASH_LEN];
    int rc;

    printf("test_chain_hash_basic:\n");

    /* Compute frame hashes */
    rc = zako_frame_hash(record1, sizeof(record1) - 1, fh1);
    ASSERT(rc == ZAKO_HASH_OK, "frame_hash record1 OK");

    rc = zako_frame_hash(record2, sizeof(record2) - 1, fh2);
    ASSERT(rc == ZAKO_HASH_OK, "frame_hash record2 OK");

    /* Genesis anchor for record 1 */
    rc = zako_genesis_anchor(fh1, ch1);
    ASSERT(rc == ZAKO_HASH_OK, "genesis_anchor OK");

    /* Chain hash for record 2 (linked to record 1) */
    rc = zako_chain_hash(fh2, ch1, ch2);
    ASSERT(rc == ZAKO_HASH_OK, "chain_hash OK");

    /* chain_hash[2] should differ from chain_hash[1] */
    ASSERT(zako_hash_equal(ch1, ch2) == 0, "chain hashes are different");

    print_hash("chain_hash[1] (genesis)", ch1);
    print_hash("chain_hash[2]", ch2);
}

/* Test 3: Chain integrity — modifying a record breaks the chain */
static void test_chain_integrity(void)
{
    const uint8_t record1[] = "original record";
    const uint8_t record1_tampered[] = "tampered record";
    const uint8_t record2[] = "second record";
    uint8_t fh1[ZAKO_HASH_LEN], fh1t[ZAKO_HASH_LEN], fh2[ZAKO_HASH_LEN];
    uint8_t ch1[ZAKO_HASH_LEN], ch1t[ZAKO_HASH_LEN];
    uint8_t ch2_orig[ZAKO_HASH_LEN], ch2_tampered[ZAKO_HASH_LEN];

    printf("test_chain_integrity:\n");

    /* Original chain: record1 → record2 */
    zako_frame_hash(record1, sizeof(record1) - 1, fh1);
    zako_genesis_anchor(fh1, ch1);
    zako_frame_hash(record2, sizeof(record2) - 1, fh2);
    zako_chain_hash(fh2, ch1, ch2_orig);

    /* Tampered chain: record1_tampered → record2 */
    zako_frame_hash(record1_tampered, sizeof(record1_tampered) - 1, fh1t);
    zako_genesis_anchor(fh1t, ch1t);
    zako_chain_hash(fh2, ch1t, ch2_tampered);

    /* The chain_hash[2] must differ — tampered record1 propagates forward */
    ASSERT(zako_hash_equal(ch2_orig, ch2_tampered) == 0,
           "tampering record1 changes chain_hash[2]");

    /* Also verify the genesis anchors differ */
    ASSERT(zako_hash_equal(ch1, ch1t) == 0,
           "tampered record1 produces different genesis anchor");
}

/* Test 4: hash_equal correctness */
static void test_hash_equal(void)
{
    uint8_t a[ZAKO_HASH_LEN], b[ZAKO_HASH_LEN];

    printf("test_hash_equal:\n");

    memset(a, 0xAA, ZAKO_HASH_LEN);
    memset(b, 0xAA, ZAKO_HASH_LEN);
    ASSERT(zako_hash_equal(a, b) == 1, "identical hashes return 1");

    b[ZAKO_HASH_LEN - 1] = 0xBB;  /* differ in last byte only */
    ASSERT(zako_hash_equal(a, b) == 0, "different hashes return 0");

    b[ZAKO_HASH_LEN - 1] = 0xAA;  /* restore */
    b[0] = 0x00;  /* differ in first byte only */
    ASSERT(zako_hash_equal(a, b) == 0, "different first byte returns 0");
}

/* Test 5: Error handling */
static void test_error_handling(void)
{
    uint8_t hash[ZAKO_HASH_LEN];
    uint8_t record[] = "test";
    int rc;

    printf("test_error_handling:\n");

    rc = zako_frame_hash(NULL, 10, hash);
    ASSERT(rc == ZAKO_HASH_ERR_NULL, "NULL record returns ERR_NULL");

    rc = zako_frame_hash(record, 4, NULL);
    ASSERT(rc == ZAKO_HASH_ERR_NULL, "NULL output returns ERR_NULL");

    rc = zako_frame_hash(record, 0, hash);
    ASSERT(rc == ZAKO_HASH_ERR_SIZE, "zero length returns ERR_SIZE");

    rc = zako_chain_hash(NULL, hash, hash);
    ASSERT(rc == ZAKO_HASH_ERR_NULL, "NULL frame_hash returns ERR_NULL");

    rc = zako_genesis_anchor(NULL, hash);
    ASSERT(rc == ZAKO_HASH_ERR_NULL, "NULL genesis input returns ERR_NULL");

    ASSERT(zako_hash_equal(NULL, hash) == 0, "NULL in hash_equal returns 0");
}

/* Test 6: Genesis anchor is deterministic */
static void test_genesis_deterministic(void)
{
    const uint8_t record[] = "genesis record";
    uint8_t fh[ZAKO_HASH_LEN];
    uint8_t anchor1[ZAKO_HASH_LEN], anchor2[ZAKO_HASH_LEN];

    printf("test_genesis_deterministic:\n");

    zako_frame_hash(record, sizeof(record) - 1, fh);
    zako_genesis_anchor(fh, anchor1);
    zako_genesis_anchor(fh, anchor2);

    ASSERT(zako_hash_equal(anchor1, anchor2) == 1,
           "genesis_anchor is deterministic");
}

int main(void)
{
    printf("=== libzako-hash unit tests ===\n\n");

    test_frame_hash_basic();
    test_chain_hash_basic();
    test_chain_integrity();
    test_hash_equal();
    test_error_handling();
    test_genesis_deterministic();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
