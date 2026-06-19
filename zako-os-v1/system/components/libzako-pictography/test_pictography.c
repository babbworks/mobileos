/*
 * test_pictography.c — Unit tests for libzako-pictography
 *
 * Tests symbol packing/unpacking, context declaration waves,
 * codebook lookup, ALERT promotion, and edge cases.
 */

#include "zako_pictography.h"
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

static void test_pack_two_symbols(void)
{
    uint8_t syms[] = { 0x02, 0x04 }; /* CONSERVE + EMERGENCY */
    uint8_t out[4];
    size_t len;
    int rc;

    printf("test_pack_two_symbols:\n");

    rc = zpi_pack(syms, 2, out, &len);
    ASSERT(rc == ZPI_OK, "pack OK");
    ASSERT(len == 1, "2 symbols = 1 byte");
    ASSERT(out[0] == 0x24, "CONSERVE(2) + EMERGENCY(4) = 0x24");
}

static void test_pack_four_symbols(void)
{
    uint8_t syms[] = { 0x0, 0x5, 0xC, 0xF }; /* FULL + GRANT + ACK + ALERT */
    uint8_t out[4];
    size_t len;
    int rc;

    printf("test_pack_four_symbols:\n");

    rc = zpi_pack(syms, 4, out, &len);
    ASSERT(rc == ZPI_OK, "pack OK");
    ASSERT(len == 2, "4 symbols = 2 bytes");
    ASSERT(out[0] == 0x05, "FULL(0)+GRANT(5) = 0x05");
    ASSERT(out[1] == 0xCF, "ACK(C)+ALERT(F) = 0xCF");
}

static void test_pack_odd_count(void)
{
    uint8_t syms[] = { 0xA, 0xB, 0xC }; /* 3 symbols */
    uint8_t out[4];
    size_t len;

    printf("test_pack_odd_count:\n");

    zpi_pack(syms, 3, out, &len);
    ASSERT(len == 2, "3 symbols = 2 bytes");
    ASSERT(out[0] == 0xAB, "first pair");
    ASSERT((out[1] & 0xF0) == 0xC0, "third symbol in high nibble");
    ASSERT((out[1] & 0x0F) == 0x00, "low nibble zero-padded");
}

static void test_pack_max_sequence(void)
{
    uint8_t syms[8] = { 0,1,2,3,4,5,6,7 };
    uint8_t out[4];
    size_t len;

    printf("test_pack_max_sequence:\n");

    int rc = zpi_pack(syms, 8, out, &len);
    ASSERT(rc == ZPI_OK, "8 symbols OK");
    ASSERT(len == 4, "8 symbols = 4 bytes");
    ASSERT(out[0] == 0x01, "0+1");
    ASSERT(out[1] == 0x23, "2+3");
    ASSERT(out[2] == 0x45, "4+5");
    ASSERT(out[3] == 0x67, "6+7");
}

static void test_pack_overflow(void)
{
    uint8_t syms[9] = {0};
    uint8_t out[8];
    size_t len;

    printf("test_pack_overflow:\n");

    int rc = zpi_pack(syms, 9, out, &len);
    ASSERT(rc == ZPI_ERR_SIZE, "9 symbols rejected");

    rc = zpi_pack(syms, 0, out, &len);
    ASSERT(rc == ZPI_ERR_SIZE, "0 symbols rejected");
}

static void test_unpack_roundtrip(void)
{
    uint8_t syms_in[] = { 0x2, 0x4, 0x3, 0xC };
    uint8_t packed[4];
    size_t pack_len;
    zpi_sequence_t seq;

    printf("test_unpack_roundtrip:\n");

    zpi_pack(syms_in, 4, packed, &pack_len);
    int rc = zpi_unpack(packed, pack_len, 4, &seq);
    ASSERT(rc == ZPI_OK, "unpack OK");
    ASSERT(seq.count == 4, "count = 4");
    ASSERT(seq.symbols[0] == 0x2, "sym 0");
    ASSERT(seq.symbols[1] == 0x4, "sym 1");
    ASSERT(seq.symbols[2] == 0x3, "sym 2");
    ASSERT(seq.symbols[3] == 0xC, "sym 3");
    ASSERT(seq.has_alert == 0, "no ALERT in sequence");
}

static void test_unpack_with_alert(void)
{
    uint8_t packed[] = { 0x5F }; /* GRANT(5) + ALERT(F) */
    zpi_sequence_t seq;

    printf("test_unpack_with_alert:\n");

    int rc = zpi_unpack(packed, 1, 2, &seq);
    ASSERT(rc == ZPI_OK, "unpack OK");
    ASSERT(seq.symbols[0] == 0x5, "GRANT");
    ASSERT(seq.symbols[1] == 0xF, "ALERT");
    ASSERT(seq.has_alert == 1, "ALERT detected");
}

static void test_context_encode_decode(void)
{
    uint8_t wave[ZPI_CONTEXT_WAVE_SIZE];
    zpi_context_t ctx;
    int rc;

    printf("test_context_encode_decode:\n");

    /* Core codebook, version 1 */
    rc = zpi_context_encode(ZPI_CB_CORE, 1, wave);
    ASSERT(rc == ZPI_OK, "encode OK");

    rc = zpi_context_decode(wave, &ctx);
    ASSERT(rc == ZPI_OK, "decode OK");
    ASSERT(ctx.codebook_id == ZPI_CB_CORE, "codebook = CORE (0x0F)");
    ASSERT(ctx.version == 1, "version = 1");
    ASSERT(ctx.valid == 1, "checksum valid");

    /* Exchange codebook, version 2 */
    zpi_context_encode(ZPI_CB_EXCHANGE, 2, wave);
    zpi_context_decode(wave, &ctx);
    ASSERT(ctx.codebook_id == ZPI_CB_EXCHANGE, "codebook = EXCHANGE");
    ASSERT(ctx.version == 2, "version = 2");
    ASSERT(ctx.valid == 1, "valid");
}

static void test_context_bad_checksum(void)
{
    uint8_t wave[ZPI_CONTEXT_WAVE_SIZE];
    zpi_context_t ctx;

    printf("test_context_bad_checksum:\n");

    zpi_context_encode(ZPI_CB_WORK, 1, wave);
    /* Corrupt checksum */
    wave[3] ^= 0xFF;

    int rc = zpi_context_decode(wave, &ctx);
    ASSERT(rc == ZPI_ERR_FORMAT, "bad checksum rejected");
    ASSERT(ctx.valid == 0, "valid = 0");
}

static void test_core_codebook_names(void)
{
    printf("test_core_codebook_names:\n");

    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_CORE, 0x0), "FULL") == 0, "core 0=FULL");
    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_CORE, 0x4), "EMERGENCY") == 0, "core 4=EMERGENCY");
    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_CORE, 0x9), "COMMIT") == 0, "core 9=COMMIT");
    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_CORE, 0xF), "ALERT") == 0, "core F=ALERT");
}

static void test_exchange_codebook_names(void)
{
    printf("test_exchange_codebook_names:\n");

    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_EXCHANGE, 0x0), "SEND") == 0, "ex 0=SEND");
    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_EXCHANGE, 0x3), "PAY") == 0, "ex 3=PAY");
    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_EXCHANGE, 0x6), "COMPLETE") == 0, "ex 6=COMPLETE");
    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_EXCHANGE, 0xF), "ALERT") == 0, "ex F=ALERT");
}

static void test_work_codebook_names(void)
{
    printf("test_work_codebook_names:\n");

    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_WORK, 0x0), "ASSIGN") == 0, "work 0=ASSIGN");
    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_WORK, 0x3), "FINISH") == 0, "work 3=FINISH");
    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_WORK, 0xB), "DELIVER") == 0, "work B=DELIVER");
}

static void test_health_codebook_names(void)
{
    printf("test_health_codebook_names:\n");

    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_HEALTH, 0x0), "VITALS") == 0, "health 0=VITALS");
    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_HEALTH, 0x3), "GLUCOSE") == 0, "health 3=GLUCOSE");
    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_HEALTH, 0xF), "ALERT") == 0, "health F=ALERT");
}

static void test_unknown_codebook(void)
{
    printf("test_unknown_codebook:\n");

    ASSERT(strcmp(zpi_symbol_name(0x09, 0x0), "UNKNOWN") == 0, "reserved codebook");
    ASSERT(strcmp(zpi_symbol_name(ZPI_CB_CORE, 0x10), "UNKNOWN") == 0, "invalid symbol");
}

static void test_alert_promotion(void)
{
    printf("test_alert_promotion:\n");

    ASSERT(zpi_is_alert(0xF) == 1, "0xF is ALERT");
    ASSERT(zpi_is_alert(0x0) == 0, "0x0 is not ALERT");
    ASSERT(zpi_is_alert(0xE) == 0, "0xE is not ALERT");

    zpi_sequence_t seq;
    memset(&seq, 0, sizeof(seq));
    seq.count = 2;
    seq.symbols[0] = 0x5;
    seq.symbols[1] = 0xF;
    seq.has_alert = 1;
    ASSERT(zpi_sequence_has_alert(&seq) == 1, "sequence with ALERT");

    seq.symbols[1] = 0xC;
    seq.has_alert = 0;
    ASSERT(zpi_sequence_has_alert(&seq) == 0, "sequence without ALERT");
    ASSERT(zpi_sequence_has_alert(NULL) == 0, "NULL sequence");
}

static void test_null_checks(void)
{
    uint8_t buf[4];
    size_t len;
    zpi_sequence_t seq;
    zpi_context_t ctx;

    printf("test_null_checks:\n");

    ASSERT(zpi_pack(NULL, 2, buf, &len) == ZPI_ERR_NULL, "pack NULL symbols");
    ASSERT(zpi_pack(buf, 2, NULL, &len) == ZPI_ERR_NULL, "pack NULL out");
    ASSERT(zpi_pack(buf, 2, buf, NULL) == ZPI_ERR_NULL, "pack NULL len");
    ASSERT(zpi_unpack(NULL, 1, 2, &seq) == ZPI_ERR_NULL, "unpack NULL bytes");
    ASSERT(zpi_unpack(buf, 1, 2, NULL) == ZPI_ERR_NULL, "unpack NULL out");
    ASSERT(zpi_context_encode(0, 0, NULL) == ZPI_ERR_NULL, "ctx encode NULL");
    ASSERT(zpi_context_decode(NULL, &ctx) == ZPI_ERR_NULL, "ctx decode NULL data");
    ASSERT(zpi_context_decode(buf, NULL) == ZPI_ERR_NULL, "ctx decode NULL out");
}

static void test_all_codebooks_context_wave(void)
{
    uint8_t wave[ZPI_CONTEXT_WAVE_SIZE];
    zpi_context_t ctx;
    uint8_t cb;

    printf("test_all_codebooks_context_wave:\n");

    int all_ok = 1;
    for (cb = 0; cb < 16; cb++) {
        zpi_context_encode(cb, 1, wave);
        int rc = zpi_context_decode(wave, &ctx);
        if (rc != ZPI_OK || ctx.codebook_id != cb || ctx.version != 1) {
            all_ok = 0;
            break;
        }
    }
    ASSERT(all_ok == 1, "all 16 codebook IDs round-trip via context wave");
}

/* ======================================================================== */

int main(void)
{
    printf("=== libzako-pictography unit tests ===\n\n");

    test_pack_two_symbols();
    test_pack_four_symbols();
    test_pack_odd_count();
    test_pack_max_sequence();
    test_pack_overflow();
    test_unpack_roundtrip();
    test_unpack_with_alert();
    test_context_encode_decode();
    test_context_bad_checksum();
    test_core_codebook_names();
    test_exchange_codebook_names();
    test_work_codebook_names();
    test_health_codebook_names();
    test_unknown_codebook();
    test_alert_promotion();
    test_null_checks();
    test_all_codebooks_context_wave();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
