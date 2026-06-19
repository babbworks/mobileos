/*
 * test_bitledger.c — Unit tests for libzako-bitledger
 *
 * Tests Layer 2, Layer 3, control records, cross-layer validation,
 * conservation invariant, value split/join, and spec examples.
 */

#include "zako_bitledger.h"
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

static void test_value_split_join(void)
{
    uint32_t a, r;
    uint32_t n;

    printf("test_value_split_join:\n");

    /* Spec example 1: $4.53 → N=453, S=8, A=1, r=197 */
    zbl_value_split(453, 8, &a, &r);
    ASSERT(a == 1, "$4.53: A=1");
    ASSERT(r == 197, "$4.53: r=197");
    n = zbl_value_join(1, 197, 8);
    ASSERT(n == 453, "$4.53: join gives 453");

    /* Spec example 2: $98,765.43 → N=9876543, S=8, A=38580, r=63 */
    zbl_value_split(9876543, 8, &a, &r);
    ASSERT(a == 38580, "$98765.43: A=38580");
    ASSERT(r == 63, "$98765.43: r=63");
    n = zbl_value_join(38580, 63, 8);
    ASSERT(n == 9876543, "$98765.43: join gives 9876543");

    /* Spec example 3: quantity split — 24 units at $2.49 → A=249, r=24 */
    n = zbl_value_join(249, 24, 8);
    ASSERT(n == 249 * 256 + 24, "quantity: 249*256+24");
    zbl_value_split(n, 8, &a, &r);
    ASSERT(a == 249 && r == 24, "quantity round-trip");

    /* Max value: N = 33,554,431 */
    zbl_value_split(33554431, 8, &a, &r);
    ASSERT(a == 131071, "max: A=131071");
    ASSERT(r == 255, "max: r=255");
    n = zbl_value_join(131071, 255, 8);
    ASSERT(n == 33554431, "max: join gives 33554431");

    /* Different split: S=4 */
    zbl_value_split(453, 4, &a, &r);
    ASSERT(a == 28, "S=4: A=28");
    ASSERT(r == 5, "S=4: r=5");
    n = zbl_value_join(28, 5, 4);
    ASSERT(n == 453, "S=4: join gives 453");
}

static void test_record_round_trip(void)
{
    uint8_t buf[ZBL_LAYER3_SIZE];
    zbl_record_t rec;
    int rc;

    printf("test_record_round_trip:\n");

    /* Encode: $4.53, Plus/In, Paid, Credit, Op Income/Asset */
    rc = zbl_record_encode(
        453,                       /* value_n */
        0, 0,                      /* exact, no rounding */
        0,                         /* session default split order */
        0,                         /* Plus / In */
        0,                         /* Paid */
        0,                         /* Credit primary */
        0,                         /* no quantity */
        ZBL_AP_OP_INCOME_ASSET,    /* account pair */
        0,                         /* complete */
        0,                         /* no extension */
        buf
    );
    ASSERT(rc == ZBL_OK, "encode $4.53 OK");

    /* Decode */
    rc = zbl_record_decode(buf, 8, &rec);
    ASSERT(rc == ZBL_OK, "decode OK");
    ASSERT(rec.value_n == 453, "value=453");
    ASSERT(rec.value_a == 1, "A=1");
    ASSERT(rec.value_r == 197, "r=197");
    ASSERT(rec.rounding == 0, "exact");
    ASSERT(rec.direction == 0, "Plus/In");
    ASSERT(rec.status == 0, "Paid");
    ASSERT(rec.debit_credit == 0, "Credit");
    ASSERT(rec.account_pair == ZBL_AP_OP_INCOME_ASSET, "Op Income/Asset");
    ASSERT(rec.completeness == 0, "complete");
    ASSERT(rec.extension == 0, "no extension");
    ASSERT(rec.crosslayer_valid == 1, "cross-layer valid");
    ASSERT(rec.rounding_valid == 1, "rounding valid");
}

static void test_record_outflow(void)
{
    uint8_t buf[ZBL_LAYER3_SIZE];
    zbl_record_t rec;
    int rc;

    printf("test_record_outflow:\n");

    /* Encode: $98,765.43, Minus/Out, Debt, Debit, Op Expense/Liability */
    rc = zbl_record_encode(
        9876543, 1, 1, 0, 1, 1, 1, 0,
        ZBL_AP_OP_EXPENSE_LIABILITY, 0, 0, buf
    );
    ASSERT(rc == ZBL_OK, "encode large outflow OK");

    rc = zbl_record_decode(buf, 8, &rec);
    ASSERT(rc == ZBL_OK, "decode OK");
    ASSERT(rec.value_n == 9876543, "value=9876543");
    ASSERT(rec.rounding == 1, "rounded");
    ASSERT(rec.round_dir == 1, "rounded up");
    ASSERT(rec.direction == 1, "Minus/Out");
    ASSERT(rec.status == 1, "Debt");
    ASSERT(rec.debit_credit == 1, "Debit");
    ASSERT(rec.account_pair == ZBL_AP_OP_EXPENSE_LIABILITY, "Op Expense/Liability");
    ASSERT(rec.dir_echo == 1, "dir echo matches");
    ASSERT(rec.status_echo == 1, "status echo matches");
    ASSERT(rec.crosslayer_valid == 1, "cross-layer valid");
}

static void test_crosslayer_validation(void)
{
    uint8_t buf[ZBL_LAYER3_SIZE];
    zbl_record_t rec;

    printf("test_crosslayer_validation:\n");

    /* Encode a valid record, then corrupt the echo bits */
    zbl_record_encode(1000, 0, 0, 0, 1, 0, 1, 0,
                      ZBL_AP_ASSET_ASSET, 0, 0, buf);

    /* Corrupt bit 37 (dir echo) — flip it */
    /* bit 37 is at byte 4, bit position within byte = 7-(36%8) = 7-4 = 3 */
    buf[4] ^= 0x08u;  /* flip bit at position 37 (0-indexed: byte4, bit3) */

    zbl_record_decode(buf, 8, &rec);
    ASSERT(rec.crosslayer_valid == 0, "corrupted dir echo detected");

    /* Encode again, corrupt bit 38 (status echo) */
    zbl_record_encode(1000, 0, 0, 0, 0, 1, 0, 0,
                      ZBL_AP_SUSPENSE_CLEARING, 0, 0, buf);
    /* bit 38 = byte4, bit position 7-(37%8) = 7-5 = 2 */
    buf[4] ^= 0x04u;

    zbl_record_decode(buf, 8, &rec);
    ASSERT(rec.crosslayer_valid == 0, "corrupted status echo detected");
}

static void test_rounding_validity(void)
{
    uint8_t buf[ZBL_LAYER3_SIZE];
    zbl_record_t rec;

    printf("test_rounding_validity:\n");

    /* Valid: rounding=0, round_dir=0 (exact, direction N/A) */
    zbl_record_encode(100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, buf);
    zbl_record_decode(buf, 8, &rec);
    ASSERT(rec.rounding_valid == 1, "exact + dir=0 is valid");

    /* Valid: rounding=1, round_dir=0 (rounded down) */
    zbl_record_encode(100, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, buf);
    zbl_record_decode(buf, 8, &rec);
    ASSERT(rec.rounding_valid == 1, "rounded down is valid");

    /* Valid: rounding=1, round_dir=1 (rounded up) */
    zbl_record_encode(100, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, buf);
    zbl_record_decode(buf, 8, &rec);
    ASSERT(rec.rounding_valid == 1, "rounded up is valid");

    /* INVALID: rounding=0, round_dir=1 — protocol error */
    /* Must manually construct since encode won't produce this */
    zbl_record_encode(100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, buf);
    /* Flip bit 27 (round_dir) without setting bit 26 (rounding) */
    /* bit 27 = byte 3, position 7-((27-1)%8) = 7-2 = 5... let me calculate */
    /* bit 27: 0-indexed pos 26, byte 26/8=3, bit_in_byte 7-(26%8)=7-2=5 */
    buf[3] |= 0x20u;  /* set bit 27 */
    zbl_record_decode(buf, 8, &rec);
    ASSERT(rec.rounding_valid == 0, "bit26=0,bit27=1 is protocol error");
}

static void test_conservation_balanced(void)
{
    zbl_record_t batch[4];
    uint8_t buf[ZBL_LAYER3_SIZE];
    int64_t balance;
    int rc;

    printf("test_conservation_balanced:\n");

    /* Two inflows and two outflows that balance:
     * +500 +300 -600 -200 = 0 */
    zbl_record_encode(500, 0, 0, 0, 0, 0, 0, 0, ZBL_AP_OP_INCOME_ASSET, 0, 0, buf);
    zbl_record_decode(buf, 8, &batch[0]);

    zbl_record_encode(300, 0, 0, 0, 0, 0, 0, 0, ZBL_AP_OP_INCOME_ASSET, 0, 0, buf);
    zbl_record_decode(buf, 8, &batch[1]);

    zbl_record_encode(600, 0, 0, 0, 1, 0, 1, 0, ZBL_AP_OP_EXPENSE_ASSET, 0, 0, buf);
    zbl_record_decode(buf, 8, &batch[2]);

    zbl_record_encode(200, 0, 0, 0, 1, 0, 1, 0, ZBL_AP_OP_EXPENSE_ASSET, 0, 0, buf);
    zbl_record_decode(buf, 8, &batch[3]);

    rc = zbl_conservation_check(batch, 4, &balance);
    ASSERT(rc == ZBL_OK, "balanced batch passes");
    ASSERT(balance == 0, "balance is zero");
}

static void test_conservation_imbalanced(void)
{
    zbl_record_t batch[2];
    uint8_t buf[ZBL_LAYER3_SIZE];
    int64_t balance;
    int rc;

    printf("test_conservation_imbalanced:\n");

    /* +1000 -999 = imbalance of +1 */
    zbl_record_encode(1000, 0, 0, 0, 0, 0, 0, 0, ZBL_AP_OP_INCOME_ASSET, 0, 0, buf);
    zbl_record_decode(buf, 8, &batch[0]);

    zbl_record_encode(999, 0, 0, 0, 1, 0, 1, 0, ZBL_AP_OP_EXPENSE_ASSET, 0, 0, buf);
    zbl_record_decode(buf, 8, &batch[1]);

    rc = zbl_conservation_check(batch, 2, &balance);
    ASSERT(rc == ZBL_ERR_CONSERVATION, "imbalanced batch rejected");
    ASSERT(balance == 1, "balance reports +1");
}

static void test_layer2_round_trip(void)
{
    zbl_layer2_t fields, decoded;
    uint8_t buf[ZBL_LAYER2_SIZE];
    int rc;

    printf("test_layer2_round_trip:\n");

    memset(&fields, 0, sizeof(fields));
    fields.transmission_type = 2;   /* copy from sender */
    fields.scaling_factor = 2;      /* ×100 */
    fields.optimal_split = 8;       /* default */
    fields.decimal_pos = 2;         /* 2 dp */
    fields.enquiry_bell = 1;
    fields.ack_bell = 0;
    fields.group_sep = 3;
    fields.record_sep = 15;
    fields.file_sep = 2;
    fields.entity_id = 17;
    fields.currency_code = 5;       /* some currency */
    fields.rounding_balance = -3;
    fields.compound_prefix = 1;

    rc = zbl_layer2_encode(&fields, buf);
    ASSERT(rc == ZBL_OK, "layer2 encode OK");

    rc = zbl_layer2_decode(buf, &decoded);
    ASSERT(rc == ZBL_OK, "layer2 decode OK");
    ASSERT(decoded.transmission_type == 2, "tx type");
    ASSERT(decoded.scaling_factor == 2, "scaling");
    ASSERT(decoded.optimal_split == 8, "split");
    ASSERT(decoded.decimal_pos == 2, "decimal");
    ASSERT(decoded.enquiry_bell == 1, "enquiry");
    ASSERT(decoded.ack_bell == 0, "ack");
    ASSERT(decoded.group_sep == 3, "group sep");
    ASSERT(decoded.record_sep == 15, "record sep");
    ASSERT(decoded.file_sep == 2, "file sep");
    ASSERT(decoded.entity_id == 17, "entity");
    ASSERT(decoded.currency_code == 5, "currency");
    ASSERT(decoded.rounding_balance == -3, "rounding balance");
    ASSERT(decoded.compound_prefix == 1, "compound");
}

static void test_layer2_invalid_type(void)
{
    zbl_layer2_t fields;
    uint8_t buf[ZBL_LAYER2_SIZE];

    printf("test_layer2_invalid_type:\n");

    memset(&fields, 0, sizeof(fields));
    fields.transmission_type = 0;  /* protocol error */
    ASSERT(zbl_layer2_encode(&fields, buf) == ZBL_ERR_INVALID,
           "type 00 rejected on encode");

    /* Manually create a buffer with bits 1-2 = 00 */
    memset(buf, 0, ZBL_LAYER2_SIZE);
    buf[5] = 0x01u; /* bit 48 = 1 (reserved) to avoid all-zero */
    ASSERT(zbl_layer2_decode(buf, &fields) == ZBL_ERR_INVALID,
           "type 00 rejected on decode");
}

static void test_control_records(void)
{
    zbl_control_t ctrl;
    uint8_t byte;

    printf("test_control_records:\n");

    /* Session open */
    byte = zbl_control_encode(ZBL_CTRL_SESSION_OPEN, 0);
    zbl_control_decode(byte, &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_SESSION_OPEN, "session open type");
    ASSERT(ctrl.payload == 0, "no payload");

    /* Batch close with payload */
    byte = zbl_control_encode(ZBL_CTRL_BATCH_CLOSE, 0x1F);
    zbl_control_decode(byte, &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_BATCH_CLOSE, "batch close type");
    ASSERT(ctrl.payload == 0x1F, "max payload");

    /* All 8 types round-trip */
    int all_ok = 1;
    for (uint8_t t = 0; t < 8; t++) {
        byte = zbl_control_encode(t, t);
        zbl_control_decode(byte, &ctrl);
        if (ctrl.type != t || ctrl.payload != t) { all_ok = 0; break; }
    }
    ASSERT(all_ok == 1, "all 8 control types round-trip");

    ASSERT(zbl_control_decode(0, NULL) == ZBL_ERR_NULL, "NULL check");
}

static void test_overflow(void)
{
    uint8_t buf[ZBL_LAYER3_SIZE];
    int rc;

    printf("test_overflow:\n");

    /* Max value = 33,554,431 should succeed */
    rc = zbl_record_encode(33554431, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, buf);
    ASSERT(rc == ZBL_OK, "max value encodes");

    /* Max + 1 should fail */
    rc = zbl_record_encode(33554432, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, buf);
    ASSERT(rc == ZBL_ERR_OVERFLOW, "max+1 overflows");
}

static void test_account_pair_names(void)
{
    printf("test_account_pair_names:\n");

    ASSERT(strcmp(zbl_account_pair_name(0), "Op Expense / Asset") == 0, "pair 0");
    ASSERT(strcmp(zbl_account_pair_name(4), "Op Income / Asset") == 0, "pair 4");
    ASSERT(strcmp(zbl_account_pair_name(15), "Compound Continuation") == 0, "pair 15");
    ASSERT(strcmp(zbl_account_pair_name(16), "UNKNOWN") == 0, "pair 16 invalid");
}

static void test_null_checks(void)
{
    int rc;

    printf("test_null_checks:\n");

    rc = zbl_layer2_decode(NULL, NULL);
    ASSERT(rc == ZBL_ERR_NULL, "layer2 decode NULL");
    rc = zbl_layer2_encode(NULL, NULL);
    ASSERT(rc == ZBL_ERR_NULL, "layer2 encode NULL");
    rc = zbl_record_decode(NULL, 8, NULL);
    ASSERT(rc == ZBL_ERR_NULL, "record decode NULL");
    rc = zbl_record_encode(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL);
    ASSERT(rc == ZBL_ERR_NULL, "record encode NULL");
    rc = zbl_conservation_check(NULL, 1, NULL);
    ASSERT(rc == ZBL_ERR_NULL, "conservation NULL");
}

/* ======================================================================== */

int main(void)
{
    printf("=== libzako-bitledger unit tests ===\n\n");

    test_value_split_join();
    test_record_round_trip();
    test_record_outflow();
    test_crosslayer_validation();
    test_rounding_validity();
    test_conservation_balanced();
    test_conservation_imbalanced();
    test_layer2_round_trip();
    test_layer2_invalid_type();
    test_control_records();
    test_overflow();
    test_account_pair_names();
    test_null_checks();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
