/*
 * test_bitpads_session.c — Unit tests for libzako-bitpads-session
 *
 * Tests Layer 1 (CRC-15, encode/decode round-trip), Meta Byte 2,
 * Setup Byte, Value Block, Task, Note, Time header codecs.
 */

#include "zako_bitpads_session.h"
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

static void test_crc15_basic(void)
{
    uint16_t crc;
    uint8_t data1[] = { 0x00 };
    uint8_t data2[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    printf("test_crc15_basic:\n");

    /* Zero input gives zero CRC */
    crc = zbp_crc15(data1, 8);
    ASSERT(crc == 0, "all-zero byte gives CRC=0");

    /* Non-zero input gives non-zero CRC */
    crc = zbp_crc15(data2, 49);
    ASSERT(crc != 0, "all-ones 49 bits gives non-zero CRC");
    ASSERT((crc & 0x8000u) == 0, "CRC fits in 15 bits");

    /* NULL returns 0 */
    crc = zbp_crc15(NULL, 10);
    ASSERT(crc == 0, "NULL data returns 0");

    /* Zero bits returns 0 */
    crc = zbp_crc15(data2, 0);
    ASSERT(crc == 0, "0 bits returns 0");
}

static void test_crc15_sensitivity(void)
{
    uint8_t data_a[7] = { 0x80, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78 };
    uint8_t data_b[7];
    uint16_t crc_a, crc_b;

    printf("test_crc15_sensitivity:\n");

    crc_a = zbp_crc15(data_a, 49);

    /* Flip one bit — CRC must change */
    memcpy(data_b, data_a, 7);
    data_b[3] ^= 0x01u;  /* flip last bit of byte 3 */
    crc_b = zbp_crc15(data_b, 49);
    ASSERT(crc_a != crc_b, "single bit flip changes CRC");

    /* Flip a different bit */
    memcpy(data_b, data_a, 7);
    data_b[0] ^= 0x40u;  /* flip bit 2 */
    crc_b = zbp_crc15(data_b, 49);
    ASSERT(crc_a != crc_b, "different bit flip also changes CRC");
}

static void test_layer1_round_trip(void)
{
    uint8_t buf[ZBP_L1_SIZE];
    zbp_layer1_t decoded;
    int rc;

    printf("test_layer1_round_trip:\n");

    /* Encode with known values */
    rc = zbp_layer1_encode(
        ZBP_L1_DOMAIN_FINANCIAL,  /* domain */
        ZBP_L1_PERM_WRITE | ZBP_L1_PERM_DELEGATE,  /* permissions */
        0,                        /* split order: MSB */
        1,                        /* sender split mode */
        0,                        /* no session enhancement */
        0xDEADBEEF,               /* sender ID */
        17,                       /* sub-entity */
        buf
    );
    ASSERT(rc == ZBP_OK, "encode returns OK");

    /* Decode */
    rc = zbp_layer1_decode(buf, &decoded);
    ASSERT(rc == ZBP_OK, "decode returns OK");
    ASSERT(decoded.soh == 1, "SOH is always 1");
    ASSERT(decoded.wire_version == 0, "wire version is 0 (v2.0)");
    ASSERT(decoded.domain == ZBP_L1_DOMAIN_FINANCIAL, "domain round-trips");
    ASSERT(decoded.permissions == (ZBP_L1_PERM_WRITE | ZBP_L1_PERM_DELEGATE),
           "permissions round-trip");
    ASSERT(decoded.split_order == 0, "split order round-trips");
    ASSERT(decoded.sender_split == 1, "sender split round-trips");
    ASSERT(decoded.session_enh == 0, "session enh round-trips");
    ASSERT(decoded.sender_id == 0xDEADBEEF, "sender ID round-trips");
    ASSERT(decoded.sub_entity == 17, "sub-entity round-trips");
    ASSERT(decoded.crc_valid == 1, "CRC validates");
}

static void test_layer1_crc_detects_corruption(void)
{
    uint8_t buf[ZBP_L1_SIZE];
    zbp_layer1_t decoded;
    int rc;

    printf("test_layer1_crc_detects_corruption:\n");

    /* Encode valid */
    zbp_layer1_encode(ZBP_L1_DOMAIN_GENERAL, 0x0F, 1, 3, 1,
                      0x12345678, 31, buf);

    /* Verify it's valid first */
    rc = zbp_layer1_decode(buf, &decoded);
    ASSERT(rc == ZBP_OK && decoded.crc_valid == 1, "clean encode validates");

    /* Corrupt one byte in the sender ID area */
    buf[3] ^= 0x42u;
    rc = zbp_layer1_decode(buf, &decoded);
    ASSERT(rc == ZBP_OK, "decode still succeeds (returns data)");
    ASSERT(decoded.crc_valid == 0, "CRC detects corruption");
}

static void test_layer1_edge_cases(void)
{
    uint8_t buf[ZBP_L1_SIZE];
    zbp_layer1_t decoded;

    printf("test_layer1_edge_cases:\n");

    /* Minimum values */
    zbp_layer1_encode(0, 0, 0, 0, 0, 0, 0, buf);
    zbp_layer1_decode(buf, &decoded);
    ASSERT(decoded.sender_id == 0, "sender_id=0 round-trips");
    ASSERT(decoded.sub_entity == 0, "sub_entity=0 round-trips");
    ASSERT(decoded.crc_valid == 1, "min-values CRC valid");

    /* Maximum values */
    zbp_layer1_encode(3, 0x0F, 1, 3, 1, 0xFFFFFFFF, 31, buf);
    zbp_layer1_decode(buf, &decoded);
    ASSERT(decoded.domain == 3, "max domain");
    ASSERT(decoded.permissions == 0x0F, "max perms");
    ASSERT(decoded.sender_id == 0xFFFFFFFF, "max sender_id");
    ASSERT(decoded.sub_entity == 31, "max sub_entity");
    ASSERT(decoded.crc_valid == 1, "max-values CRC valid");

    /* NULL checks */
    ASSERT(zbp_layer1_decode(NULL, &decoded) == ZBP_ERR_NULL, "NULL data");
    ASSERT(zbp_layer1_decode(buf, NULL) == ZBP_ERR_NULL, "NULL out");
    ASSERT(zbp_layer1_encode(0, 0, 0, 0, 0, 0, 0, NULL) == ZBP_ERR_NULL, "NULL out buf");
}

static void test_meta2_round_trip(void)
{
    zbp_meta2_t m;
    uint8_t byte;
    int rc;

    printf("test_meta2_round_trip:\n");

    byte = zbp_meta2_encode(0x0A, ZBP_TIME_REF_TIER2, 1, 1);
    rc = zbp_meta2_decode(byte, &m);
    ASSERT(rc == ZBP_OK, "meta2 decode OK");
    ASSERT(m.archetype == 0x0A, "archetype round-trips");
    ASSERT(m.time_ref_sel == ZBP_TIME_REF_TIER2, "time_ref round-trips");
    ASSERT(m.setup_present == 1, "setup_present round-trips");
    ASSERT(m.sigslot_present == 1, "sigslot_present round-trips");

    /* All zeros */
    byte = zbp_meta2_encode(0, 0, 0, 0);
    zbp_meta2_decode(byte, &m);
    ASSERT(m.archetype == 0 && m.time_ref_sel == 0 &&
           m.setup_present == 0 && m.sigslot_present == 0,
           "all-zero meta2 round-trips");

    /* NULL check */
    ASSERT(zbp_meta2_decode(0, NULL) == ZBP_ERR_NULL, "meta2 NULL check");
}

static void test_setup_byte(void)
{
    zbp_setup_t s;
    uint8_t byte;

    printf("test_setup_byte:\n");

    /* Default financial: Tier 3, x100 scaling, 2dp, half-up */
    byte = zbp_setup_encode(ZBP_VALUE_TIER3, ZBP_SCALE_X100,
                            ZBP_DECIMAL_2, 0, ZBP_ROUND_HALF_UP);
    zbp_setup_decode(byte, &s);
    ASSERT(s.value_tier == ZBP_VALUE_TIER3, "tier 3");
    ASSERT(s.scaling == ZBP_SCALE_X100, "x100 scaling");
    ASSERT(s.decimal_pos == ZBP_DECIMAL_2, "2 dp");
    ASSERT(s.context_src == 0, "inline context");
    ASSERT(s.rounding == ZBP_ROUND_HALF_UP, "half-up");

    /* All max */
    byte = zbp_setup_encode(3, 3, 3, 1, 1);
    zbp_setup_decode(byte, &s);
    ASSERT(s.value_tier == 3 && s.scaling == 3 && s.decimal_pos == 3 &&
           s.context_src == 1 && s.rounding == 1,
           "all-max setup round-trips");

    ASSERT(zbp_setup_decode(0, NULL) == ZBP_ERR_NULL, "setup NULL check");
}

static void test_value_block(void)
{
    uint8_t buf[4];
    size_t len;
    uint32_t n;
    int rc;

    printf("test_value_block:\n");

    /* Tier 1: encode 200 */
    rc = zbp_value_encode(200, ZBP_VALUE_TIER1, buf, &len);
    ASSERT(rc == ZBP_OK && len == 1, "tier1 encode 200");
    rc = zbp_value_decode(buf, ZBP_VALUE_TIER1, &n);
    ASSERT(rc == ZBP_OK && n == 200, "tier1 decode 200");

    /* Tier 1: overflow */
    rc = zbp_value_encode(256, ZBP_VALUE_TIER1, buf, &len);
    ASSERT(rc == ZBP_ERR_SIZE, "tier1 overflow at 256");

    /* Tier 2: encode 453 ($4.53 example from spec) */
    rc = zbp_value_encode(453, ZBP_VALUE_TIER2, buf, &len);
    ASSERT(rc == ZBP_OK && len == 2, "tier2 encode 453");
    rc = zbp_value_decode(buf, ZBP_VALUE_TIER2, &n);
    ASSERT(rc == ZBP_OK && n == 453, "tier2 decode 453");

    /* Tier 3: encode 9876543 ($98,765.43 example from spec) */
    rc = zbp_value_encode(9876543, ZBP_VALUE_TIER3, buf, &len);
    ASSERT(rc == ZBP_OK && len == 3, "tier3 encode 9876543");
    rc = zbp_value_decode(buf, ZBP_VALUE_TIER3, &n);
    ASSERT(rc == ZBP_OK && n == 9876543, "tier3 decode 9876543");

    /* Tier 3: max value */
    rc = zbp_value_encode(16777215, ZBP_VALUE_TIER3, buf, &len);
    ASSERT(rc == ZBP_OK, "tier3 max encodes");
    zbp_value_decode(buf, ZBP_VALUE_TIER3, &n);
    ASSERT(n == 16777215, "tier3 max round-trips");

    /* Tier 3: overflow */
    rc = zbp_value_encode(16777216, ZBP_VALUE_TIER3, buf, &len);
    ASSERT(rc == ZBP_ERR_SIZE, "tier3 overflow");

    /* Tier 4: encode max uint32 */
    rc = zbp_value_encode(0xFFFFFFFF, ZBP_VALUE_TIER4, buf, &len);
    ASSERT(rc == ZBP_OK && len == 4, "tier4 max encodes");
    zbp_value_decode(buf, ZBP_VALUE_TIER4, &n);
    ASSERT(n == 0xFFFFFFFF, "tier4 max round-trips");

    /* NULL checks */
    ASSERT(zbp_value_encode(0, 0, NULL, &len) == ZBP_ERR_NULL, "encode NULL buf");
    ASSERT(zbp_value_encode(0, 0, buf, NULL) == ZBP_ERR_NULL, "encode NULL len");
    ASSERT(zbp_value_decode(NULL, 0, &n) == ZBP_ERR_NULL, "decode NULL data");
    ASSERT(zbp_value_decode(buf, 0, NULL) == ZBP_ERR_NULL, "decode NULL out");
}

static void test_task_byte(void)
{
    zbp_task_t t;
    uint8_t byte;

    printf("test_task_byte:\n");

    /* Execute, normal priority, no target, no timing */
    byte = zbp_task_encode(ZBP_TASK_EXECUTE, ZBP_TASK_PRI_NORMAL, 0, 0);
    zbp_task_decode(byte, &t);
    ASSERT(t.category == ZBP_TASK_EXECUTE, "execute category");
    ASSERT(t.priority == ZBP_TASK_PRI_NORMAL, "normal priority");
    ASSERT(t.target_spec == 0, "no target");
    ASSERT(t.timing_spec == 0, "no timing");

    /* Delegate, critical, with target and timing */
    byte = zbp_task_encode(ZBP_TASK_DELEGATE, ZBP_TASK_PRI_CRITICAL, 1, 1);
    zbp_task_decode(byte, &t);
    ASSERT(t.category == ZBP_TASK_DELEGATE, "delegate");
    ASSERT(t.priority == ZBP_TASK_PRI_CRITICAL, "critical");
    ASSERT(t.target_spec == 1, "target specified");
    ASSERT(t.timing_spec == 1, "timing specified");

    /* All 16 categories round-trip */
    int all_ok = 1;
    for (uint8_t c = 0; c < 16; c++) {
        byte = zbp_task_encode(c, 0, 0, 0);
        zbp_task_decode(byte, &t);
        if (t.category != c) { all_ok = 0; break; }
    }
    ASSERT(all_ok == 1, "all 16 task categories round-trip");

    ASSERT(zbp_task_decode(0, NULL) == ZBP_ERR_NULL, "task NULL check");
}

static void test_note_header(void)
{
    zbp_note_header_t nh;
    uint8_t byte;

    printf("test_note_header:\n");

    /* UTF-8, default codebook, length=8 */
    byte = zbp_note_header_encode(ZBP_NOTE_UTF8, 0, 8);
    zbp_note_header_decode(byte, &nh);
    ASSERT(nh.encoding == ZBP_NOTE_UTF8, "UTF-8 encoding");
    ASSERT(nh.codebook == 0, "default codebook");
    ASSERT(nh.length_field == 8, "length field = 8");
    ASSERT(nh.body_length == 8, "body length = 8");

    /* Pictography, codebook A, length=0 (next byte is length) */
    byte = zbp_note_header_encode(ZBP_NOTE_PICTOGRAPHY, 1, 0);
    zbp_note_header_decode(byte, &nh);
    ASSERT(nh.encoding == ZBP_NOTE_PICTOGRAPHY, "pictography");
    ASSERT(nh.codebook == 1, "codebook A");
    ASSERT(nh.length_field == 0, "length field 0 = deferred");
    ASSERT(nh.body_length == 0, "body length 0 (caller reads next byte)");

    /* Extended length (0xF = 1111) */
    byte = zbp_note_header_encode(ZBP_NOTE_BINARY, 0, 0x0F);
    zbp_note_header_decode(byte, &nh);
    ASSERT(nh.length_field == 0x0F, "extended length marker");
    ASSERT(nh.body_length == 0, "extended = 0 (caller reads 2 bytes)");

    ASSERT(zbp_note_header_decode(0, NULL) == ZBP_ERR_NULL, "note NULL check");
}

static void test_time_header(void)
{
    zbp_time_header_t th;
    uint8_t byte;

    printf("test_time_header:\n");

    /* 32-bit Unix, seconds, with timezone, no duration */
    byte = zbp_time_header_encode(1, 0, 1, 0);
    zbp_time_header_decode(byte, &th);
    ASSERT(th.format == 1, "format=01 (32-bit Unix)");
    ASSERT(th.resolution == 0, "resolution=seconds");
    ASSERT(th.tz_present == 1, "timezone present");
    ASSERT(th.dur_present == 0, "no duration");

    /* 16-bit offset, milliseconds, no tz, with duration */
    byte = zbp_time_header_encode(0, 1, 0, 1);
    zbp_time_header_decode(byte, &th);
    ASSERT(th.format == 0, "format=00 (16-bit offset)");
    ASSERT(th.resolution == 1, "resolution=milliseconds");
    ASSERT(th.tz_present == 0, "no timezone");
    ASSERT(th.dur_present == 1, "duration present");

    ASSERT(zbp_time_header_decode(0, NULL) == ZBP_ERR_NULL, "time NULL check");
}

static void test_value_tier_bytes(void)
{
    printf("test_value_tier_bytes:\n");

    ASSERT(zbp_value_tier_bytes(ZBP_VALUE_TIER1) == 1, "tier1 = 1 byte");
    ASSERT(zbp_value_tier_bytes(ZBP_VALUE_TIER2) == 2, "tier2 = 2 bytes");
    ASSERT(zbp_value_tier_bytes(ZBP_VALUE_TIER3) == 3, "tier3 = 3 bytes");
    ASSERT(zbp_value_tier_bytes(ZBP_VALUE_TIER4) == 4, "tier4 = 4 bytes");
}

/* ======================================================================== */

int main(void)
{
    printf("=== libzako-bitpads-session unit tests ===\n\n");

    test_crc15_basic();
    test_crc15_sensitivity();
    test_layer1_round_trip();
    test_layer1_crc_detects_corruption();
    test_layer1_edge_cases();
    test_meta2_round_trip();
    test_setup_byte();
    test_value_block();
    test_task_byte();
    test_note_header();
    test_time_header();
    test_value_tier_bytes();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
