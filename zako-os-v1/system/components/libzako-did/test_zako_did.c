/*
 * test_zako_did.c — Unit tests for libzako-did
 */

#include "zako_did.h"
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

/* Test 1: Basic encode produces a valid-looking DID string */
static void test_encode_basic(void)
{
    uint8_t pubkey[ZAKO_DID_PUBKEY_LEN];
    char did[ZAKO_DID_STR_MAX];
    int rc;

    printf("test_encode_basic:\n");

    /* Use a known key (all 0x01 bytes for simplicity) */
    memset(pubkey, 0x01, ZAKO_DID_PUBKEY_LEN);

    rc = zako_did_from_pubkey(pubkey, did, sizeof(did));
    ASSERT(rc == ZAKO_DID_OK, "encode returns OK");

    /* Verify prefix */
    ASSERT(strncmp(did, "did:key:z", 9) == 0, "starts with did:key:z");

    /* Verify reasonable length (should be ~55-58 chars) */
    size_t len = strlen(did);
    ASSERT(len > 50 && len < 64, "DID length is in expected range");

    printf("  DID: %s (len=%zu)\n", did, len);
}

/* Test 2: Round-trip encode → decode recovers the original key */
static void test_roundtrip(void)
{
    uint8_t pubkey[ZAKO_DID_PUBKEY_LEN];
    uint8_t recovered[ZAKO_DID_PUBKEY_LEN];
    char did[ZAKO_DID_STR_MAX];
    int rc;

    printf("test_roundtrip:\n");

    /* Known key pattern */
    for (size_t i = 0; i < ZAKO_DID_PUBKEY_LEN; i++) {
        pubkey[i] = (uint8_t)(i * 7 + 13);  /* arbitrary non-trivial pattern */
    }

    rc = zako_did_from_pubkey(pubkey, did, sizeof(did));
    ASSERT(rc == ZAKO_DID_OK, "encode OK");

    rc = zako_did_to_pubkey(did, recovered);
    ASSERT(rc == ZAKO_DID_OK, "decode OK");

    ASSERT(memcmp(pubkey, recovered, ZAKO_DID_PUBKEY_LEN) == 0,
           "roundtrip recovers original key");
}

/* Test 3: Different keys produce different DIDs */
static void test_different_keys(void)
{
    uint8_t pk1[ZAKO_DID_PUBKEY_LEN], pk2[ZAKO_DID_PUBKEY_LEN];
    char did1[ZAKO_DID_STR_MAX], did2[ZAKO_DID_STR_MAX];

    printf("test_different_keys:\n");

    memset(pk1, 0xAA, ZAKO_DID_PUBKEY_LEN);
    memset(pk2, 0xBB, ZAKO_DID_PUBKEY_LEN);

    zako_did_from_pubkey(pk1, did1, sizeof(did1));
    zako_did_from_pubkey(pk2, did2, sizeof(did2));

    ASSERT(strcmp(did1, did2) != 0, "different keys produce different DIDs");
}

/* Test 4: All-zero key encodes and decodes correctly */
static void test_zero_key(void)
{
    uint8_t pubkey[ZAKO_DID_PUBKEY_LEN] = {0};
    uint8_t recovered[ZAKO_DID_PUBKEY_LEN];
    char did[ZAKO_DID_STR_MAX];
    int rc;

    printf("test_zero_key:\n");

    rc = zako_did_from_pubkey(pubkey, did, sizeof(did));
    ASSERT(rc == ZAKO_DID_OK, "zero key encode OK");

    rc = zako_did_to_pubkey(did, recovered);
    ASSERT(rc == ZAKO_DID_OK, "zero key decode OK");

    ASSERT(memcmp(pubkey, recovered, ZAKO_DID_PUBKEY_LEN) == 0,
           "zero key roundtrip correct");
}

/* Test 5: All-FF key (edge case for base58) */
static void test_max_key(void)
{
    uint8_t pubkey[ZAKO_DID_PUBKEY_LEN];
    uint8_t recovered[ZAKO_DID_PUBKEY_LEN];
    char did[ZAKO_DID_STR_MAX];
    int rc;

    printf("test_max_key:\n");

    memset(pubkey, 0xFF, ZAKO_DID_PUBKEY_LEN);

    rc = zako_did_from_pubkey(pubkey, did, sizeof(did));
    ASSERT(rc == ZAKO_DID_OK, "max key encode OK");

    rc = zako_did_to_pubkey(did, recovered);
    ASSERT(rc == ZAKO_DID_OK, "max key decode OK");

    ASSERT(memcmp(pubkey, recovered, ZAKO_DID_PUBKEY_LEN) == 0,
           "max key roundtrip correct");
}

/* Test 6: is_valid returns correct results */
static void test_is_valid(void)
{
    uint8_t pubkey[ZAKO_DID_PUBKEY_LEN];
    char did[ZAKO_DID_STR_MAX];

    printf("test_is_valid:\n");

    memset(pubkey, 0x42, ZAKO_DID_PUBKEY_LEN);
    zako_did_from_pubkey(pubkey, did, sizeof(did));

    ASSERT(zako_did_is_valid(did) == 1, "valid DID returns 1");
    ASSERT(zako_did_is_valid("did:key:z6Mk") == 0, "too-short DID returns 0");
    ASSERT(zako_did_is_valid("did:web:example.com") == 0, "wrong method returns 0");
    ASSERT(zako_did_is_valid("not a did at all") == 0, "random string returns 0");
    ASSERT(zako_did_is_valid("") == 0, "empty string returns 0");
    ASSERT(zako_did_is_valid(NULL) == 0, "NULL returns 0");
}

/* Test 7: Decode rejects malformed input */
static void test_decode_errors(void)
{
    uint8_t pubkey[ZAKO_DID_PUBKEY_LEN];
    int rc;

    printf("test_decode_errors:\n");

    rc = zako_did_to_pubkey("did:key:z6Mk", pubkey);
    ASSERT(rc != ZAKO_DID_OK, "too-short base58 rejected");

    rc = zako_did_to_pubkey("did:web:z6MkhaXgBZDvotDkL5257", pubkey);
    ASSERT(rc == ZAKO_DID_ERR_FORMAT, "wrong method prefix rejected");

    rc = zako_did_to_pubkey(NULL, pubkey);
    ASSERT(rc == ZAKO_DID_ERR_NULL, "NULL input rejected");

    rc = zako_did_to_pubkey("did:key:z", pubkey);
    ASSERT(rc != ZAKO_DID_OK, "empty base58 rejected");

    /* Invalid base58 character (0, O, I, l are excluded) */
    rc = zako_did_to_pubkey("did:key:z0000000000000000000000000000000000000000000000", pubkey);
    ASSERT(rc == ZAKO_DID_ERR_DECODE, "invalid base58 char '0' rejected");
}

/* Test 8: Buffer too small */
static void test_buffer_small(void)
{
    uint8_t pubkey[ZAKO_DID_PUBKEY_LEN];
    char small_buf[10];
    int rc;

    printf("test_buffer_small:\n");

    memset(pubkey, 0x42, ZAKO_DID_PUBKEY_LEN);

    rc = zako_did_from_pubkey(pubkey, small_buf, sizeof(small_buf));
    ASSERT(rc == ZAKO_DID_ERR_SIZE, "small buffer returns ERR_SIZE");
}

int main(void)
{
    printf("=== libzako-did unit tests ===\n\n");

    test_encode_basic();
    test_roundtrip();
    test_different_keys();
    test_zero_key();
    test_max_key();
    test_is_valid();
    test_decode_errors();
    test_buffer_small();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
