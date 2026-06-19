/*
 * zako_bitpads.c — BitPads v2.0 frame codec implementation for ZAKO OS
 *
 * Implements the four frame types defined in zako_bitpads.h:
 *   - Pure Signal (1 byte)
 *   - Anonymous Wave (4 bytes)
 *   - Full Record (13-29 bytes)
 *   - Full BitLedger (22-44 bytes, delegated to libzako-bitledger)
 *
 * All encoding/decoding is zero-allocation, operating on caller-provided
 * fixed-size buffers. Wire format is big-endian.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "zako_bitpads.h"
#include <string.h>

/* ---- Internal helpers ---- */

/*
 * Pack a 16-bit value into 2 bytes, big-endian.
 */
static void pack_u16_be(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFFu);
}

/*
 * Unpack 2 bytes (big-endian) into a 16-bit value.
 */
static uint16_t unpack_u16_be(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
}

/*
 * Pack a 32-bit value into 4 bytes, big-endian.
 */
static void pack_u32_be(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)((val >> 16) & 0xFFu);
    buf[2] = (uint8_t)((val >> 8) & 0xFFu);
    buf[3] = (uint8_t)(val & 0xFFu);
}

/*
 * Unpack 4 bytes (big-endian) into a 32-bit value.
 */
static uint32_t unpack_u32_be(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
           (uint32_t)buf[3];
}

/* ---- Meta byte operations ---- */

int zako_meta_decode(uint8_t byte, zako_meta_t *out_meta)
{
    if (out_meta == NULL) {
        return ZAKO_BP_ERR_NULL;
    }

    out_meta->raw          = byte;
    out_meta->frame_type   = byte & ZAKO_FRAME_TYPE_MASK;
    out_meta->priority     = byte & ZAKO_PRIORITY_MASK;
    out_meta->domain       = byte & ZAKO_DOMAIN_MASK;
    out_meta->direction    = (byte & ZAKO_DIRECTION_MASK) ? 1u : 0u;
    out_meta->continuation = (byte & ZAKO_CONTINUATION_MASK) ? 1u : 0u;

    return ZAKO_BP_OK;
}

uint8_t zako_meta_encode(uint8_t frame_type, uint8_t priority,
                         uint8_t domain, uint8_t direction,
                         uint8_t continuation)
{
    uint8_t byte = 0u;

    byte |= (frame_type & ZAKO_FRAME_TYPE_MASK);
    byte |= (priority & ZAKO_PRIORITY_MASK);
    byte |= (domain & ZAKO_DOMAIN_MASK);
    byte |= (direction & ZAKO_DIRECTION_MASK);
    byte |= (continuation & ZAKO_CONTINUATION_MASK);

    return byte;
}

uint8_t zako_frame_type(uint8_t meta_byte)
{
    return meta_byte & ZAKO_FRAME_TYPE_MASK;
}

size_t zako_frame_min_size(uint8_t frame_type)
{
    switch (frame_type & ZAKO_FRAME_TYPE_MASK) {
    case ZAKO_FRAME_PURE_SIGNAL:
        return ZAKO_PURE_SIGNAL_SIZE;
    case ZAKO_FRAME_ANON_WAVE:
        return ZAKO_ANON_WAVE_SIZE;
    case ZAKO_FRAME_FULL_RECORD:
        return ZAKO_FULL_RECORD_MIN;
    case ZAKO_FRAME_FULL_BITLEDGER:
        return ZAKO_FULL_BITLEDGER_MIN;
    default:
        return 0u;
    }
}

/* ---- Pure Signal ---- */

int zako_pure_signal_encode(uint8_t priority, uint8_t domain,
                            uint8_t direction, uint8_t continuation,
                            uint8_t *out_frame)
{
    if (out_frame == NULL) {
        return ZAKO_BP_ERR_NULL;
    }

    out_frame[0] = zako_meta_encode(ZAKO_FRAME_PURE_SIGNAL, priority,
                                    domain, direction, continuation);
    return 1;
}

/* ---- Anonymous Wave ---- */

int zako_anon_wave_encode(uint8_t priority, uint8_t domain,
                          const uint8_t payload[3],
                          uint8_t *out_frame)
{
    if (payload == NULL || out_frame == NULL) {
        return ZAKO_BP_ERR_NULL;
    }

    out_frame[0] = zako_meta_encode(ZAKO_FRAME_ANON_WAVE, priority,
                                    domain, ZAKO_DIRECTION_OUTFLOW,
                                    ZAKO_CONTINUATION_LAST);
    out_frame[1] = payload[0];
    out_frame[2] = payload[1];
    out_frame[3] = payload[2];

    return 4;
}

int zako_anon_wave_decode(const uint8_t *frame, size_t frame_len,
                          zako_meta_t *out_meta,
                          uint8_t out_payload[3])
{
    if (frame == NULL || out_payload == NULL) {
        return ZAKO_BP_ERR_NULL;
    }
    if (frame_len < ZAKO_ANON_WAVE_SIZE) {
        return ZAKO_BP_ERR_TRUNCATED;
    }
    if (zako_frame_type(frame[0]) != ZAKO_FRAME_ANON_WAVE) {
        return ZAKO_BP_ERR_TYPE;
    }

    if (out_meta != NULL) {
        zako_meta_decode(frame[0], out_meta);
    }

    out_payload[0] = frame[1];
    out_payload[1] = frame[2];
    out_payload[2] = frame[3];

    return ZAKO_BP_OK;
}

/* ---- Full Record ---- */

/*
 * Full Record wire layout (13 bytes minimum):
 *
 * Byte 0:       Meta byte
 * Byte 1:       task_code (6 bits) | account_pair high 2 bits
 * Byte 2:       account_pair low 2 bits | sub_entity (5 bits) | file_sep high 1 bit
 * Byte 3:       file_sep low 2 bits | status (4 bits) | padding 2 bits
 * Bytes 4-5:    value (16 bits, big-endian)
 * Bytes 6-9:    wall_ts (32 bits, big-endian)
 * Bytes 10-11:  reserved / custom_domain + padding
 * Byte 12:      checksum (XOR of bytes 0-11)
 *
 * Total: 13 bytes minimum for a complete record.
 */

int zako_full_record_encode(const zako_meta_t *meta,
                            const zako_record_fields_t *fields,
                            uint8_t *out_frame,
                            size_t *out_len)
{
    uint8_t cksum;
    size_t i;

    if (meta == NULL || fields == NULL || out_frame == NULL || out_len == NULL) {
        return ZAKO_BP_ERR_NULL;
    }

    /* Byte 0: Meta byte (force frame type to Full Record) */
    out_frame[0] = zako_meta_encode(ZAKO_FRAME_FULL_RECORD,
                                    meta->priority,
                                    meta->domain,
                                    meta->direction ? ZAKO_DIRECTION_INFLOW : ZAKO_DIRECTION_OUTFLOW,
                                    meta->continuation ? ZAKO_CONTINUATION_MORE : ZAKO_CONTINUATION_LAST);

    /*
     * Byte 1: task_code[5:0] | account_pair[3:2]
     *   task_code occupies bits 7-2, account_pair high 2 bits in bits 1-0
     */
    out_frame[1] = (uint8_t)(((fields->task_code & 0x3Fu) << 2) |
                             ((fields->account_pair >> 2) & 0x03u));

    /*
     * Byte 2: account_pair[1:0] | sub_entity[4:0] | file_sep[2]
     *   account_pair low 2 bits in bits 7-6
     *   sub_entity in bits 5-1
     *   file_sep high bit in bit 0
     */
    out_frame[2] = (uint8_t)(((fields->account_pair & 0x03u) << 6) |
                             ((fields->sub_entity & 0x1Fu) << 1) |
                             ((fields->file_sep >> 2) & 0x01u));

    /*
     * Byte 3: file_sep[1:0] | status[3:0] | reserved[1:0]
     *   file_sep low 2 bits in bits 7-6
     *   status in bits 5-2
     *   reserved (zero) in bits 1-0
     */
    out_frame[3] = (uint8_t)(((fields->file_sep & 0x03u) << 6) |
                             ((fields->status & 0x0Fu) << 2));

    /* Bytes 4-5: value (16-bit big-endian) */
    pack_u16_be(&out_frame[4], (uint16_t)(fields->value & 0xFFFFu));

    /* Bytes 6-9: wall_ts (32-bit big-endian) */
    pack_u32_be(&out_frame[6], fields->wall_ts);

    /* Bytes 10-11: custom_domain + reserved */
    out_frame[10] = fields->custom_domain;
    out_frame[11] = 0x00u;  /* reserved */

    /* Byte 12: XOR checksum of bytes 0-11 */
    cksum = 0u;
    for (i = 0u; i < 12u; i++) {
        cksum ^= out_frame[i];
    }
    out_frame[12] = cksum;

    *out_len = ZAKO_FULL_RECORD_MIN;
    return ZAKO_BP_OK;
}

int zako_full_record_decode(const uint8_t *frame, size_t frame_len,
                            zako_meta_t *out_meta,
                            zako_record_fields_t *out_fields)
{
    uint8_t cksum;
    size_t i;

    if (frame == NULL || out_meta == NULL || out_fields == NULL) {
        return ZAKO_BP_ERR_NULL;
    }
    if (frame_len < ZAKO_FULL_RECORD_MIN) {
        return ZAKO_BP_ERR_TRUNCATED;
    }
    if (zako_frame_type(frame[0]) != ZAKO_FRAME_FULL_RECORD) {
        return ZAKO_BP_ERR_TYPE;
    }

    /* Verify checksum */
    cksum = 0u;
    for (i = 0u; i < 12u; i++) {
        cksum ^= frame[i];
    }
    if (cksum != frame[12]) {
        return ZAKO_BP_ERR_TRUNCATED;  /* checksum mismatch — corrupted */
    }

    /* Decode meta */
    zako_meta_decode(frame[0], out_meta);

    /* Decode fields */
    memset(out_fields, 0, sizeof(*out_fields));

    /* Byte 1: task_code[5:0] | account_pair[3:2] */
    out_fields->task_code    = (frame[1] >> 2) & 0x3Fu;
    out_fields->account_pair = (uint8_t)(((frame[1] & 0x03u) << 2) |
                                         ((frame[2] >> 6) & 0x03u));

    /* Byte 2: account_pair[1:0] | sub_entity[4:0] | file_sep[2] */
    out_fields->sub_entity = (frame[2] >> 1) & 0x1Fu;
    out_fields->file_sep   = (uint8_t)(((frame[2] & 0x01u) << 2) |
                                        ((frame[3] >> 6) & 0x03u));

    /* Byte 3: file_sep[1:0] | status[3:0] | reserved */
    out_fields->status = (frame[3] >> 2) & 0x0Fu;

    /* Bytes 4-5: value */
    out_fields->value = (uint32_t)unpack_u16_be(&frame[4]);

    /* Bytes 6-9: wall_ts */
    out_fields->wall_ts = unpack_u32_be(&frame[6]);

    /* Bytes 10-11: custom_domain */
    out_fields->custom_domain = frame[10];

    return ZAKO_BP_OK;
}
