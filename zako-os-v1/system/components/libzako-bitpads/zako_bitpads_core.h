/*
 * zako_bitpads_core.h — BitPads v2.0 Core Codec (Meta Byte 1 + Wave Mode)
 *
 * Implements the foundational parsing layer of the BitPads Protocol v2.0:
 *   - Meta Byte 1 decode/encode (8-bit universal header)
 *   - Mode dispatch (Wave vs Record)
 *   - Wave Role A (Basic Treatment — 4 boolean flags)
 *   - Wave Role B (Category Mode — 16 categories + extended)
 *   - Pure Signal (1-byte minimal transmission)
 *   - Frame type detection and minimum size calculation
 *
 * Meta Byte 1 layout (BitPads v2.0 normative):
 *   Bit 1: Mode — 0=Wave, 1=Record
 *   Bit 2: ACK Request (Wave) / SysCtx Present (Record)
 *   Bit 3: Continuation — 0=complete, 1=fragment (more follows)
 *   Bit 4: Treatment Switch (Wave only) — 0=Role A (Basic), 1=Role B (Category)
 *          In Record mode: reserved (must be 0)
 *   Bits 5-8: Content field (role-dependent):
 *     Role A: bit5=Priority, bit6=Cipher, bit7=ExtFlags, bit8=Profile
 *     Role B: 4-bit category code (0000-1111)
 *     Role C: bit5=Value, bit6=Time, bit7=Task, bit8=Note (Record mode)
 *
 * Bit numbering: bit 1 = MSB (0x80), bit 8 = LSB (0x01)
 * This matches the spec's numbering convention.
 *
 * MISRA-C:2012 compliance:
 * - No dynamic allocation
 * - All functions return explicit error codes or values
 * - No recursion
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef ZAKO_BITPADS_CORE_H
#define ZAKO_BITPADS_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * BIT POSITION MASKS — spec uses bit 1 = MSB, bit 8 = LSB
 * ======================================================================== */

#define ZBP_BIT1  0x80u  /* Mode: 0=Wave, 1=Record */
#define ZBP_BIT2  0x40u  /* ACK Request / SysCtx */
#define ZBP_BIT3  0x20u  /* Continuation */
#define ZBP_BIT4  0x10u  /* Treatment Switch (Wave) / Reserved (Record) */
#define ZBP_BIT5  0x08u  /* Content field MSB */
#define ZBP_BIT6  0x04u  /* Content field */
#define ZBP_BIT7  0x02u  /* Content field */
#define ZBP_BIT8  0x01u  /* Content field LSB */

/* Content field mask (bits 5-8) */
#define ZBP_CONTENT_MASK  0x0Fu  /* bits 5-8 */

/* ========================================================================
 * MODE CONSTANTS
 * ======================================================================== */

#define ZBP_MODE_WAVE    0u
#define ZBP_MODE_RECORD  1u

/* ========================================================================
 * WAVE ROLE A FLAGS (bit 4 = 0, Basic Treatment)
 * Bits 5-8 are independent boolean flags
 * ======================================================================== */

#define ZBP_ROLE_A_PRIORITY   ZBP_BIT5  /* 0=normal, 1=high priority */
#define ZBP_ROLE_A_CIPHER     ZBP_BIT6  /* 0=plaintext, 1=encrypted (v2.0) */
#define ZBP_ROLE_A_EXT_FLAGS  ZBP_BIT7  /* 0=no ext, 1=flag byte follows */
#define ZBP_ROLE_A_PROFILE    ZBP_BIT8  /* 0=standard, 1=profile-defined */

/* ========================================================================
 * WAVE ROLE B CATEGORIES (bit 4 = 1, Category Mode)
 * Bits 5-8 form a 4-bit category code
 * ======================================================================== */

#define ZBP_CAT_PURE_SIGNAL       0x00u  /* 0000 — heartbeat, keepalive */
#define ZBP_CAT_STATUS_REPORT     0x01u  /* 0001 — device/system status */
#define ZBP_CAT_COMMAND           0x02u  /* 0010 — instruction to receiver */
#define ZBP_CAT_QUERY             0x03u  /* 0011 — request for data */
#define ZBP_CAT_ALERT             0x04u  /* 0100 — time-sensitive alert */
#define ZBP_CAT_ACK               0x05u  /* 0101 — positive acknowledgement */
#define ZBP_CAT_NACK              0x06u  /* 0110 — negative acknowledgement */
#define ZBP_CAT_SYNC              0x07u  /* 0111 — clock sync / time ref */
#define ZBP_CAT_DISCOVERY         0x08u  /* 1000 — presence broadcast */
#define ZBP_CAT_DATA_TRANSFER     0x09u  /* 1001 — structured data payload */
#define ZBP_CAT_CONFIGURATION     0x0Au  /* 1010 — config push */
#define ZBP_CAT_DIAGNOSTIC        0x0Bu  /* 1011 — debug/analysis payload */
#define ZBP_CAT_FINANCIAL         0x0Cu  /* 1100 — financial signal (v2.0) */
#define ZBP_CAT_IDENTITY          0x0Du  /* 1101 — identity assertion (v2.0) */
#define ZBP_CAT_CONTROL           0x0Eu  /* 1110 — protocol control (v2.0) */
#define ZBP_CAT_EXTENDED          0x0Fu  /* 1111 — ext category byte follows */

/* ========================================================================
 * ROLE C COMPONENT FLAGS (Record mode, bits 5-8)
 * ======================================================================== */

#define ZBP_COMP_VALUE   ZBP_BIT5  /* Value block present */
#define ZBP_COMP_TIME    ZBP_BIT6  /* Time field present */
#define ZBP_COMP_TASK    ZBP_BIT7  /* Task block present */
#define ZBP_COMP_NOTE    ZBP_BIT8  /* Note block present */

/* ========================================================================
 * LAYER 1 REQUIREMENT FOR WAVE CATEGORIES
 * ======================================================================== */

#define ZBP_L1_NOT_REQUIRED  0u
#define ZBP_L1_OPTIONAL      1u
#define ZBP_L1_RECOMMENDED   2u
#define ZBP_L1_REQUIRED      3u

/* ========================================================================
 * ERROR CODES
 * ======================================================================== */

#define ZBP_OK              0
#define ZBP_ERR_NULL       (-1)
#define ZBP_ERR_SIZE       (-2)
#define ZBP_ERR_INVALID    (-3)  /* Invalid field value or combination */
#define ZBP_ERR_TRUNCATED  (-4)  /* Buffer too short */

/* ========================================================================
 * DECODED META BYTE 1 STRUCTURE
 * ======================================================================== */

typedef struct {
    uint8_t raw;            /* Original byte value */

    /* Bit fields (decoded) */
    uint8_t mode;           /* 0=Wave, 1=Record */
    uint8_t ack_sysctx;     /* Wave: ACK request. Record: SysCtx present */
    uint8_t continuation;   /* 0=complete/final, 1=fragment continues */
    uint8_t treatment;      /* Wave: 0=Role A, 1=Role B. Record: reserved */

    /* Content interpretation (depends on mode + treatment) */
    uint8_t content;        /* Raw 4-bit content field (bits 5-8) */

    /* Role A flags (valid only when mode=Wave, treatment=0) */
    uint8_t priority;       /* bit 5: 0=normal, 1=high */
    uint8_t cipher;         /* bit 6: 0=plaintext, 1=encrypted */
    uint8_t ext_flags;      /* bit 7: 0=none, 1=ext byte follows */
    uint8_t profile_def;    /* bit 8: 0=standard, 1=profile-defined */

    /* Role B category (valid only when mode=Wave, treatment=1) */
    uint8_t category;       /* 4-bit category code (0-15) */

    /* Role C component flags (valid only when mode=Record) */
    uint8_t value_present;  /* bit 5 */
    uint8_t time_present;   /* bit 6 */
    uint8_t task_present;   /* bit 7 */
    uint8_t note_present;   /* bit 8 */
} zbp_meta1_t;

/* ========================================================================
 * PUBLIC API — META BYTE 1
 * ======================================================================== */

/*
 * zbp_meta1_decode — Parse Meta Byte 1 into all component fields.
 *
 * Decodes the universal 8-bit header according to the BitPads v2.0
 * decoder decision tree. All role-specific fields are populated;
 * the caller should only read the fields valid for the decoded mode.
 *
 * @param byte      The raw Meta Byte 1
 * @param out       Pointer to zbp_meta1_t structure to fill
 * @return ZBP_OK on success, ZBP_ERR_NULL if out is NULL
 */
int zbp_meta1_decode(uint8_t byte, zbp_meta1_t *out);

/*
 * zbp_meta1_encode_wave_a — Encode a Wave Mode / Role A meta byte.
 *
 * @param ack        1 to request acknowledgement, 0 otherwise
 * @param cont       1 if fragment continues, 0 if complete
 * @param priority   1 for high priority, 0 for normal
 * @param cipher     1 if encrypted, 0 if plaintext
 * @param ext_flags  1 if extension byte follows, 0 otherwise
 * @param profile    1 if profile-defined, 0 if standard
 * @return The encoded Meta Byte 1
 */
uint8_t zbp_meta1_encode_wave_a(uint8_t ack, uint8_t cont,
                                uint8_t priority, uint8_t cipher,
                                uint8_t ext_flags, uint8_t profile);

/*
 * zbp_meta1_encode_wave_b — Encode a Wave Mode / Role B meta byte.
 *
 * @param ack       1 to request acknowledgement, 0 otherwise
 * @param cont      1 if fragment continues, 0 if complete
 * @param category  4-bit category code (0-15, use ZBP_CAT_* constants)
 * @return The encoded Meta Byte 1
 */
uint8_t zbp_meta1_encode_wave_b(uint8_t ack, uint8_t cont,
                                uint8_t category);

/*
 * zbp_meta1_encode_record — Encode a Record Mode meta byte (Role C).
 *
 * @param sysctx    1 if System Context Extension follows Layer 1
 * @param cont      1 if fragment continues, 0 if complete
 * @param value     1 if Value block present
 * @param time      1 if Time field present
 * @param task      1 if Task block present
 * @param note      1 if Note block present
 * @return The encoded Meta Byte 1
 */
uint8_t zbp_meta1_encode_record(uint8_t sysctx, uint8_t cont,
                                uint8_t value, uint8_t time,
                                uint8_t task, uint8_t note);

/* ========================================================================
 * PUBLIC API — PURE SIGNAL
 * ======================================================================== */

/*
 * zbp_pure_signal_encode — Encode a 1-byte Pure Signal transmission.
 *
 * A Pure Signal is a Wave/Role B frame with category 0000 (heartbeat)
 * and no payload. The entire transmission is a single meta byte.
 *
 * @param ack       1 to request ACK response, 0 otherwise
 * @param out_byte  Output: the single byte
 * @return ZBP_OK, or ZBP_ERR_NULL
 */
int zbp_pure_signal_encode(uint8_t ack, uint8_t *out_byte);

/*
 * zbp_pure_signal_decode — Verify and decode a Pure Signal byte.
 *
 * @param byte      The candidate byte
 * @param out       Output: decoded meta (can be NULL for validation only)
 * @return ZBP_OK if valid pure signal, ZBP_ERR_INVALID if not
 */
int zbp_pure_signal_decode(uint8_t byte, zbp_meta1_t *out);

/* ========================================================================
 * PUBLIC API — FRAME UTILITIES
 * ======================================================================== */

/*
 * zbp_is_wave — Quick check: is this byte a Wave mode meta byte?
 */
int zbp_is_wave(uint8_t meta_byte);

/*
 * zbp_is_record — Quick check: is this byte a Record mode meta byte?
 */
int zbp_is_record(uint8_t meta_byte);

/*
 * zbp_category_l1_requirement — Layer 1 requirement for a wave category.
 *
 * @param category  4-bit category code (0-15)
 * @return ZBP_L1_NOT_REQUIRED, ZBP_L1_OPTIONAL, ZBP_L1_RECOMMENDED, or ZBP_L1_REQUIRED
 */
uint8_t zbp_category_l1_requirement(uint8_t category);

/*
 * zbp_wave_min_size — Minimum frame size for a wave transmission.
 *
 * Returns minimum byte count including meta byte and Layer 1 if required.
 * Does NOT include payload (category-dependent).
 *
 * @param meta_byte  The Wave mode meta byte
 * @return Minimum size in bytes (1 for pure signal, 9 for wave+L1)
 */
size_t zbp_wave_min_size(uint8_t meta_byte);

/*
 * zbp_record_min_size — Minimum frame size for a Record transmission.
 *
 * Always at least 10 bytes: Meta1(1) + Meta2(1) + Layer1(8).
 *
 * @return 10
 */
size_t zbp_record_min_size(void);

#ifdef __cplusplus
}
#endif

#endif /* ZAKO_BITPADS_CORE_H */
