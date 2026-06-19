/*
 * zako_bitledger.c — BitLedger v3.0 Codec Implementation
 *
 * Layer 2 (batch header), Layer 3 (40-bit transaction record),
 * control records, cross-layer validation, conservation invariant.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "zako_bitledger.h"
#include <string.h>

/* ========================================================================
 * BIT MANIPULATION HELPERS
 * ======================================================================== */

static uint32_t extract_bits(const uint8_t *data, size_t start_bit, size_t count)
{
    uint32_t result = 0u;
    size_t i;
    for (i = 0u; i < count; i++) {
        size_t bit_pos = (start_bit - 1u) + i;
        size_t byte_idx = bit_pos / 8u;
        size_t bit_idx  = 7u - (bit_pos % 8u);
        uint8_t bit = (uint8_t)((data[byte_idx] >> bit_idx) & 1u);
        result = (result << 1u) | bit;
    }
    return result;
}

static void set_bits(uint8_t *data, size_t start_bit, size_t count, uint32_t value)
{
    size_t i;
    for (i = 0u; i < count; i++) {
        size_t bit_pos = (start_bit - 1u) + i;
        size_t byte_idx = bit_pos / 8u;
        size_t bit_idx  = 7u - (bit_pos % 8u);
        uint8_t bit = (uint8_t)((value >> (count - 1u - i)) & 1u);
        if (bit) {
            data[byte_idx] |= (uint8_t)(1u << bit_idx);
        } else {
            data[byte_idx] &= (uint8_t)~(1u << bit_idx);
        }
    }
}

/* ========================================================================
 * LAYER 2 — BATCH HEADER (48 bits / 6 bytes)
 * ======================================================================== */

int zbl_layer2_decode(const uint8_t data[ZBL_LAYER2_SIZE], zbl_layer2_t *out)
{
    uint8_t rb_raw;

    if (data == NULL || out == NULL) {
        return ZBL_ERR_NULL;
    }

    memcpy(out->raw, data, ZBL_LAYER2_SIZE);

    out->transmission_type = (uint8_t)extract_bits(data, 1, 2);
    out->scaling_factor    = (uint8_t)extract_bits(data, 3, 7);
    out->optimal_split     = (uint8_t)extract_bits(data, 10, 4);
    out->decimal_pos       = (uint8_t)extract_bits(data, 14, 3);
    out->enquiry_bell      = (uint8_t)extract_bits(data, 17, 1);
    out->ack_bell          = (uint8_t)extract_bits(data, 18, 1);
    out->group_sep         = (uint8_t)extract_bits(data, 19, 4);
    out->record_sep        = (uint8_t)extract_bits(data, 23, 5);
    out->file_sep          = (uint8_t)extract_bits(data, 28, 3);
    out->entity_id         = (uint8_t)extract_bits(data, 31, 5);
    out->currency_code     = (uint8_t)extract_bits(data, 36, 6);

    /* Rounding balance: 4-bit sign-magnitude (bit 42 = sign, bits 43-45 = magnitude) */
    rb_raw = (uint8_t)extract_bits(data, 42, 4);
    if (rb_raw & 0x08u) {
        out->rounding_balance = -(int8_t)(rb_raw & 0x07u);
    } else {
        out->rounding_balance = (int8_t)(rb_raw & 0x07u);
    }

    out->compound_prefix = (uint8_t)extract_bits(data, 46, 2);
    /* bit 48 is reserved = 1 (we don't store it) */

    /* Validate transmission type (00 is protocol error) */
    if (out->transmission_type == 0u) {
        return ZBL_ERR_INVALID;
    }

    return ZBL_OK;
}

int zbl_layer2_encode(const zbl_layer2_t *fields, uint8_t out[ZBL_LAYER2_SIZE])
{
    uint8_t rb_raw;

    if (fields == NULL || out == NULL) {
        return ZBL_ERR_NULL;
    }

    if (fields->transmission_type == 0u) {
        return ZBL_ERR_INVALID;
    }

    memset(out, 0, ZBL_LAYER2_SIZE);

    set_bits(out, 1, 2, (uint32_t)(fields->transmission_type & 0x03u));
    set_bits(out, 3, 7, (uint32_t)(fields->scaling_factor & 0x7Fu));
    set_bits(out, 10, 4, (uint32_t)(fields->optimal_split & 0x0Fu));
    set_bits(out, 14, 3, (uint32_t)(fields->decimal_pos & 0x07u));
    set_bits(out, 17, 1, (uint32_t)(fields->enquiry_bell & 0x01u));
    set_bits(out, 18, 1, (uint32_t)(fields->ack_bell & 0x01u));
    set_bits(out, 19, 4, (uint32_t)(fields->group_sep & 0x0Fu));
    set_bits(out, 23, 5, (uint32_t)(fields->record_sep & 0x1Fu));
    set_bits(out, 28, 3, (uint32_t)(fields->file_sep & 0x07u));
    set_bits(out, 31, 5, (uint32_t)(fields->entity_id & 0x1Fu));
    set_bits(out, 36, 6, (uint32_t)(fields->currency_code & 0x3Fu));

    /* Rounding balance: sign-magnitude */
    if (fields->rounding_balance < 0) {
        rb_raw = (uint8_t)(0x08u | ((uint8_t)(-(fields->rounding_balance)) & 0x07u));
    } else {
        rb_raw = (uint8_t)(fields->rounding_balance & 0x07u);
    }
    set_bits(out, 42, 4, (uint32_t)rb_raw);

    set_bits(out, 46, 2, (uint32_t)(fields->compound_prefix & 0x03u));
    /* bit 48 = reserved = 1 */
    set_bits(out, 48, 1, 1u);

    return ZBL_OK;
}

/* ========================================================================
 * LAYER 3 — TRANSACTION RECORD (40 bits / 5 bytes)
 * ======================================================================== */

int zbl_record_decode(const uint8_t data[ZBL_LAYER3_SIZE],
                      uint8_t split_s, zbl_record_t *out)
{
    uint32_t pow2s;
    uint8_t flags;

    if (data == NULL || out == NULL) {
        return ZBL_ERR_NULL;
    }

    memcpy(out->raw, data, ZBL_LAYER3_SIZE);

    /*
     * Manual bitshift extraction — replaces per-bit extract_bits() loop.
     * Saves ~250 instructions per decode on the hot path.
     *
     * Layout (40 bits / 5 bytes, MSB-first):
     *   Byte 0: bits 1-8     (value_n[25:18])
     *   Byte 1: bits 9-16    (value_n[17:10])
     *   Byte 2: bits 17-24   (value_n[9:2])
     *   Byte 3: bits 25-32   (value_n[1:0] | rounding | round_dir | split_order | direction | status | debit_credit)
     *   Byte 4: bits 33-40   (qty_present | account_pair[4] | dir_echo | status_echo | completeness | extension)
     */

    /* Value block: 25 bits spanning bytes 0-3 (bits 1-25) */
    out->value_n = ((uint32_t)data[0] << 17)
                 | ((uint32_t)data[1] << 9)
                 | ((uint32_t)data[2] << 1)
                 | ((uint32_t)data[3] >> 7);

    /* Split into A and r based on provided S */
    if (split_s > 24u) { split_s = 8u; }
    pow2s = 1u << split_s;
    out->value_a = out->value_n / pow2s;
    out->value_r = out->value_n % pow2s;

    /* Flag bits 26-32: all in byte 3 (bits [6:0]) */
    flags = data[3];
    out->rounding     = (flags >> 6) & 1u;
    out->round_dir    = (flags >> 5) & 1u;
    out->split_order  = (flags >> 4) & 1u;
    out->direction    = (flags >> 3) & 1u;
    out->status       = (flags >> 2) & 1u;
    out->debit_credit = (flags >> 1) & 1u;
    out->qty_present  = flags & 1u;

    /* Accounting block: bits 33-40 all in byte 4 */
    flags = data[4];
    out->account_pair  = (flags >> 4) & 0x0Fu;
    out->dir_echo      = (flags >> 3) & 1u;
    out->status_echo   = (flags >> 2) & 1u;
    out->completeness  = (flags >> 1) & 1u;
    out->extension     = flags & 1u;

    /* Cross-layer validation */
    out->crosslayer_valid = ((out->direction == out->dir_echo) &&
                             (out->status == out->status_echo)) ? 1u : 0u;

    /* Rounding validity: bit26=0 AND bit27=1 is protocol error */
    out->rounding_valid = !((out->rounding == 0u) && (out->round_dir == 1u)) ? 1u : 0u;

    return ZBL_OK;
}

int zbl_record_encode(uint32_t value_n,
                      uint8_t rounding, uint8_t round_dir,
                      uint8_t split_order, uint8_t direction,
                      uint8_t status, uint8_t debit_credit,
                      uint8_t qty_present, uint8_t account_pair,
                      uint8_t completeness, uint8_t extension,
                      uint8_t out[ZBL_LAYER3_SIZE])
{
    if (out == NULL) {
        return ZBL_ERR_NULL;
    }

    /* 25-bit max = 33,554,431 */
    if (value_n > 0x01FFFFFFu) {
        return ZBL_ERR_OVERFLOW;
    }

    memset(out, 0, ZBL_LAYER3_SIZE);

    /* Value block: bits 1-25 */
    set_bits(out, 1, 25, value_n);

    /* Flag bits: 26-32 */
    set_bits(out, 26, 1, (uint32_t)(rounding & 1u));
    set_bits(out, 27, 1, (uint32_t)(round_dir & 1u));
    set_bits(out, 28, 1, (uint32_t)(split_order & 1u));
    set_bits(out, 29, 1, (uint32_t)(direction & 1u));
    set_bits(out, 30, 1, (uint32_t)(status & 1u));
    set_bits(out, 31, 1, (uint32_t)(debit_credit & 1u));
    set_bits(out, 32, 1, (uint32_t)(qty_present & 1u));

    /* Accounting block: bits 33-40 */
    set_bits(out, 33, 4, (uint32_t)(account_pair & 0x0Fu));
    /* Cross-layer echo: bit37 = bit29, bit38 = bit30 */
    set_bits(out, 37, 1, (uint32_t)(direction & 1u));
    set_bits(out, 38, 1, (uint32_t)(status & 1u));
    set_bits(out, 39, 1, (uint32_t)(completeness & 1u));
    set_bits(out, 40, 1, (uint32_t)(extension & 1u));

    return ZBL_OK;
}

/* ========================================================================
 * CONTROL RECORDS
 * ======================================================================== */

int zbl_control_decode(uint8_t byte, zbl_control_t *out)
{
    if (out == NULL) {
        return ZBL_ERR_NULL;
    }

    out->raw     = byte;
    out->type    = (uint8_t)((byte >> 5u) & 0x07u);
    out->payload = (uint8_t)(byte & 0x1Fu);

    return ZBL_OK;
}

uint8_t zbl_control_encode(uint8_t type, uint8_t payload)
{
    return (uint8_t)(((type & 0x07u) << 5u) | (payload & 0x1Fu));
}

/* ========================================================================
 * CONSERVATION CHECK
 * ======================================================================== */

int zbl_conservation_check(const zbl_record_t *records, size_t count,
                           int64_t *out_balance)
{
    int64_t balance = 0;
    size_t i;

    if (records == NULL && count > 0u) {
        return ZBL_ERR_NULL;
    }

    for (i = 0u; i < count; i++) {
        int64_t val = (int64_t)records[i].value_n;
        if (records[i].direction == 0u) {
            /* Plus / In */
            balance += val;
        } else {
            /* Minus / Out */
            balance -= val;
        }
    }

    if (out_balance != NULL) {
        *out_balance = balance;
    }

    return (balance == 0) ? ZBL_OK : ZBL_ERR_CONSERVATION;
}

/* ========================================================================
 * VALUE HELPERS
 * ======================================================================== */

void zbl_value_split(uint32_t n, uint8_t split_s,
                     uint32_t *out_a, uint32_t *out_r)
{
    uint32_t pow2s;

    if (split_s > 24u) { split_s = 8u; }
    pow2s = 1u << split_s;

    if (out_a != NULL) { *out_a = n / pow2s; }
    if (out_r != NULL) { *out_r = n % pow2s; }
}

uint32_t zbl_value_join(uint32_t a, uint32_t r, uint8_t split_s)
{
    if (split_s > 24u) { split_s = 8u; }
    return (a << split_s) + r;
}

const char *zbl_account_pair_name(uint8_t code)
{
    static const char *names[16] = {
        "Op Expense / Asset",
        "Op Expense / Liability",
        "Non-Op Expense / Asset",
        "Non-Op Expense / Liability",
        "Op Income / Asset",
        "Op Income / Liability",
        "Non-Op Income / Asset",
        "Non-Op Income / Liability",
        "Asset / Liability",
        "Asset / Equity",
        "Liability / Equity",
        "Asset / Asset",
        "Suspense / Clearing",
        "Inter-Company",
        "Correction / Netting",
        "Compound Continuation"
    };

    if (code > 15u) { return "UNKNOWN"; }
    return names[code];
}
