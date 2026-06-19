/*
 * test_zako_sign.c — Unit tests for libzako-sign
 *
 * Tests:
 * 1. Random keypair generation produces valid keys
 * 2. Deterministic keypair from seed is reproducible
 * 3. Sign and verify succeeds for correct key
 * 4. Verify fails for wrong public key
 * 5. Verify fails for tampered message
 * 6. Verify fails for tampered signature
 * 7. Signing is deterministic (same message + key = same signature)
 * 8. seckey_zero actually clears memory
 * 9. Error handling: NULL pointers, zero-length messages
 */

#include "zako_sign.h"
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

/* Test 1: Random keypair generation */
static void test_keypair_random(void)
{
    uint8_t pk[ZAKO_SIGN_PUBKEY_LEN];
    uint8_t sk[ZAKO_SIGN_SECKEY_LEN];
    uint8_t zeros[ZAKO_SIGN_PUBKEY_LEN] = {0};
    int rc;

    printf("test_keypair_random:\n");

    rc = zako_sign_keypair(pk, sk);
    ASSERT(rc == ZAKO_SIGN_OK, "keypair returns OK");
    ASSERT(memcmp(pk, zeros, ZAKO_SIGN_PUBKEY_LEN) != 0, "public key is not zeros");

    /* Generate second keypair — should differ */
    uint8_t pk2[ZAKO_SIGN_PUBKEY_LEN];
    uint8_t sk2[ZAKO_SIGN_SECKEY_LEN];
    rc = zako_sign_keypair(pk2, sk2);
    ASSERT(rc == ZAKO_SIGN_OK, "second keypair OK");
    ASSERT(memcmp(pk, pk2, ZAKO_SIGN_PUBKEY_LEN) != 0, "two random keypairs differ");

    zako_sign_seckey_zero(sk);
    zako_sign_seckey_zero(sk2);
}

/* Test 2: Deterministic keypair from seed */
static void test_keypair_from_seed(void)
{
    /* Known seed — all 0x42 bytes */
    uint8_t seed[ZAKO_SIGN_SEED_LEN];
    memset(seed, 0x42, ZAKO_SIGN_SEED_LEN);

    uint8_t pk1[ZAKO_SIGN_PUBKEY_LEN], sk1[ZAKO_SIGN_SECKEY_LEN];
    uint8_t pk2[ZAKO_SIGN_PUBKEY_LEN], sk2[ZAKO_SIGN_SECKEY_LEN];
    int rc;

    printf("test_keypair_from_seed:\n");

    rc = zako_sign_keypair_from_seed(seed, pk1, sk1);
    ASSERT(rc == ZAKO_SIGN_OK, "seed keypair 1 OK");

    rc = zako_sign_keypair_from_seed(seed, pk2, sk2);
    ASSERT(rc == ZAKO_SIGN_OK, "seed keypair 2 OK");

    ASSERT(memcmp(pk1, pk2, ZAKO_SIGN_PUBKEY_LEN) == 0, "same seed produces same pk");
    ASSERT(memcmp(sk1, sk2, ZAKO_SIGN_SECKEY_LEN) == 0, "same seed produces same sk");

    /* Different seed produces different keys */
    uint8_t seed2[ZAKO_SIGN_SEED_LEN];
    memset(seed2, 0x43, ZAKO_SIGN_SEED_LEN);
    uint8_t pk3[ZAKO_SIGN_PUBKEY_LEN], sk3[ZAKO_SIGN_SECKEY_LEN];
    rc = zako_sign_keypair_from_seed(seed2, pk3, sk3);
    ASSERT(rc == ZAKO_SIGN_OK, "different seed OK");
    ASSERT(memcmp(pk1, pk3, ZAKO_SIGN_PUBKEY_LEN) != 0, "different seed produces different pk");

    zako_sign_seckey_zero(sk1);
    zako_sign_seckey_zero(sk2);
    zako_sign_seckey_zero(sk3);
}

/* Test 3: Sign and verify */
static void test_sign_verify(void)
{
    uint8_t pk[ZAKO_SIGN_PUBKEY_LEN], sk[ZAKO_SIGN_SECKEY_LEN];
    uint8_t sig[ZAKO_SIGN_SIG_LEN];
    const uint8_t msg[] = "This is a ZAKO record";
    int rc;

    printf("test_sign_verify:\n");

    zako_sign_keypair(pk, sk);

    rc = zako_sign(msg, sizeof(msg) - 1, sk, sig);
    ASSERT(rc == ZAKO_SIGN_OK, "sign OK");

    rc = zako_sign_verify(msg, sizeof(msg) - 1, sig, pk);
    ASSERT(rc == ZAKO_SIGN_OK, "verify OK with correct key");

    zako_sign_seckey_zero(sk);
}

/* Test 4: Verify fails with wrong public key */
static void test_verify_wrong_key(void)
{
    uint8_t pk[ZAKO_SIGN_PUBKEY_LEN], sk[ZAKO_SIGN_SECKEY_LEN];
    uint8_t pk_wrong[ZAKO_SIGN_PUBKEY_LEN], sk_wrong[ZAKO_SIGN_SECKEY_LEN];
    uint8_t sig[ZAKO_SIGN_SIG_LEN];
    const uint8_t msg[] = "payment: 500 ZMW";
    int rc;

    printf("test_verify_wrong_key:\n");

    zako_sign_keypair(pk, sk);
    zako_sign_keypair(pk_wrong, sk_wrong);

    zako_sign(msg, sizeof(msg) - 1, sk, sig);

    rc = zako_sign_verify(msg, sizeof(msg) - 1, sig, pk_wrong);
    ASSERT(rc == ZAKO_SIGN_ERR_VERIFY, "verify fails with wrong key");

    zako_sign_seckey_zero(sk);
    zako_sign_seckey_zero(sk_wrong);
}

/* Test 5: Verify fails with tampered message */
static void test_verify_tampered_message(void)
{
    uint8_t pk[ZAKO_SIGN_PUBKEY_LEN], sk[ZAKO_SIGN_SECKEY_LEN];
    uint8_t sig[ZAKO_SIGN_SIG_LEN];
    uint8_t msg[] = "original message";
    int rc;

    printf("test_verify_tampered_message:\n");

    zako_sign_keypair(pk, sk);
    zako_sign(msg, sizeof(msg) - 1, sk, sig);

    /* Tamper with the message */
    msg[0] = 'O';  /* change 'o' to 'O' */

    rc = zako_sign_verify(msg, sizeof(msg) - 1, sig, pk);
    ASSERT(rc == ZAKO_SIGN_ERR_VERIFY, "verify fails with tampered message");

    zako_sign_seckey_zero(sk);
}

/* Test 6: Verify fails with tampered signature */
static void test_verify_tampered_sig(void)
{
    uint8_t pk[ZAKO_SIGN_PUBKEY_LEN], sk[ZAKO_SIGN_SECKEY_LEN];
    uint8_t sig[ZAKO_SIGN_SIG_LEN];
    const uint8_t msg[] = "capability grant";
    int rc;

    printf("test_verify_tampered_sig:\n");

    zako_sign_keypair(pk, sk);
    zako_sign(msg, sizeof(msg) - 1, sk, sig);

    /* Flip one bit in the signature */
    sig[32] ^= 0x01u;

    rc = zako_sign_verify(msg, sizeof(msg) - 1, sig, pk);
    ASSERT(rc == ZAKO_SIGN_ERR_VERIFY, "verify fails with tampered signature");

    zako_sign_seckey_zero(sk);
}

/* Test 7: Signing is deterministic */
static void test_sign_deterministic(void)
{
    uint8_t seed[ZAKO_SIGN_SEED_LEN];
    memset(seed, 0xAA, ZAKO_SIGN_SEED_LEN);

    uint8_t pk[ZAKO_SIGN_PUBKEY_LEN], sk[ZAKO_SIGN_SECKEY_LEN];
    uint8_t sig1[ZAKO_SIGN_SIG_LEN], sig2[ZAKO_SIGN_SIG_LEN];
    const uint8_t msg[] = "deterministic test";

    printf("test_sign_deterministic:\n");

    zako_sign_keypair_from_seed(seed, pk, sk);
    zako_sign(msg, sizeof(msg) - 1, sk, sig1);
    zako_sign(msg, sizeof(msg) - 1, sk, sig2);

    ASSERT(memcmp(sig1, sig2, ZAKO_SIGN_SIG_LEN) == 0,
           "same message + same key = same signature");

    zako_sign_seckey_zero(sk);
}

/* Test 8: seckey_zero clears memory */
static void test_seckey_zero(void)
{
    uint8_t sk[ZAKO_SIGN_SECKEY_LEN];
    uint8_t zeros[ZAKO_SIGN_SECKEY_LEN] = {0};

    printf("test_seckey_zero:\n");

    memset(sk, 0xFF, ZAKO_SIGN_SECKEY_LEN);
    zako_sign_seckey_zero(sk);
    ASSERT(memcmp(sk, zeros, ZAKO_SIGN_SECKEY_LEN) == 0, "seckey is zeroed");
}

/* Test 9: Error handling */
static void test_errors(void)
{
    uint8_t pk[ZAKO_SIGN_PUBKEY_LEN], sk[ZAKO_SIGN_SECKEY_LEN];
    uint8_t sig[ZAKO_SIGN_SIG_LEN];
    uint8_t msg[] = "x";
    int rc;

    printf("test_errors:\n");

    rc = zako_sign_keypair(NULL, sk);
    ASSERT(rc == ZAKO_SIGN_ERR_NULL, "NULL pubkey returns ERR_NULL");

    rc = zako_sign_keypair(pk, NULL);
    ASSERT(rc == ZAKO_SIGN_ERR_NULL, "NULL seckey returns ERR_NULL");

    zako_sign_keypair(pk, sk);

    rc = zako_sign(NULL, 1, sk, sig);
    ASSERT(rc == ZAKO_SIGN_ERR_NULL, "NULL message returns ERR_NULL");

    rc = zako_sign(msg, 0, sk, sig);
    ASSERT(rc == ZAKO_SIGN_ERR_SIZE, "zero-length message returns ERR_SIZE");

    rc = zako_sign_verify(NULL, 1, sig, pk);
    ASSERT(rc == ZAKO_SIGN_ERR_NULL, "NULL verify message returns ERR_NULL");

    rc = zako_sign_keypair_from_seed(NULL, pk, sk);
    ASSERT(rc == ZAKO_SIGN_ERR_NULL, "NULL seed returns ERR_NULL");

    zako_sign_seckey_zero(sk);
}

int main(void)
{
    printf("=== libzako-sign unit tests ===\n\n");

    test_keypair_random();
    test_keypair_from_seed();
    test_sign_verify();
    test_verify_wrong_key();
    test_verify_tampered_message();
    test_verify_tampered_sig();
    test_sign_deterministic();
    test_seckey_zero();
    test_errors();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
