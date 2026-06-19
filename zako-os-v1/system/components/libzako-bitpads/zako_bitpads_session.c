/*
 * zako_bitpads_session.c — BitPads v2.0 Session Layer Implementation
 *
 * Layer 1 (64-bit session header), CRC-15, Meta Byte 2,
 * Setup Byte, Value Block, Time, Task, Note component codecs.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "zako_bitpads_session.h"
#include <string.h>

/* ========================================================================
 * CRC-15 — Polynomial x^15 + x + 1
 *
 * Processes individual bits from MSB of first byte to last bit.
 * Used over Layer 1 bits 1-49 (6 bytes + 1 bit).
 * ======================================================================== */

uint16_t zbp_crc15(const uint8_t *data, size_t num_bits)
{
    uint16_t crc = 0u;
    size_t i;

    if (data == NULL || num_bits == 0u) {
        return 0u;
    }

    for (i = 0u; i < num_bits; i++) {
        size_t byte_idx = i / 8u;
        size_t bit_idx  = 7u - (i % 8u);  /* MSB first */
        uint8_t bit = (uint8_t)((data[byte_idx] >> bit_idx) & 1u);

        /* XOR in the new bit at position 14 (MSB of 15-bit register) */
        uint8_t feedback = (uint8_t)((crc >> 14u) ^ bit);
        crc = (uint16_t)((crc << 1u) & 0x7FFFu);

        if (feedback) {
            crc ^= ZBP_L1_CRC_POLY;
        }
    }

    return (uint16_t)(crc & 0x7FFFu);
}

/* ========================================================================
 * META BYTE 2
 *
 * Layout:
 *   Bits 1-4: Archetype / Extended Flags (4 bits)
 *   Bits 5-6: Time Reference Selector (2 bits)
 *   Bit 7:    Setup Byte Present
 *   Bit 8:    Signal Slot Presence (v2.0)
 * ======================================================================== */

int zbp_meta2_decode(uint8_t byte, zbp_meta2_t *out)
{
    if (out == NULL) {
        return ZBP_ERR_NULL;
    }

    out->raw = byte;
    out->archetype       = (uint8_t)((byte >> 4u) & 0x0Fu);
    out->time_ref_sel    = (uint8_t)((byte >> 2u) & 0x03u);
    out->setup_present   = (uint8_t)((byte >> 1u) & 0x01u);
    out->sigslot_present = (uint8_t)(byte & 0x01u);

    return ZBP_OK;
}

uint8_t zbp_meta2_encode(uint8_t archetype, uint8_t time_ref_sel,
                         uint8_t setup_present, uint8_t sigslot_present)
{
    uint8_t byte = 0u;

    byte |= (uint8_t)((archetype & 0x0Fu) << 4u);
    byte |= (uint8_t)((time_ref_sel & 0x03u) << 2u);
    if (setup_present)   { byte |= 0x02u; }
    if (sigslot_present) { byte |= 0x01u; }

    return byte;
}

/* ========================================================================
 * LAYER 1 — 64-BIT SESSION HEADER
 *
 * Bit-level packing into 8 bytes, big-endian bit order.
 * Bit 1 = MSB of byte 0, Bit 64 = LSB of byte 7.
 * ======================================================================== */

/* Helper: extract N bits starting at bit position (1-indexed, MSB=1) */
static uint32_t extract_bits(const uint8_t *data, size_t start_bit, size_t count)
{
    uint32_t result = 0u;
    size_t i;

    for (i = 0u; i < count; i++) {
        size_t bit_pos = (start_bit - 1u) + i;  /* 0-indexed */
        size_t byte_idx = bit_pos / 8u;
        size_t bit_idx  = 7u - (bit_pos % 8u);
        uint8_t bit = (uint8_t)((data[byte_idx] >> bit_idx) & 1u);
        result = (result << 1u) | bit;
    }

    return result;
}

/* Helper: set N bits starting at bit position (1-indexed) */
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

int zbp_layer1_decode(const uint8_t data[ZBP_L1_SIZE], zbp_layer1_t *out)
{
    if (data == NULL || out == NULL) {
        return ZBP_ERR_NULL;
    }

    memcpy(out->raw, data, ZBP_L1_SIZE);

    out->soh          = (uint8_t)extract_bits(data, 1, 1);
    out->wire_version = (uint8_t)extract_bits(data, 2, 1);
    out->domain       = (uint8_t)extract_bits(data, 3, 2);
    out->permissions  = (uint8_t)extract_bits(data, 5, 4);
    out->split_order  = (uint8_t)extract_bits(data, 9, 1);
    out->sender_split = (uint8_t)extract_bits(data, 10, 2);
    out->session_enh  = (uint8_t)extract_bits(data, 12, 1);
    out->sender_id    = (uint32_t)extract_bits(data, 13, 32);
    out->sub_entity   = (uint8_t)extract_bits(data, 45, 5);
    out->crc15        = (uint16_t)extract_bits(data, 50, 15);

    /* Compute CRC over bits 1-49 and compare */
    out->crc15_computed = zbp_crc15(data, 49u);
    out->crc_valid = (out->crc15 == out->crc15_computed) ? 1u : 0u;

    return ZBP_OK;
}

int zbp_layer1_encode(uint8_t domain, uint8_t permissions,
                      uint8_t split_order, uint8_t sender_split,
                      uint8_t session_enh, uint32_t sender_id,
                      uint8_t sub_entity, uint8_t out[ZBP_L1_SIZE])
{
    uint16_t crc;

    if (out == NULL) {
        return ZBP_ERR_NULL;
    }

    memset(out, 0, ZBP_L1_SIZE);

    /* SOH = 1 (always) */
    set_bits(out, 1, 1, 1u);
    /* Wire version = 0 (v2.0) */
    set_bits(out, 2, 1, 0u);
    set_bits(out, 3, 2, (uint32_t)(domain & 0x03u));
    set_bits(out, 5, 4, (uint32_t)(permissions & 0x0Fu));
    set_bits(out, 9, 1, (uint32_t)(split_order & 0x01u));
    set_bits(out, 10, 2, (uint32_t)(sender_split & 0x03u));
    set_bits(out, 12, 1, (uint32_t)(session_enh & 0x01u));
    set_bits(out, 13, 32, sender_id);
    set_bits(out, 45, 5, (uint32_t)(sub_entity & 0x1Fu));

    /* Compute CRC-15 over bits 1-49 and insert at bits 50-64 */
    crc = zbp_crc15(out, 49u);
    set_bits(out, 50, 15, (uint32_t)crc);

    return ZBP_OK;
}

/* ========================================================================
 * SETUP BYTE
 *
 * Layout:
 *   Bits 1-2: Value Tier (00=T1, 01=T2, 10=T3, 11=T4)
 *   Bits 3-4: Scaling Factor (00=x1, 01=x10, 10=x100, 11=x1000)
 *   Bits 5-6: Decimal Position (00=int, 01=1dp, 10=2dp, 11=3dp)
 *   Bit 7:    Context Source (0=inline, 1=inherit)
 *   Bit 8:    Rounding Convention (0=half-up, 1=half-even)
 * ======================================================================== */

int zbp_setup_decode(uint8_t byte, zbp_setup_t *out)
{
    if (out == NULL) {
        return ZBP_ERR_NULL;
    }

    out->raw = byte;
    out->value_tier  = (uint8_t)((byte >> 6u) & 0x03u);
    out->scaling     = (uint8_t)((byte >> 4u) & 0x03u);
    out->decimal_pos = (uint8_t)((byte >> 2u) & 0x03u);
    out->context_src = (uint8_t)((byte >> 1u) & 0x01u);
    out->rounding    = (uint8_t)(byte & 0x01u);

    return ZBP_OK;
}

uint8_t zbp_setup_encode(uint8_t tier, uint8_t scaling,
                         uint8_t decimal_pos, uint8_t context_src,
                         uint8_t rounding)
{
    uint8_t byte = 0u;

    byte |= (uint8_t)((tier & 0x03u) << 6u);
    byte |= (uint8_t)((scaling & 0x03u) << 4u);
    byte |= (uint8_t)((decimal_pos & 0x03u) << 2u);
    if (context_src) { byte |= 0x02u; }
    if (rounding)    { byte |= 0x01u; }

    return byte;
}

size_t zbp_value_tier_bytes(uint8_t tier)
{
    static const size_t tier_sizes[4] = { 1u, 2u, 3u, 4u };
    return tier_sizes[tier & 0x03u];
}

/* ========================================================================
 * VALUE BLOCK — N encoded as big-endian bytes
 * ======================================================================== */

int zbp_value_encode(uint32_t n, uint8_t tier,
                     uint8_t *out, size_t *out_len)
{
    size_t nbytes;
    size_t i;

    if (out == NULL || out_len == NULL) {
        return ZBP_ERR_NULL;
    }

    nbytes = zbp_value_tier_bytes(tier);

    /* Check N fits in the tier */
    if (tier == ZBP_VALUE_TIER1 && n > 255u) { return ZBP_ERR_SIZE; }
    if (tier == ZBP_VALUE_TIER2 && n > 65535u) { return ZBP_ERR_SIZE; }
    if (tier == ZBP_VALUE_TIER3 && n > 16777215u) { return ZBP_ERR_SIZE; }
    /* Tier 4: all uint32_t values fit */

    /* Encode big-endian */
    for (i = 0u; i < nbytes; i++) {
        out[nbytes - 1u - i] = (uint8_t)(n & 0xFFu);
        n >>= 8u;
    }

    *out_len = nbytes;
    return ZBP_OK;
}

int zbp_value_decode(const uint8_t *data, uint8_t tier, uint32_t *out_n)
{
    size_t nbytes;
    size_t i;
    uint32_t n = 0u;

    if (data == NULL || out_n == NULL) {
        return ZBP_ERR_NULL;
    }

    nbytes = zbp_value_tier_bytes(tier);

    /* Decode big-endian */
    for (i = 0u; i < nbytes; i++) {
        n = (n << 8u) | data[i];
    }

    *out_n = n;
    return ZBP_OK;
}

/* ========================================================================
 * TASK BYTE
 *
 * Layout:
 *   Bits 1-4: Category (16 task types)
 *   Bits 5-6: Priority (4 levels)
 *   Bit 7:    Target Specified
 *   Bit 8:    Timing Specified
 * ======================================================================== */

int zbp_task_decode(uint8_t byte, zbp_task_t *out)
{
    if (out == NULL) {
        return ZBP_ERR_NULL;
    }

    out->raw = byte;
    out->category    = (uint8_t)((byte >> 4u) & 0x0Fu);
    out->priority    = (uint8_t)((byte >> 2u) & 0x03u);
    out->target_spec = (uint8_t)((byte >> 1u) & 0x01u);
    out->timing_spec = (uint8_t)(byte & 0x01u);

    return ZBP_OK;
}

uint8_t zbp_task_encode(uint8_t category, uint8_t priority,
                        uint8_t target_spec, uint8_t timing_spec)
{
    uint8_t byte = 0u;

    byte |= (uint8_t)((category & 0x0Fu) << 4u);
    byte |= (uint8_t)((priority & 0x03u) << 2u);
    if (target_spec) { byte |= 0x02u; }
    if (timing_spec) { byte |= 0x01u; }

    return byte;
}

/* ========================================================================
 * NOTE HEADER
 *
 * Layout:
 *   Bits 1-2: Encoding Type
 *   Bits 3-4: Language / Codebook
 *   Bits 5-8: Length Field (0000=next byte, 0001-1110=1-14 bytes, 1111=ext)
 * ======================================================================== */

int zbp_note_header_decode(uint8_t byte, zbp_note_header_t *out)
{
    if (out == NULL) {
        return ZBP_ERR_NULL;
    }

    out->raw = byte;
    out->encoding     = (uint8_t)((byte >> 6u) & 0x03u);
    out->codebook     = (uint8_t)((byte >> 4u) & 0x03u);
    out->length_field = (uint8_t)(byte & 0x0Fu);

    /* Resolve body length */
    if (out->length_field == 0u) {
        out->body_length = 0u;  /* Next byte is actual length — caller reads it */
    } else if (out->length_field <= 14u) {
        out->body_length = (size_t)out->length_field;
    } else {
        out->body_length = 0u;  /* 1111 = extended (2 bytes follow) — caller reads them */
    }

    return ZBP_OK;
}

uint8_t zbp_note_header_encode(uint8_t encoding, uint8_t codebook,
                               uint8_t length_field)
{
    uint8_t byte = 0u;

    byte |= (uint8_t)((encoding & 0x03u) << 6u);
    byte |= (uint8_t)((codebook & 0x03u) << 4u);
    byte |= (uint8_t)(length_field & 0x0Fu);

    return byte;
}

/* ========================================================================
 * TIME BLOCK HEADER (TIER 2)
 *
 * Layout:
 *   Bits 1-2: Timestamp Format
 *   Bits 3-4: Resolution
 *   Bit 5:    Timezone Present
 *   Bit 6:    Duration Present
 *   Bits 7-8: Reserved (00)
 * ======================================================================== */

int zbp_time_header_decode(uint8_t byte, zbp_time_header_t *out)
{
    if (out == NULL) {
        return ZBP_ERR_NULL;
    }

    out->raw = byte;
    out->format      = (uint8_t)((byte >> 6u) & 0x03u);
    out->resolution  = (uint8_t)((byte >> 4u) & 0x03u);
    out->tz_present  = (uint8_t)((byte >> 3u) & 0x01u);
    out->dur_present = (uint8_t)((byte >> 2u) & 0x01u);

    return ZBP_OK;
}

uint8_t zbp_time_header_encode(uint8_t format, uint8_t resolution,
                               uint8_t tz_present, uint8_t dur_present)
{
    uint8_t byte = 0u;

    byte |= (uint8_t)((format & 0x03u) << 6u);
    byte |= (uint8_t)((resolution & 0x03u) << 4u);
    if (tz_present)  { byte |= 0x08u; }
    if (dur_present) { byte |= 0x04u; }
    /* Bits 7-8 reserved = 00 */

    return byte;
}
