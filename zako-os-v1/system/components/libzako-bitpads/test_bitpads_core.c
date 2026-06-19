/*
 * test_bitpads_core.c — Unit tests for libzako-bitpads-core
 *
 * Tests the BitPads v2.0 Meta Byte 1 codec:
 * 1. Wave Role A encode/decode round-trip
 * 2. Wave Role B encode/decode round-trip (all 16 categories)
 * 3. Record mode encode/decode (Role C component flags)
 * 4. Pure Signal encode/decode
 * 5. Frame utility functions
 * 6. Layer 1 requirement table
 * 7. Known byte values from spec examples
 * 8. Error handling (NULL pointers)
 * 9. Exhaustive 256-byte decode (no crashes)
 *
 * Test framework: minimal self-contained assertions (no external deps)
 */

#include "zako_bitpads_core.h"
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

static void test_wave_role_a_basic(void)
{
    zbp_meta1_t m;
    uint8_t byte;
    int rc;

    printf("test_wave_role_a_basic:\n");

    /* Encode: no flags set */
    byte = zbp_meta1_encode_wave_a(0, 0, 0, 0, 0, 0);
    ASSERT(byte == 0x00, "all-zero wave/A encodes to 0x00");

    rc = zbp_meta1_decode(byte, &m);
    ASSERT(rc == ZBP_OK, "decode returns OK");
    ASSERT(m.mode == ZBP_MODE_WAVE, "mode is wave");
    ASSERT(m.treatment == 0, "treatment is Role A");
    ASSERT(m.ack_sysctx == 0, "no ACK");
    ASSERT(m.continuation == 0, "no continuation");
    ASSERT(m.priority == 0, "no priority");
    ASSERT(m.cipher == 0, "no cipher");
    ASSERT(m.ext_flags == 0, "no ext flags");
    ASSERT(m.profile_def == 0, "no profile");

    /* Encode: all flags set */
    byte = zbp_meta1_encode_wave_a(1, 1, 1, 1, 1, 1);
    rc = zbp_meta1_decode(byte, &m);
    ASSERT(rc == ZBP_OK, "decode all-flags OK");
    ASSERT(m.mode == ZBP_MODE_WAVE, "still wave");
    ASSERT(m.treatment == 0, "still Role A (bit4=0)");
    ASSERT(m.ack_sysctx == 1, "ACK set");
    ASSERT(m.continuation == 1, "continuation set");
    ASSERT(m.priority == 1, "priority set");
    ASSERT(m.cipher == 1, "cipher set");
    ASSERT(m.ext_flags == 1, "ext_flags set");
    ASSERT(m.profile_def == 1, "profile set");

    /* Verify the byte value: 0_1_1_0_1_1_1_1 = 0x6F */
    /* bit1=0(wave) bit2=1(ack) bit3=1(cont) bit4=0(roleA) bit5-8=1111 */
    ASSERT(byte == 0x6F, "all-flags wave/A encodes to 0x6F");
}

static void test_wave_role_b_categories(void)
{
    zbp_meta1_t m;
    uint8_t byte;
    int rc;
    uint8_t cat;

    printf("test_wave_role_b_categories:\n");

    /* Test all 16 categories round-trip */
    for (cat = 0; cat < 16; cat++) {
        byte = zbp_meta1_encode_wave_b(0, 0, cat);
        rc = zbp_meta1_decode(byte, &m);
        ASSERT(rc == ZBP_OK, "decode category OK");
        ASSERT(m.mode == ZBP_MODE_WAVE, "mode is wave");
        ASSERT(m.treatment == 1, "treatment is Role B");
        ASSERT(m.category == cat, "category round-trips");
    }

    /* Category with ACK and continuation */
    byte = zbp_meta1_encode_wave_b(1, 1, ZBP_CAT_ALERT);
    rc = zbp_meta1_decode(byte, &m);
    ASSERT(rc == ZBP_OK, "alert+ack+cont decode OK");
    ASSERT(m.ack_sysctx == 1, "ACK set on category wave");
    ASSERT(m.continuation == 1, "continuation set on category wave");
    ASSERT(m.category == ZBP_CAT_ALERT, "category is alert (0100)");
}

static void test_record_mode(void)
{
    zbp_meta1_t m;
    uint8_t byte;
    int rc;

    printf("test_record_mode:\n");

    /* Minimal record: no components */
    byte = zbp_meta1_encode_record(0, 0, 0, 0, 0, 0);
    rc = zbp_meta1_decode(byte, &m);
    ASSERT(rc == ZBP_OK, "minimal record decode OK");
    ASSERT(m.mode == ZBP_MODE_RECORD, "mode is record");
    ASSERT(m.ack_sysctx == 0, "no sysctx");
    ASSERT(m.continuation == 0, "no continuation");
    ASSERT(m.value_present == 0, "no value");
    ASSERT(m.time_present == 0, "no time");
    ASSERT(m.task_present == 0, "no task");
    ASSERT(m.note_present == 0, "no note");
    /* bit1=1 rest=0 → 0x80 */
    ASSERT(byte == 0x80, "minimal record is 0x80");

    /* Full record: all components + sysctx + continuation */
    byte = zbp_meta1_encode_record(1, 1, 1, 1, 1, 1);
    rc = zbp_meta1_decode(byte, &m);
    ASSERT(rc == ZBP_OK, "full record decode OK");
    ASSERT(m.mode == ZBP_MODE_RECORD, "mode record");
    ASSERT(m.ack_sysctx == 1, "sysctx present");
    ASSERT(m.continuation == 1, "continuation");
    ASSERT(m.value_present == 1, "value present");
    ASSERT(m.time_present == 1, "time present");
    ASSERT(m.task_present == 1, "task present");
    ASSERT(m.note_present == 1, "note present");
    /* bit1=1 bit2=1 bit3=1 bit4=0 bit5-8=1111 → 0xEF */
    ASSERT(byte == 0xEF, "full record is 0xEF");

    /* Record with only value + time (common financial record) */
    byte = zbp_meta1_encode_record(0, 0, 1, 1, 0, 0);
    rc = zbp_meta1_decode(byte, &m);
    ASSERT(rc == ZBP_OK, "value+time record OK");
    ASSERT(m.value_present == 1, "value yes");
    ASSERT(m.time_present == 1, "time yes");
    ASSERT(m.task_present == 0, "task no");
    ASSERT(m.note_present == 0, "note no");
    /* bit1=1 bit5=1 bit6=1 → 0x80|0x08|0x04 = 0x8C */
    ASSERT(byte == 0x8C, "value+time record is 0x8C");
}

static void test_pure_signal(void)
{
    zbp_meta1_t m;
    uint8_t byte;
    int rc;

    printf("test_pure_signal:\n");

    /* Encode without ACK */
    rc = zbp_pure_signal_encode(0, &byte);
    ASSERT(rc == ZBP_OK, "encode pure signal OK");
    /* Wave(0) + bit4=1(category) + cat=0000 → 0x10 */
    ASSERT(byte == 0x10, "pure signal no-ack is 0x10");

    /* Decode it back */
    rc = zbp_pure_signal_decode(byte, &m);
    ASSERT(rc == ZBP_OK, "decode pure signal OK");
    ASSERT(m.mode == ZBP_MODE_WAVE, "wave mode");
    ASSERT(m.treatment == 1, "category mode");
    ASSERT(m.category == ZBP_CAT_PURE_SIGNAL, "category is pure signal");
    ASSERT(m.ack_sysctx == 0, "no ack");

    /* Encode with ACK — spec example 0x40 (broadcast discovery) */
    rc = zbp_pure_signal_encode(1, &byte);
    ASSERT(rc == ZBP_OK, "encode pure signal+ack OK");
    /* Wave(0) + ack(bit2=1) + bit4=1 + cat=0000 → 0x50 */
    ASSERT(byte == 0x50, "pure signal ack is 0x50");

    /* Non-pure-signal should fail decode */
    rc = zbp_pure_signal_decode(0x80, NULL);
    ASSERT(rc == ZBP_ERR_INVALID, "record byte fails pure signal decode");

    rc = zbp_pure_signal_decode(0x11, NULL);
    ASSERT(rc == ZBP_ERR_INVALID, "category 0001 fails pure signal decode");

    /* NULL output test */
    rc = zbp_pure_signal_encode(0, NULL);
    ASSERT(rc == ZBP_ERR_NULL, "NULL output returns ERR_NULL");
}

static void test_spec_examples(void)
{
    zbp_meta1_t m;
    int rc;

    printf("test_spec_examples:\n");

    /*
     * Spec §6 example: 0x0F = 0000 1111
     * Wave, Role A (bit4=0), all content flags set
     * Priority=1, Cipher=1, ExtFlags=1, Profile=1
     */
    rc = zbp_meta1_decode(0x0F, &m);
    ASSERT(rc == ZBP_OK, "0x0F decodes OK");
    ASSERT(m.mode == ZBP_MODE_WAVE, "0x0F is wave");
    ASSERT(m.treatment == 0, "0x0F is Role A");
    ASSERT(m.ack_sysctx == 0, "0x0F no ACK");
    ASSERT(m.continuation == 0, "0x0F no cont");
    ASSERT(m.priority == 1, "0x0F priority set");
    ASSERT(m.cipher == 1, "0x0F cipher set");
    ASSERT(m.ext_flags == 1, "0x0F ext_flags set");
    ASSERT(m.profile_def == 1, "0x0F profile set");

    /*
     * Spec §6 example: 0x40 = 0100 0000
     * Wave, bit2=1(ACK), bit4=0(Role A), content=0000
     * "Broadcast Discovery" — ACK requested, all flags zero
     */
    rc = zbp_meta1_decode(0x40, &m);
    ASSERT(rc == ZBP_OK, "0x40 decodes OK");
    ASSERT(m.mode == ZBP_MODE_WAVE, "0x40 is wave");
    ASSERT(m.ack_sysctx == 1, "0x40 ACK requested");
    ASSERT(m.treatment == 0, "0x40 is Role A");
    ASSERT(m.priority == 0, "0x40 no priority");

    /*
     * Category wave: 0x10 = 0001 0000
     * Wave, bit4=1(category), category=0000 (Pure Signal)
     */
    rc = zbp_meta1_decode(0x10, &m);
    ASSERT(rc == ZBP_OK, "0x10 decodes OK");
    ASSERT(m.mode == ZBP_MODE_WAVE, "0x10 is wave");
    ASSERT(m.treatment == 1, "0x10 is Role B");
    ASSERT(m.category == ZBP_CAT_PURE_SIGNAL, "0x10 is pure signal cat");
}

static void test_frame_utilities(void)
{
    printf("test_frame_utilities:\n");

    /* is_wave / is_record */
    ASSERT(zbp_is_wave(0x00) == 1, "0x00 is wave");
    ASSERT(zbp_is_wave(0x7F) == 1, "0x7F is wave");
    ASSERT(zbp_is_wave(0x80) == 0, "0x80 is not wave");
    ASSERT(zbp_is_record(0x80) == 1, "0x80 is record");
    ASSERT(zbp_is_record(0xFF) == 1, "0xFF is record");
    ASSERT(zbp_is_record(0x00) == 0, "0x00 is not record");

    /* Layer 1 requirements */
    ASSERT(zbp_category_l1_requirement(ZBP_CAT_PURE_SIGNAL) == ZBP_L1_NOT_REQUIRED,
           "pure signal: L1 not required");
    ASSERT(zbp_category_l1_requirement(ZBP_CAT_DATA_TRANSFER) == ZBP_L1_REQUIRED,
           "data transfer: L1 required");
    ASSERT(zbp_category_l1_requirement(ZBP_CAT_ALERT) == ZBP_L1_RECOMMENDED,
           "alert: L1 recommended");
    ASSERT(zbp_category_l1_requirement(ZBP_CAT_FINANCIAL) == ZBP_L1_REQUIRED,
           "financial: L1 required");
    ASSERT(zbp_category_l1_requirement(ZBP_CAT_EXTENDED) == ZBP_L1_REQUIRED,
           "extended: L1 required");

    /* Wave min size */
    ASSERT(zbp_wave_min_size(0x10) == 1, "pure signal min = 1 byte");
    ASSERT(zbp_wave_min_size(zbp_meta1_encode_wave_b(0, 0, ZBP_CAT_DATA_TRANSFER)) == 9,
           "data transfer min = 9 (meta+L1)");
    ASSERT(zbp_wave_min_size(zbp_meta1_encode_wave_b(0, 0, ZBP_CAT_EXTENDED)) == 10,
           "extended min = 10 (meta+ext_cat+L1)");

    /* Record min size */
    ASSERT(zbp_record_min_size() == 10, "record min = 10 (M1+M2+L1)");
}

static void test_error_handling(void)
{
    int rc;

    printf("test_error_handling:\n");

    rc = zbp_meta1_decode(0x00, NULL);
    ASSERT(rc == ZBP_ERR_NULL, "NULL out returns ERR_NULL");

    rc = zbp_pure_signal_encode(0, NULL);
    ASSERT(rc == ZBP_ERR_NULL, "pure_signal_encode NULL returns ERR_NULL");
}

static void test_exhaustive_decode(void)
{
    zbp_meta1_t m;
    int rc;
    unsigned int i;
    int all_ok = 1;

    printf("test_exhaustive_decode:\n");

    /* Decode all 256 possible byte values — none should crash */
    for (i = 0; i < 256; i++) {
        rc = zbp_meta1_decode((uint8_t)i, &m);
        if (rc != ZBP_OK) {
            all_ok = 0;
            break;
        }
        /* Verify mode is consistent with bit 1 */
        if (m.mode != (((uint8_t)i & 0x80u) ? 1u : 0u)) {
            all_ok = 0;
            break;
        }
    }
    ASSERT(all_ok == 1, "all 256 bytes decode without error");
}

static void test_round_trip_all_categories(void)
{
    zbp_meta1_t m;
    uint8_t byte;
    uint8_t cat;
    int all_ok = 1;

    printf("test_round_trip_all_categories:\n");

    for (cat = 0; cat < 16; cat++) {
        byte = zbp_meta1_encode_wave_b(0, 0, cat);
        (void)zbp_meta1_decode(byte, &m);
        if (m.category != cat || m.mode != ZBP_MODE_WAVE || m.treatment != 1) {
            all_ok = 0;
            fprintf(stderr, "  FAIL at category %u\n", cat);
            break;
        }
    }
    ASSERT(all_ok == 1, "all 16 categories round-trip correctly");

    /* With ACK + continuation */
    for (cat = 0; cat < 16; cat++) {
        byte = zbp_meta1_encode_wave_b(1, 1, cat);
        (void)zbp_meta1_decode(byte, &m);
        if (m.category != cat || m.ack_sysctx != 1 || m.continuation != 1) {
            all_ok = 0;
            break;
        }
    }
    ASSERT(all_ok == 1, "all 16 categories round-trip with ack+cont");
}

/* ======================================================================== */

int main(void)
{
    printf("=== libzako-bitpads-core unit tests ===\n\n");

    test_wave_role_a_basic();
    test_wave_role_b_categories();
    test_record_mode();
    test_pure_signal();
    test_spec_examples();
    test_frame_utilities();
    test_error_handling();
    test_exhaustive_decode();
    test_round_trip_all_categories();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
