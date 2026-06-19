/*
 * zako_bitpads_core.c — BitPads v2.0 Core Codec Implementation
 *
 * Meta Byte 1 parsing, Wave mode encoding, Pure Signal, frame utilities.
 *
 * Implementation follows the BitPads v2.0 Decoder Decision Tree (§14):
 *   READ meta1
 *   IF meta1.bit1 == 0: WAVE MODE
 *     IF meta1.bit4 == 0: Role A (Basic Treatment)
 *     ELSE: Role B (Category Mode)
 *   IF meta1.bit1 == 1: RECORD MODE
 *     → Role C component flags in bits 5-8
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "zako_bitpads_core.h"

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

/* Extract a single bit as 0 or 1 */
static inline uint8_t bit_val(uint8_t byte, uint8_t mask)
{
    return (uint8_t)((byte & mask) ? 1u : 0u);
}

/* ========================================================================
 * META BYTE 1 — DECODE
 * ======================================================================== */

int zbp_meta1_decode(uint8_t byte, zbp_meta1_t *out)
{
    if (out == NULL) {
        return ZBP_ERR_NULL;
    }

    out->raw = byte;

    /* Decode control bits (always valid) */
    out->mode         = bit_val(byte, ZBP_BIT1);
    out->ack_sysctx   = bit_val(byte, ZBP_BIT2);
    out->continuation = bit_val(byte, ZBP_BIT3);
    out->treatment    = bit_val(byte, ZBP_BIT4);
    out->content      = (uint8_t)(byte & ZBP_CONTENT_MASK);

    if (out->mode == ZBP_MODE_WAVE) {
        if (out->treatment == 0u) {
            /* Role A — Basic Treatment: bits 5-8 are independent flags */
            out->priority    = bit_val(byte, ZBP_BIT5);
            out->cipher      = bit_val(byte, ZBP_BIT6);
            out->ext_flags   = bit_val(byte, ZBP_BIT7);
            out->profile_def = bit_val(byte, ZBP_BIT8);
            out->category    = 0u;
        } else {
            /* Role B — Category Mode: bits 5-8 form 4-bit category */
            out->category    = out->content;
            out->priority    = 0u;
            out->cipher      = 0u;
            out->ext_flags   = 0u;
            out->profile_def = 0u;
        }
        /* Clear Record-mode fields */
        out->value_present = 0u;
        out->time_present  = 0u;
        out->task_present  = 0u;
        out->note_present  = 0u;
    } else {
        /* Record mode — Role C: bits 5-8 are component expect flags */
        out->value_present = bit_val(byte, ZBP_BIT5);
        out->time_present  = bit_val(byte, ZBP_BIT6);
        out->task_present  = bit_val(byte, ZBP_BIT7);
        out->note_present  = bit_val(byte, ZBP_BIT8);
        /* Clear Wave-mode fields */
        out->priority    = 0u;
        out->cipher      = 0u;
        out->ext_flags   = 0u;
        out->profile_def = 0u;
        out->category    = 0u;
    }

    return ZBP_OK;
}

/* ========================================================================
 * META BYTE 1 — ENCODE
 * ======================================================================== */

uint8_t zbp_meta1_encode_wave_a(uint8_t ack, uint8_t cont,
                                uint8_t priority, uint8_t cipher,
                                uint8_t ext_flags, uint8_t profile)
{
    uint8_t byte = 0u;

    /* Bit 1 = 0 (Wave), Bit 4 = 0 (Role A / Basic) */
    if (ack)       { byte |= ZBP_BIT2; }
    if (cont)      { byte |= ZBP_BIT3; }
    /* bit 4 stays 0 for Role A */
    if (priority)  { byte |= ZBP_BIT5; }
    if (cipher)    { byte |= ZBP_BIT6; }
    if (ext_flags) { byte |= ZBP_BIT7; }
    if (profile)   { byte |= ZBP_BIT8; }

    return byte;
}

uint8_t zbp_meta1_encode_wave_b(uint8_t ack, uint8_t cont,
                                uint8_t category)
{
    uint8_t byte = 0u;

    /* Bit 1 = 0 (Wave), Bit 4 = 1 (Role B / Category) */
    if (ack)  { byte |= ZBP_BIT2; }
    if (cont) { byte |= ZBP_BIT3; }
    byte |= ZBP_BIT4;  /* Treatment Switch = 1 → Category Mode */
    byte |= (uint8_t)(category & ZBP_CONTENT_MASK);

    return byte;
}

uint8_t zbp_meta1_encode_record(uint8_t sysctx, uint8_t cont,
                                uint8_t value, uint8_t time,
                                uint8_t task, uint8_t note)
{
    uint8_t byte = ZBP_BIT1;  /* Bit 1 = 1 (Record mode) */

    if (sysctx) { byte |= ZBP_BIT2; }
    if (cont)   { byte |= ZBP_BIT3; }
    /* Bit 4 = 0 (reserved in Record mode) */
    if (value)  { byte |= ZBP_BIT5; }
    if (time)   { byte |= ZBP_BIT6; }
    if (task)   { byte |= ZBP_BIT7; }
    if (note)   { byte |= ZBP_BIT8; }

    return byte;
}

/* ========================================================================
 * PURE SIGNAL
 * ======================================================================== */

int zbp_pure_signal_encode(uint8_t ack, uint8_t *out_byte)
{
    if (out_byte == NULL) {
        return ZBP_ERR_NULL;
    }

    /*
     * Pure Signal = Wave / Role B / Category 0000
     * Bit 1 = 0 (Wave)
     * Bit 4 = 1 (Category Mode)
     * Bits 5-8 = 0000 (Pure Signal / Heartbeat)
     */
    *out_byte = zbp_meta1_encode_wave_b(ack, 0u, ZBP_CAT_PURE_SIGNAL);

    return ZBP_OK;
}

int zbp_pure_signal_decode(uint8_t byte, zbp_meta1_t *out)
{
    zbp_meta1_t tmp;
    int rc;

    rc = zbp_meta1_decode(byte, &tmp);
    if (rc != ZBP_OK) {
        return rc;
    }

    /* Must be Wave, Role B, Category 0000 */
    if (tmp.mode != ZBP_MODE_WAVE) {
        return ZBP_ERR_INVALID;
    }
    if (tmp.treatment != 1u) {
        return ZBP_ERR_INVALID;
    }
    if (tmp.category != ZBP_CAT_PURE_SIGNAL) {
        return ZBP_ERR_INVALID;
    }

    if (out != NULL) {
        *out = tmp;
    }

    return ZBP_OK;
}

/* ========================================================================
 * FRAME UTILITIES
 * ======================================================================== */

int zbp_is_wave(uint8_t meta_byte)
{
    return (meta_byte & ZBP_BIT1) == 0u ? 1 : 0;
}

int zbp_is_record(uint8_t meta_byte)
{
    return (meta_byte & ZBP_BIT1) != 0u ? 1 : 0;
}

uint8_t zbp_category_l1_requirement(uint8_t category)
{
    /*
     * Layer 1 requirement matrix from BitPads v2.0 §9:
     *   0000 Pure Signal      — Not required
     *   0001 Status Report    — Optional
     *   0010 Command          — Optional
     *   0011 Query            — Optional
     *   0100 Alert            — Recommended
     *   0101 ACK              — Optional
     *   0110 NACK             — Optional
     *   0111 Sync             — Recommended
     *   1000 Discovery        — Not required
     *   1001 Data Transfer    — Required
     *   1010 Configuration    — Required
     *   1011 Diagnostic       — Optional
     *   1100 Financial        — Required (v2.0)
     *   1101 Identity         — Required (v2.0)
     *   1110 Control          — Optional (v2.0)
     *   1111 Extended         — Required
     */
    static const uint8_t l1_table[16] = {
        ZBP_L1_NOT_REQUIRED,  /* 0000 */
        ZBP_L1_OPTIONAL,      /* 0001 */
        ZBP_L1_OPTIONAL,      /* 0010 */
        ZBP_L1_OPTIONAL,      /* 0011 */
        ZBP_L1_RECOMMENDED,   /* 0100 */
        ZBP_L1_OPTIONAL,      /* 0101 */
        ZBP_L1_OPTIONAL,      /* 0110 */
        ZBP_L1_RECOMMENDED,   /* 0111 */
        ZBP_L1_NOT_REQUIRED,  /* 1000 */
        ZBP_L1_REQUIRED,      /* 1001 */
        ZBP_L1_REQUIRED,      /* 1010 */
        ZBP_L1_OPTIONAL,      /* 1011 */
        ZBP_L1_REQUIRED,      /* 1100 */
        ZBP_L1_REQUIRED,      /* 1101 */
        ZBP_L1_OPTIONAL,      /* 1110 */
        ZBP_L1_REQUIRED,      /* 1111 */
    };

    return l1_table[category & 0x0Fu];
}

size_t zbp_wave_min_size(uint8_t meta_byte)
{
    zbp_meta1_t m;

    if (zbp_meta1_decode(meta_byte, &m) != ZBP_OK) {
        return 1u;
    }

    if (m.mode != ZBP_MODE_WAVE) {
        return 1u;  /* Caller error — not a wave byte */
    }

    if (m.treatment == 0u) {
        /* Role A — always 1 byte (meta only), unless ext_flags */
        return m.ext_flags ? 2u : 1u;
    }

    /* Role B — check if Layer 1 is required */
    uint8_t l1_req = zbp_category_l1_requirement(m.category);

    if (l1_req == ZBP_L1_REQUIRED) {
        /* Meta(1) + Layer1(8) = 9, or +1 for extended category byte */
        return (m.category == ZBP_CAT_EXTENDED) ? 10u : 9u;
    }

    /* For optional/recommended/not-required: minimum is just meta byte */
    /* Extended category always needs the extension byte */
    return (m.category == ZBP_CAT_EXTENDED) ? 2u : 1u;
}

size_t zbp_record_min_size(void)
{
    /* Meta1(1) + Meta2(1) + Layer1(8) = 10 bytes minimum */
    return 10u;
}
