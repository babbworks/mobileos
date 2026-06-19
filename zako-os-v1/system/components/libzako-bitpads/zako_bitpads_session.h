/*
 * zako_bitpads_session.h — BitPads v2.0 Session Layer Codec
 *
 * Implements the session and component layer of BitPads Protocol v2.0:
 *   - Meta Byte 2 (Record mode extended context)
 *   - Signal Slot Presence Byte (v2.0)
 *   - Layer 1 (64-bit session header with CRC-15)
 *   - Session Configuration Extension (v2.0)
 *   - Setup Byte (value encoding parameters)
 *   - Value Block (Tier 1-4 encoding/decoding)
 *   - Time Field (Tier 1 offset + Tier 2 block)
 *   - Task Block (8-bit task instruction)
 *   - Note Block (variable-length text/binary)
 *   - CRC-15 computation (polynomial x^15 + x + 1)
 *
 * Layer 1 — 64-bit Session Header layout:
 *   Bit 1:     SOH (always 1)
 *   Bit 2:     Wire Format Version (0=v2.0)
 *   Bits 3-4:  Domain (00=General, 01=Financial, 10=Control, 11=Extended)
 *   Bits 5-8:  Core Permissions (Write, Delegate, Compound, Admin)
 *   Bit 9:     Split Order Default (0=MSB first, 1=LSB first)
 *   Bits 10-11: Sender ID Split Mode
 *   Bit 12:    Session Enhancement Flag (v2.0)
 *   Bits 13-44: Sender ID (32 bits)
 *   Bits 45-49: Sub-Entity ID (5 bits)
 *   Bits 50-64: CRC-15 (integrity check over bits 1-49)
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef ZAKO_BITPADS_SESSION_H
#define ZAKO_BITPADS_SESSION_H

#include <stddef.h>
#include <stdint.h>
#include "zako_bitpads_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * LAYER 1 CONSTANTS
 * ======================================================================== */

#define ZBP_L1_SIZE          8u   /* 64 bits = 8 bytes */
#define ZBP_L1_CRC_POLY     0x4003u  /* x^15 + x + 1 = 0b100000000000011 */
#define ZBP_L1_CRC_BITS     15u

/* Domain codes (Layer 1 bits 3-4) */
#define ZBP_L1_DOMAIN_GENERAL    0u
#define ZBP_L1_DOMAIN_FINANCIAL  1u
#define ZBP_L1_DOMAIN_CONTROL    2u
#define ZBP_L1_DOMAIN_EXTENDED   3u

/* Permission flags (Layer 1 bits 5-8) */
#define ZBP_L1_PERM_WRITE       0x08u  /* bit 5 */
#define ZBP_L1_PERM_DELEGATE    0x04u  /* bit 6 */
#define ZBP_L1_PERM_COMPOUND    0x02u  /* bit 7 */
#define ZBP_L1_PERM_ADMIN       0x01u  /* bit 8 */

/* ========================================================================
 * META BYTE 2 CONSTANTS
 * ======================================================================== */

/* Time Reference Selector (Meta Byte 2, bits 5-6) */
#define ZBP_TIME_REF_NONE            0u  /* No time reference */
#define ZBP_TIME_REF_SESSION_OFFSET  1u  /* Tier 1: 8-bit session offset */
#define ZBP_TIME_REF_EXTERNAL        2u  /* Tier 1: 8-bit external offset */
#define ZBP_TIME_REF_TIER2           3u  /* Tier 2: full time block */

/* ========================================================================
 * SETUP BYTE CONSTANTS
 * ======================================================================== */

/* Value Tier (Setup Byte bits 1-2) */
#define ZBP_VALUE_TIER1   0u  /* 8-bit, max 255 */
#define ZBP_VALUE_TIER2   1u  /* 16-bit, max 65535 */
#define ZBP_VALUE_TIER3   2u  /* 24-bit, max 16777215 (DEFAULT) */
#define ZBP_VALUE_TIER4   3u  /* 32-bit, max 4294967295 */

/* Scaling Factor (Setup Byte bits 3-4) */
#define ZBP_SCALE_X1      0u
#define ZBP_SCALE_X10     1u
#define ZBP_SCALE_X100    2u
#define ZBP_SCALE_X1000   3u

/* Decimal Position (Setup Byte bits 5-6) */
#define ZBP_DECIMAL_0     0u  /* Integer */
#define ZBP_DECIMAL_1     1u  /* 1 decimal place */
#define ZBP_DECIMAL_2     2u  /* 2 decimal places */
#define ZBP_DECIMAL_3     3u  /* 3 decimal places */

/* Rounding (Setup Byte bit 8) */
#define ZBP_ROUND_HALF_UP   0u
#define ZBP_ROUND_HALF_EVEN 1u  /* Banker's rounding */

/* ========================================================================
 * TASK BYTE CONSTANTS
 * ======================================================================== */

#define ZBP_TASK_EXECUTE      0x00u
#define ZBP_TASK_ACKNOWLEDGE  0x01u
#define ZBP_TASK_REQUEST      0x02u
#define ZBP_TASK_CANCEL       0x03u
#define ZBP_TASK_SCHEDULE     0x04u
#define ZBP_TASK_DELEGATE     0x05u
#define ZBP_TASK_MONITOR      0x06u
#define ZBP_TASK_ALERT        0x07u
#define ZBP_TASK_APPROVE      0x08u
#define ZBP_TASK_REJECT       0x09u
#define ZBP_TASK_TRANSFER     0x0Au
#define ZBP_TASK_HOLD         0x0Bu
#define ZBP_TASK_RESUME       0x0Cu
#define ZBP_TASK_CLOSE        0x0Du
#define ZBP_TASK_CORRECTION   0x0Eu
#define ZBP_TASK_EXTENDED     0x0Fu

/* Task priority (bits 5-6) */
#define ZBP_TASK_PRI_NORMAL    0u
#define ZBP_TASK_PRI_ELEVATED  1u
#define ZBP_TASK_PRI_HIGH      2u
#define ZBP_TASK_PRI_CRITICAL  3u

/* ========================================================================
 * NOTE HEADER CONSTANTS
 * ======================================================================== */

/* Encoding type (bits 1-2) */
#define ZBP_NOTE_UTF8        0u
#define ZBP_NOTE_PICTOGRAPHY 1u
#define ZBP_NOTE_BINARY      2u
#define ZBP_NOTE_PROFILE_DEF 3u

/* ========================================================================
 * DECODED STRUCTURES
 * ======================================================================== */

/* Meta Byte 2 decoded */
typedef struct {
    uint8_t raw;
    uint8_t archetype;       /* bits 1-4: archetype / ext flags */
    uint8_t time_ref_sel;    /* bits 5-6: time reference selector */
    uint8_t setup_present;   /* bit 7: Setup Byte follows */
    uint8_t sigslot_present; /* bit 8: Signal Slot Presence Byte follows (v2.0) */
} zbp_meta2_t;

/* Layer 1 decoded */
typedef struct {
    uint8_t  raw[ZBP_L1_SIZE]; /* Raw 8 bytes */
    uint8_t  soh;              /* bit 1: always 1 */
    uint8_t  wire_version;     /* bit 2: 0=v2.0 */
    uint8_t  domain;           /* bits 3-4 */
    uint8_t  permissions;      /* bits 5-8 (4 flags packed) */
    uint8_t  split_order;      /* bit 9: 0=MSB, 1=LSB */
    uint8_t  sender_split;     /* bits 10-11 */
    uint8_t  session_enh;      /* bit 12: session enhancement flag (v2.0) */
    uint32_t sender_id;        /* bits 13-44 (32 bits) */
    uint8_t  sub_entity;       /* bits 45-49 (5 bits) */
    uint16_t crc15;            /* bits 50-64 (15 bits) */
    uint16_t crc15_computed;   /* independently computed for verification */
    uint8_t  crc_valid;        /* 1 if crc15 == crc15_computed */
} zbp_layer1_t;

/* Setup Byte decoded */
typedef struct {
    uint8_t raw;
    uint8_t value_tier;     /* bits 1-2: 0-3 */
    uint8_t scaling;        /* bits 3-4: 0-3 */
    uint8_t decimal_pos;    /* bits 5-6: 0-3 */
    uint8_t context_src;    /* bit 7: 0=inline, 1=inherit */
    uint8_t rounding;       /* bit 8: 0=half-up, 1=half-even */
} zbp_setup_t;

/* Task Byte decoded */
typedef struct {
    uint8_t raw;
    uint8_t category;       /* bits 1-4: task category (0-15) */
    uint8_t priority;       /* bits 5-6: priority level */
    uint8_t target_spec;    /* bit 7: target identity follows */
    uint8_t timing_spec;    /* bit 8: timing byte follows */
} zbp_task_t;

/* Note Header decoded */
typedef struct {
    uint8_t raw;
    uint8_t encoding;       /* bits 1-2: encoding type */
    uint8_t codebook;       /* bits 3-4: language/codebook */
    uint8_t length_field;   /* bits 5-8: length or length-mode indicator */
    size_t  body_length;    /* resolved actual body length in bytes */
} zbp_note_header_t;

/* Time Block Header (Tier 2) decoded */
typedef struct {
    uint8_t raw;
    uint8_t format;         /* bits 1-2: timestamp format */
    uint8_t resolution;     /* bits 3-4: time resolution */
    uint8_t tz_present;     /* bit 5: timezone offset follows */
    uint8_t dur_present;    /* bit 6: duration field follows */
} zbp_time_header_t;

/* ========================================================================
 * PUBLIC API — CRC-15
 * ======================================================================== */

/*
 * zbp_crc15 — Compute CRC-15 over a bit stream.
 *
 * Polynomial: x^15 + x + 1 (0x4003).
 * Used to protect Layer 1 bits 1-49.
 *
 * @param data     Byte array containing the bit stream
 * @param num_bits Number of bits to process (not bytes)
 * @return 15-bit CRC value
 */
uint16_t zbp_crc15(const uint8_t *data, size_t num_bits);

/* ========================================================================
 * PUBLIC API — META BYTE 2
 * ======================================================================== */

int zbp_meta2_decode(uint8_t byte, zbp_meta2_t *out);

uint8_t zbp_meta2_encode(uint8_t archetype, uint8_t time_ref_sel,
                         uint8_t setup_present, uint8_t sigslot_present);

/* ========================================================================
 * PUBLIC API — LAYER 1
 * ======================================================================== */

/*
 * zbp_layer1_decode — Decode 8 bytes into Layer 1 fields.
 *
 * Computes CRC-15 over bits 1-49 and compares to bits 50-64.
 * Sets crc_valid = 1 if they match.
 *
 * @param data    Input buffer (exactly 8 bytes)
 * @param out     Output structure
 * @return ZBP_OK on success, ZBP_ERR_NULL if NULL args
 */
int zbp_layer1_decode(const uint8_t data[ZBP_L1_SIZE], zbp_layer1_t *out);

/*
 * zbp_layer1_encode — Encode Layer 1 fields into 8 bytes.
 *
 * Computes and inserts CRC-15 automatically.
 *
 * @param domain       Domain code (0-3)
 * @param permissions  4-bit permission flags
 * @param split_order  0=MSB, 1=LSB
 * @param sender_split Sender ID split mode (0-3)
 * @param session_enh  Session enhancement flag
 * @param sender_id    32-bit sender identity
 * @param sub_entity   5-bit sub-entity (0-31)
 * @param out          Output buffer (8 bytes)
 * @return ZBP_OK on success
 */
int zbp_layer1_encode(uint8_t domain, uint8_t permissions,
                      uint8_t split_order, uint8_t sender_split,
                      uint8_t session_enh, uint32_t sender_id,
                      uint8_t sub_entity, uint8_t out[ZBP_L1_SIZE]);

/* ========================================================================
 * PUBLIC API — SETUP BYTE
 * ======================================================================== */

int zbp_setup_decode(uint8_t byte, zbp_setup_t *out);

uint8_t zbp_setup_encode(uint8_t tier, uint8_t scaling,
                         uint8_t decimal_pos, uint8_t context_src,
                         uint8_t rounding);

/*
 * zbp_value_tier_bytes — Number of bytes for a given value tier.
 */
size_t zbp_value_tier_bytes(uint8_t tier);

/* ========================================================================
 * PUBLIC API — VALUE BLOCK
 * ======================================================================== */

/*
 * zbp_value_encode — Encode an integer N into tier-sized bytes (big-endian).
 *
 * @param n         The integer value to encode
 * @param tier      Value tier (ZBP_VALUE_TIER1..TIER4)
 * @param out       Output buffer (1-4 bytes depending on tier)
 * @param out_len   Output: number of bytes written
 * @return ZBP_OK, ZBP_ERR_SIZE if N exceeds tier capacity
 */
int zbp_value_encode(uint32_t n, uint8_t tier,
                     uint8_t *out, size_t *out_len);

/*
 * zbp_value_decode — Decode tier-sized bytes into integer N.
 *
 * @param data      Input bytes (big-endian)
 * @param tier      Value tier
 * @param out_n     Output: decoded integer
 * @return ZBP_OK on success
 */
int zbp_value_decode(const uint8_t *data, uint8_t tier, uint32_t *out_n);

/* ========================================================================
 * PUBLIC API — TASK BYTE
 * ======================================================================== */

int zbp_task_decode(uint8_t byte, zbp_task_t *out);

uint8_t zbp_task_encode(uint8_t category, uint8_t priority,
                        uint8_t target_spec, uint8_t timing_spec);

/* ========================================================================
 * PUBLIC API — NOTE HEADER
 * ======================================================================== */

int zbp_note_header_decode(uint8_t byte, zbp_note_header_t *out);

uint8_t zbp_note_header_encode(uint8_t encoding, uint8_t codebook,
                               uint8_t length_field);

/* ========================================================================
 * PUBLIC API — TIME (TIER 2 HEADER)
 * ======================================================================== */

int zbp_time_header_decode(uint8_t byte, zbp_time_header_t *out);

uint8_t zbp_time_header_encode(uint8_t format, uint8_t resolution,
                               uint8_t tz_present, uint8_t dur_present);

#ifdef __cplusplus
}
#endif

#endif /* ZAKO_BITPADS_SESSION_H */
