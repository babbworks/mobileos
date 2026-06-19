/*
 * test_padsurl.c — Unit tests for libzako-padsurl
 *
 * Tests base64url codec, record encode/decode round-trip,
 * optional fields (note, signature), size limits, error handling.
 */

#include "zako_padsurl.h"
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

/* ======================================================================== */

static void test_base64url_basic(void)
{
    uint8_t data[] = { 0x48, 0x65, 0x6C, 0x6C, 0x6F }; /* "Hello" */
    char encoded[32];
    uint8_t decoded[32];
    size_t enc_len, dec_len;
    int rc;

    printf("test_base64url_basic:\n");

    rc = zpu_base64url_encode(data, 5, encoded, sizeof(encoded), &enc_len);
    ASSERT(rc == ZPU_OK, "encode OK");
    ASSERT(enc_len > 0, "non-empty output");

    /* Decode back */
    rc = zpu_base64url_decode(encoded, enc_len, decoded, sizeof(decoded), &dec_len);
    ASSERT(rc == ZPU_OK, "decode OK");
    ASSERT(dec_len == 5, "decoded len = 5");
    ASSERT(memcmp(decoded, data, 5) == 0, "round-trip matches");
}

static void test_base64url_empty(void)
{
    char encoded[8];
    size_t enc_len;
    int rc;

    printf("test_base64url_empty:\n");

    uint8_t empty = 0;
    rc = zpu_base64url_encode(&empty, 0, encoded, sizeof(encoded), &enc_len);
    ASSERT(rc == ZPU_OK, "empty encode OK");
    ASSERT(enc_len == 0, "empty produces empty string");
}

static void test_base64url_all_bytes(void)
{
    uint8_t data[256];
    char encoded[512];
    uint8_t decoded[256];
    size_t enc_len, dec_len;
    size_t i;

    printf("test_base64url_all_bytes:\n");

    for (i = 0; i < 256; i++) { data[i] = (uint8_t)i; }

    int rc = zpu_base64url_encode(data, 256, encoded, sizeof(encoded), &enc_len);
    ASSERT(rc == ZPU_OK, "256-byte encode OK");

    rc = zpu_base64url_decode(encoded, enc_len, decoded, sizeof(decoded), &dec_len);
    ASSERT(rc == ZPU_OK, "256-byte decode OK");
    ASSERT(dec_len == 256, "decoded len = 256");
    ASSERT(memcmp(decoded, data, 256) == 0, "all bytes round-trip");
}

static void test_base64url_padding_sizes(void)
{
    uint8_t data[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
    char enc[16];
    uint8_t dec[8];
    size_t elen, dlen;

    printf("test_base64url_padding_sizes:\n");

    /* 1 byte (remainder=1 → 2 b64 chars) */
    zpu_base64url_encode(data, 1, enc, sizeof(enc), &elen);
    ASSERT(elen == 2, "1 byte → 2 chars");
    zpu_base64url_decode(enc, elen, dec, sizeof(dec), &dlen);
    ASSERT(dlen == 1 && dec[0] == 0xAA, "1-byte round-trip");

    /* 2 bytes (remainder=2 → 3 b64 chars) */
    zpu_base64url_encode(data, 2, enc, sizeof(enc), &elen);
    ASSERT(elen == 3, "2 bytes → 3 chars");
    zpu_base64url_decode(enc, elen, dec, sizeof(dec), &dlen);
    ASSERT(dlen == 2 && dec[0] == 0xAA && dec[1] == 0xBB, "2-byte round-trip");

    /* 3 bytes (exact triple → 4 b64 chars) */
    zpu_base64url_encode(data, 3, enc, sizeof(enc), &elen);
    ASSERT(elen == 4, "3 bytes → 4 chars");
    zpu_base64url_decode(enc, elen, dec, sizeof(dec), &dlen);
    ASSERT(dlen == 3, "3-byte round-trip");
}

static void test_encode_basic_record(void)
{
    zpu_record_t rec;
    char url[ZPU_URL_MAX];
    size_t written;
    int rc;

    printf("test_encode_basic_record:\n");

    memset(&rec, 0, sizeof(rec));
    rec.value_n = 453;          /* $4.53 */
    rec.direction = 0;          /* Plus/In */
    rec.status = 0;             /* Paid */
    rec.debit_credit = 0;       /* Credit */
    rec.rounding = 0;           /* Exact */
    rec.round_dir = 0;
    rec.account_pair = 0x04;    /* Op Income / Asset */
    rec.timestamp = 1718400000; /* 2024-06-15 */
    rec.sender_id = 42;
    rec.has_note = 0;
    rec.has_sig = 0;

    rc = zpu_encode(&rec, url, sizeof(url), &written);
    ASSERT(rc == ZPU_OK, "encode OK");
    ASSERT(written > ZPU_PREFIX_LEN, "URL has content after prefix");
    ASSERT(written < 300, "URL under 300 chars");
    ASSERT(memcmp(url, "#1pa/1", 6) == 0, "starts with prefix");
    ASSERT(zpu_is_padsurl(url) == 1, "detected as padsurl");

    printf("  URL: %s (len=%zu)\n", url, written);
}

static void test_roundtrip_basic(void)
{
    zpu_record_t rec, decoded;
    char url[ZPU_URL_MAX];
    size_t written;
    int rc;

    printf("test_roundtrip_basic:\n");

    memset(&rec, 0, sizeof(rec));
    rec.value_n = 9876543;
    rec.direction = 1;
    rec.status = 1;
    rec.debit_credit = 1;
    rec.rounding = 1;
    rec.round_dir = 1;
    rec.account_pair = 0x01;
    rec.timestamp = 1750000000;
    rec.sender_id = 0xDEADBEEF;
    rec.has_note = 0;
    rec.has_sig = 0;

    rc = zpu_encode(&rec, url, sizeof(url), &written);
    ASSERT(rc == ZPU_OK, "encode");

    rc = zpu_decode(url, &decoded);
    ASSERT(rc == ZPU_OK, "decode");
    ASSERT(decoded.value_n == 9876543, "value round-trip");
    ASSERT(decoded.direction == 1, "direction round-trip");
    ASSERT(decoded.status == 1, "status round-trip");
    ASSERT(decoded.debit_credit == 1, "debit_credit round-trip");
    ASSERT(decoded.rounding == 1, "rounding round-trip");
    ASSERT(decoded.round_dir == 1, "round_dir round-trip");
    ASSERT(decoded.account_pair == 0x01, "account_pair round-trip");
    ASSERT(decoded.timestamp == 1750000000, "timestamp round-trip");
    ASSERT(decoded.sender_id == 0xDEADBEEF, "sender_id round-trip");
    ASSERT(decoded.has_note == 0, "no note");
    ASSERT(decoded.has_sig == 0, "no sig");
}

static void test_roundtrip_with_note(void)
{
    zpu_record_t rec, decoded;
    char url[ZPU_URL_MAX];
    size_t written;
    int rc;

    printf("test_roundtrip_with_note:\n");

    memset(&rec, 0, sizeof(rec));
    rec.value_n = 1500;
    rec.direction = 0;
    rec.account_pair = 0x04;
    rec.timestamp = 1718400000;
    rec.sender_id = 7;
    rec.has_note = 1;
    rec.note_len = 11;
    memcpy(rec.note, "field work", 11); /* includes space */

    rc = zpu_encode(&rec, url, sizeof(url), &written);
    ASSERT(rc == ZPU_OK, "encode with note");
    ASSERT(written < 300, "still under 300");

    rc = zpu_decode(url, &decoded);
    ASSERT(rc == ZPU_OK, "decode with note");
    ASSERT(decoded.has_note == 1, "has_note set");
    ASSERT(decoded.note_len == 11, "note_len = 11");
    ASSERT(memcmp(decoded.note, "field work", 11) == 0, "note content matches");
    ASSERT(decoded.value_n == 1500, "value preserved");
}

static void test_roundtrip_with_sig(void)
{
    zpu_record_t rec, decoded;
    char url[ZPU_URL_MAX];
    size_t written;
    int rc;

    printf("test_roundtrip_with_sig:\n");

    memset(&rec, 0, sizeof(rec));
    rec.value_n = 500;
    rec.direction = 1;
    rec.account_pair = 0x00;
    rec.timestamp = 1718500000;
    rec.sender_id = 99;
    rec.has_sig = 1;
    memset(rec.sig, 0xAB, ZPU_SIG_LEN);

    rc = zpu_encode(&rec, url, sizeof(url), &written);
    ASSERT(rc == ZPU_OK, "encode with sig");
    /* 13 base + 64 sig = 77 bytes → ~103 b64 chars + 6 prefix = ~109 */
    ASSERT(written < 120, "sig URL reasonable size");

    rc = zpu_decode(url, &decoded);
    ASSERT(rc == ZPU_OK, "decode with sig");
    ASSERT(decoded.has_sig == 1, "has_sig set");
    ASSERT(memcmp(decoded.sig, rec.sig, ZPU_SIG_LEN) == 0, "sig round-trip");
}

static void test_roundtrip_with_note_and_sig(void)
{
    zpu_record_t rec, decoded;
    char url[ZPU_URL_MAX];
    size_t written;
    int rc;

    printf("test_roundtrip_with_note_and_sig:\n");

    memset(&rec, 0, sizeof(rec));
    rec.value_n = 12345;
    rec.direction = 0;
    rec.status = 0;
    rec.debit_credit = 0;
    rec.account_pair = 0x08;
    rec.timestamp = 1718600000;
    rec.sender_id = 256;
    rec.has_note = 1;
    rec.note_len = 20;
    memcpy(rec.note, "payment for service", 20);
    rec.has_sig = 1;
    memset(rec.sig, 0xCD, ZPU_SIG_LEN);

    rc = zpu_encode(&rec, url, sizeof(url), &written);
    ASSERT(rc == ZPU_OK, "encode note+sig");
    ASSERT(written < 300, "under 300 chars");

    rc = zpu_decode(url, &decoded);
    ASSERT(rc == ZPU_OK, "decode note+sig");
    ASSERT(decoded.value_n == 12345, "value");
    ASSERT(decoded.has_note == 1, "has note");
    ASSERT(decoded.note_len == 20, "note len");
    ASSERT(memcmp(decoded.note, "payment for service", 20) == 0, "note");
    ASSERT(decoded.has_sig == 1, "has sig");
    ASSERT(memcmp(decoded.sig, rec.sig, ZPU_SIG_LEN) == 0, "sig");
}

static void test_max_note(void)
{
    zpu_record_t rec, decoded;
    char url[ZPU_URL_MAX];
    size_t written;
    int rc;

    printf("test_max_note:\n");

    memset(&rec, 0, sizeof(rec));
    rec.value_n = 100;
    rec.account_pair = 0;
    rec.timestamp = 1;
    rec.sender_id = 1;
    rec.has_note = 1;
    rec.note_len = ZPU_NOTE_MAX;
    memset(rec.note, 'A', ZPU_NOTE_MAX);
    rec.has_sig = 0;

    rc = zpu_encode(&rec, url, sizeof(url), &written);
    ASSERT(rc == ZPU_OK, "max note encodes");
    /* 13 + 1 + 127 = 141 bytes → ~188 b64 + 6 prefix = 194 < 300 */
    ASSERT(written < 200, "max note URL under 200");

    rc = zpu_decode(url, &decoded);
    ASSERT(rc == ZPU_OK, "max note decodes");
    ASSERT(decoded.note_len == ZPU_NOTE_MAX, "note len preserved");
}

static void test_max_payload(void)
{
    zpu_record_t rec;
    char url[ZPU_URL_MAX];
    size_t written;
    int rc;

    printf("test_max_payload:\n");

    /* Max: note(127) + sig(64) = 13+1+127+64 = 205 bytes */
    memset(&rec, 0, sizeof(rec));
    rec.value_n = 16777215; /* max 24-bit */
    rec.direction = 1;
    rec.status = 1;
    rec.debit_credit = 1;
    rec.rounding = 1;
    rec.round_dir = 1;
    rec.account_pair = 0x0F;
    rec.timestamp = 0xFFFFFFFF;
    rec.sender_id = 0xFFFFFFFF;
    rec.has_note = 1;
    rec.note_len = ZPU_NOTE_MAX;
    memset(rec.note, 'Z', ZPU_NOTE_MAX);
    rec.has_sig = 1;
    memset(rec.sig, 0xFF, ZPU_SIG_LEN);

    rc = zpu_encode(&rec, url, sizeof(url), &written);
    ASSERT(rc == ZPU_OK, "max payload encodes");
    ASSERT(written < 300, "max payload under 300 chars");
    printf("  Max URL len: %zu\n", written);
}

static void test_is_padsurl(void)
{
    printf("test_is_padsurl:\n");

    ASSERT(zpu_is_padsurl("#1pa/1ABC") == 1, "valid prefix");
    ASSERT(zpu_is_padsurl("#1pa/1") == 0, "too short (exactly prefix)");
    ASSERT(zpu_is_padsurl("http://example.com") == 0, "not padsurl");
    ASSERT(zpu_is_padsurl("") == 0, "empty");
    ASSERT(zpu_is_padsurl(NULL) == 0, "NULL");
    ASSERT(zpu_is_padsurl("#1pa/") == 0, "prefix only, no content");
}

static void test_decode_errors(void)
{
    zpu_record_t rec;

    printf("test_decode_errors:\n");

    ASSERT(zpu_decode(NULL, &rec) == ZPU_ERR_NULL, "NULL url");
    ASSERT(zpu_decode("#1pa/1x", NULL) == ZPU_ERR_NULL, "NULL out");
    ASSERT(zpu_decode("http://x", &rec) == ZPU_ERR_FORMAT, "wrong prefix");
    ASSERT(zpu_decode("#1pa/1", &rec) == ZPU_ERR_FORMAT, "no payload after prefix");
    ASSERT(zpu_decode("#1pa/1!!!", &rec) == ZPU_ERR_DECODE, "invalid b64 chars");
}

static void test_null_encode(void)
{
    char url[ZPU_URL_MAX];
    size_t written;
    zpu_record_t rec;

    printf("test_null_encode:\n");

    memset(&rec, 0, sizeof(rec));
    ASSERT(zpu_encode(NULL, url, sizeof(url), &written) == ZPU_ERR_NULL, "NULL rec");
    ASSERT(zpu_encode(&rec, NULL, 300, &written) == ZPU_ERR_NULL, "NULL url");
    ASSERT(zpu_encode(&rec, url, 300, NULL) == ZPU_ERR_NULL, "NULL written");
}

static void test_different_values(void)
{
    zpu_record_t rec, dec;
    char url[ZPU_URL_MAX];
    size_t written;

    printf("test_different_values:\n");

    memset(&rec, 0, sizeof(rec));
    rec.account_pair = 0;
    rec.timestamp = 1000;
    rec.sender_id = 1;

    /* Zero value */
    rec.value_n = 0;
    zpu_encode(&rec, url, sizeof(url), &written);
    zpu_decode(url, &dec);
    ASSERT(dec.value_n == 0, "zero value round-trip");

    /* Value = 1 */
    rec.value_n = 1;
    zpu_encode(&rec, url, sizeof(url), &written);
    zpu_decode(url, &dec);
    ASSERT(dec.value_n == 1, "value=1 round-trip");

    /* Max 24-bit value */
    rec.value_n = 16777215;
    zpu_encode(&rec, url, sizeof(url), &written);
    zpu_decode(url, &dec);
    ASSERT(dec.value_n == 16777215, "max 24-bit value round-trip");
}

/* ======================================================================== */

int main(void)
{
    printf("=== libzako-padsurl unit tests ===\n\n");

    test_base64url_basic();
    test_base64url_empty();
    test_base64url_all_bytes();
    test_base64url_padding_sizes();
    test_encode_basic_record();
    test_roundtrip_basic();
    test_roundtrip_with_note();
    test_roundtrip_with_sig();
    test_roundtrip_with_note_and_sig();
    test_max_note();
    test_max_payload();
    test_is_padsurl();
    test_decode_errors();
    test_null_encode();
    test_different_values();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
