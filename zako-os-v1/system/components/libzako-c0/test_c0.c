/*
 * test_c0.c — Unit tests for libzako-c0
 *
 * Tests enhanced C0 byte codec, SSP byte, flag matrix,
 * signal slots, lookup tables, and spec examples.
 */

#include "zako_c0.h"
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

static void test_encode_decode_basic(void)
{
    zc0_enhanced_t e;
    uint8_t byte;

    printf("test_encode_decode_basic:\n");

    /* Plain NUL — no flags */
    byte = zc0_encode(ZC0_NUL, 0, 0, 0);
    ASSERT(byte == 0x00, "plain NUL = 0x00");
    zc0_decode(byte, &e);
    ASSERT(e.code == ZC0_NUL, "code = NUL");
    ASSERT(e.priority == 0 && e.ack_req == 0 && e.cont == 0, "no flags");

    /* Plain SOH */
    byte = zc0_encode(ZC0_SOH, 0, 0, 0);
    ASSERT(byte == 0x01, "plain SOH = 0x01");

    /* Priority BEL — spec example 0xA7 */
    /* P=1, A=0, C=1, code=7(BEL) → 1_0_1_00111 = 0xA7 */
    byte = zc0_encode(ZC0_BEL, 1, 0, 1);
    ASSERT(byte == 0xA7, "P+C BEL = 0xA7 (spec example)");
    zc0_decode(byte, &e);
    ASSERT(e.code == ZC0_BEL, "decoded code = BEL");
    ASSERT(e.priority == 1, "P flag set");
    ASSERT(e.ack_req == 0, "A flag clear");
    ASSERT(e.cont == 1, "C flag set");

    /* P+A SOH — spec example 0xC1 */
    byte = zc0_encode(ZC0_SOH, 1, 1, 0);
    ASSERT(byte == 0xC1, "P+A SOH = 0xC1 (spec example)");

    /* A-NUL — spec example 0x40 */
    byte = zc0_encode(ZC0_NUL, 0, 1, 0);
    ASSERT(byte == 0x40, "A-NUL = 0x40 (spec example)");

    /* All flags on ACK */
    byte = zc0_encode(ZC0_ACK, 1, 1, 1);
    ASSERT(byte == 0xE6, "P+A+C ACK = 0xE6");
    zc0_decode(byte, &e);
    ASSERT(e.code == ZC0_ACK, "code ACK");
    ASSERT(e.priority == 1 && e.ack_req == 1 && e.cont == 1, "all flags");
}

static void test_all_codes_round_trip(void)
{
    zc0_enhanced_t e;
    uint8_t byte;
    int all_ok = 1;

    printf("test_all_codes_round_trip:\n");

    /* All 32 codes with no flags */
    for (uint8_t code = 0; code < 32; code++) {
        byte = zc0_encode(code, 0, 0, 0);
        zc0_decode(byte, &e);
        if (e.code != code || e.flags != 0) {
            all_ok = 0;
            fprintf(stderr, "  FAIL at code %u\n", code);
            break;
        }
    }
    ASSERT(all_ok == 1, "all 32 codes round-trip plain");

    /* All 32 codes with all flags */
    all_ok = 1;
    for (uint8_t code = 0; code < 32; code++) {
        byte = zc0_encode(code, 1, 1, 1);
        zc0_decode(byte, &e);
        if (e.code != code || e.priority != 1 || e.ack_req != 1 || e.cont != 1) {
            all_ok = 0;
            break;
        }
    }
    ASSERT(all_ok == 1, "all 32 codes round-trip with all flags");
}

static void test_all_flag_combinations(void)
{
    zc0_enhanced_t e;
    uint8_t byte;
    int all_ok = 1;

    printf("test_all_flag_combinations:\n");

    /* 8 flag combos × 32 codes = 256 byte values (exhaustive) */
    for (unsigned int val = 0; val < 256; val++) {
        byte = (uint8_t)val;
        zc0_decode(byte, &e);
        /* Verify reconstruction */
        uint8_t rebuilt = zc0_encode(e.code, e.priority, e.ack_req, e.cont);
        if (rebuilt != byte) {
            all_ok = 0;
            fprintf(stderr, "  FAIL at 0x%02X: rebuilt=0x%02X\n", val, rebuilt);
            break;
        }
    }
    ASSERT(all_ok == 1, "all 256 bytes round-trip perfectly");
}

static void test_ssp_basic(void)
{
    zc0_ssp_t ssp;
    uint8_t byte;

    printf("test_ssp_basic:\n");

    /* Spec example: 0x8C = 1000 1100 → P1, P5, P6 active */
    zc0_ssp_decode(0x8C, &ssp);
    ASSERT(ssp.p1 == 1, "0x8C: P1 active");
    ASSERT(ssp.p2 == 0, "0x8C: P2 inactive");
    ASSERT(ssp.p3 == 0, "0x8C: P3 inactive");
    ASSERT(ssp.p4 == 0, "0x8C: P4 inactive");
    ASSERT(ssp.p5 == 1, "0x8C: P5 active");
    ASSERT(ssp.p6 == 1, "0x8C: P6 active");
    ASSERT(ssp.p7 == 0, "0x8C: P7 inactive");
    ASSERT(ssp.p8 == 0, "0x8C: P8 inactive");

    /* Encode round-trip */
    byte = zc0_ssp_encode(1, 0, 0, 0, 1, 1, 0, 0);
    ASSERT(byte == 0x8C, "encode P1+P5+P6 = 0x8C");

    /* All active */
    byte = zc0_ssp_encode(1, 1, 1, 1, 1, 1, 1, 1);
    ASSERT(byte == 0xFF, "all active = 0xFF");
    zc0_ssp_decode(byte, &ssp);
    ASSERT(ssp.p1 == 1 && ssp.p2 == 1 && ssp.p3 == 1 && ssp.p4 == 1 &&
           ssp.p5 == 1 && ssp.p6 == 1 && ssp.p7 == 1 && ssp.p8 == 1,
           "0xFF: all slots active");

    /* None active */
    byte = zc0_ssp_encode(0, 0, 0, 0, 0, 0, 0, 0);
    ASSERT(byte == 0x00, "none active = 0x00");

    /* NULL check */
    ASSERT(zc0_ssp_decode(0, NULL) == ZC0_ERR_NULL, "ssp NULL check");
}

static void test_ssp_is_active(void)
{
    printf("test_ssp_is_active:\n");

    /* 0x8C = P1, P5, P6 */
    ASSERT(zc0_ssp_is_active(0x8C, 1) == 1, "P1 active in 0x8C");
    ASSERT(zc0_ssp_is_active(0x8C, 2) == 0, "P2 inactive in 0x8C");
    ASSERT(zc0_ssp_is_active(0x8C, 5) == 1, "P5 active in 0x8C");
    ASSERT(zc0_ssp_is_active(0x8C, 6) == 1, "P6 active in 0x8C");
    ASSERT(zc0_ssp_is_active(0x8C, 8) == 0, "P8 inactive in 0x8C");

    /* Out of range */
    ASSERT(zc0_ssp_is_active(0xFF, 0) == 0, "slot 0 invalid");
    ASSERT(zc0_ssp_is_active(0xFF, 9) == 0, "slot 9 out of SSP range");
}

static void test_code_names(void)
{
    printf("test_code_names:\n");

    ASSERT(strcmp(zc0_code_name(0), "NUL") == 0, "code 0 = NUL");
    ASSERT(strcmp(zc0_code_name(1), "SOH") == 0, "code 1 = SOH");
    ASSERT(strcmp(zc0_code_name(6), "ACK") == 0, "code 6 = ACK");
    ASSERT(strcmp(zc0_code_name(7), "BEL") == 0, "code 7 = BEL");
    ASSERT(strcmp(zc0_code_name(21), "NAK") == 0, "code 21 = NAK");
    ASSERT(strcmp(zc0_code_name(31), "US") == 0, "code 31 = US");
    ASSERT(strcmp(zc0_code_name(32), "UNKNOWN") == 0, "code 32 = UNKNOWN");
}

static void test_code_verdicts(void)
{
    printf("test_code_verdicts:\n");

    /* Core controls */
    ASSERT(zc0_code_verdict(ZC0_SOH) == ZC0_VERDICT_CORE, "SOH is CORE");
    ASSERT(zc0_code_verdict(ZC0_ACK) == ZC0_VERDICT_CORE, "ACK is CORE");
    ASSERT(zc0_code_verdict(ZC0_NAK) == ZC0_VERDICT_CORE, "NAK is CORE");
    ASSERT(zc0_code_verdict(ZC0_ENQ) == ZC0_VERDICT_CORE, "ENQ is CORE");

    /* Conditional */
    ASSERT(zc0_code_verdict(ZC0_SO) == ZC0_VERDICT_CONDITIONAL, "SO is CONDITIONAL");
    ASSERT(zc0_code_verdict(ZC0_SI) == ZC0_VERDICT_CONDITIONAL, "SI is CONDITIONAL");
    ASSERT(zc0_code_verdict(ZC0_ESC) == ZC0_VERDICT_CONDITIONAL, "ESC is CONDITIONAL");

    /* Unconditional (everything else) */
    ASSERT(zc0_code_verdict(ZC0_NUL) == ZC0_VERDICT_UNCONDITIONAL, "NUL is UNCONDITIONAL");
    ASSERT(zc0_code_verdict(ZC0_BEL) == ZC0_VERDICT_UNCONDITIONAL, "BEL is UNCONDITIONAL");
    ASSERT(zc0_code_verdict(ZC0_LF) == ZC0_VERDICT_UNCONDITIONAL, "LF is UNCONDITIONAL");
    ASSERT(zc0_code_verdict(ZC0_RS) == ZC0_VERDICT_UNCONDITIONAL, "RS is UNCONDITIONAL");
}

static void test_slot_info(void)
{
    zc0_slot_info_t info;
    int rc;

    printf("test_slot_info:\n");

    rc = zc0_slot_info(ZC0_SLOT_P1, &info);
    ASSERT(rc == ZC0_OK, "P1 info OK");
    ASSERT(info.slot_id == 1, "P1 id");
    ASSERT(strcmp(info.layer, "Session") == 0, "P1 layer");

    rc = zc0_slot_info(ZC0_SLOT_P5, &info);
    ASSERT(rc == ZC0_OK, "P5 info OK");
    ASSERT(strcmp(info.layer, "Record") == 0, "P5 layer");

    rc = zc0_slot_info(ZC0_SLOT_P9, &info);
    ASSERT(rc == ZC0_OK, "P9 info OK");
    ASSERT(strcmp(info.layer, "Stream") == 0, "P9 layer");

    rc = zc0_slot_info(ZC0_SLOT_P13, &info);
    ASSERT(rc == ZC0_OK, "P13 info OK");
    ASSERT(strcmp(info.layer, "Session") == 0, "P13 layer");

    /* All 13 slots have valid info */
    int all_ok = 1;
    for (uint8_t s = 1; s <= 13; s++) {
        rc = zc0_slot_info(s, &info);
        if (rc != ZC0_OK || info.layer == NULL || info.position == NULL) {
            all_ok = 0;
            break;
        }
    }
    ASSERT(all_ok == 1, "all 13 slots have valid info");

    /* Invalid slots */
    ASSERT(zc0_slot_info(0, &info) == ZC0_ERR_INVALID, "slot 0 invalid");
    ASSERT(zc0_slot_info(14, &info) == ZC0_ERR_INVALID, "slot 14 invalid");
    ASSERT(zc0_slot_info(1, NULL) == ZC0_ERR_NULL, "NULL out");
}

static void test_slot_layers(void)
{
    printf("test_slot_layers:\n");

    ASSERT(strcmp(zc0_slot_layer(1), "Session") == 0, "P1=Session");
    ASSERT(strcmp(zc0_slot_layer(2), "Session") == 0, "P2=Session");
    ASSERT(strcmp(zc0_slot_layer(3), "Batch") == 0, "P3=Batch");
    ASSERT(strcmp(zc0_slot_layer(4), "Batch") == 0, "P4=Batch");
    ASSERT(strcmp(zc0_slot_layer(5), "Record") == 0, "P5=Record");
    ASSERT(strcmp(zc0_slot_layer(6), "Record") == 0, "P6=Record");
    ASSERT(strcmp(zc0_slot_layer(7), "Wave") == 0, "P7=Wave");
    ASSERT(strcmp(zc0_slot_layer(8), "Wave") == 0, "P8=Wave");
    ASSERT(strcmp(zc0_slot_layer(9), "Stream") == 0, "P9=Stream");
    ASSERT(strcmp(zc0_slot_layer(10), "Stream") == 0, "P10=Stream");
    ASSERT(strcmp(zc0_slot_layer(11), "Record") == 0, "P11=Record");
    ASSERT(strcmp(zc0_slot_layer(12), "Batch") == 0, "P12=Batch");
    ASSERT(strcmp(zc0_slot_layer(13), "Session") == 0, "P13=Session");
    ASSERT(strcmp(zc0_slot_layer(0), "UNKNOWN") == 0, "slot 0 = UNKNOWN");
}

static void test_error_handling(void)
{
    printf("test_error_handling:\n");

    ASSERT(zc0_decode(0, NULL) == ZC0_ERR_NULL, "decode NULL");
}

/* ======================================================================== */

int main(void)
{
    printf("=== libzako-c0 unit tests ===\n\n");

    test_encode_decode_basic();
    test_all_codes_round_trip();
    test_all_flag_combinations();
    test_ssp_basic();
    test_ssp_is_active();
    test_code_names();
    test_code_verdicts();
    test_slot_info();
    test_slot_layers();
    test_error_handling();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
