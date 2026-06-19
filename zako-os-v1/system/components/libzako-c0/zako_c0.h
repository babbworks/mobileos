/*
 * zako_c0.h — C0 Enhancement Grammar for ZAKO OS
 *
 * Implements the BitPads Enhancement Sub-Protocol v2.0:
 *   - Enhanced C0 byte: 5+3 split (lower 5 = code, upper 3 = P·A·C flags)
 *   - 32 C0 control codes (29 unconditional + 4 conditional)
 *   - 8 flag combinations (2³ P·A·C matrix)
 *   - 13 signal slot positions (P1–P13) spanning session/batch/record/wave/stream
 *   - Signal Slot Presence Byte (P1–P8 bitmap, v2.0)
 *   - Slot presence extension (P9–P13)
 *
 * Enhanced byte layout:
 *   Bit 8 (MSB): P — Priority (0=normal, 1=elevated)
 *   Bit 7:       A — Acknowledge Request (0=no, 1=confirm required)
 *   Bit 6:       C — Continuation (0=complete, 1=more follows)
 *   Bits 5-1:    C0 code identity (0–31)
 *
 * Lineage: Baudot (1870) → Murray → ASCII → ISO 6429 → BitPads Enhancement.
 * The upper 3 bits have been structurally available since 1870.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef ZAKO_C0_H
#define ZAKO_C0_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * C0 CONTROL CODE CONSTANTS (lower 5 bits, 0–31)
 * ======================================================================== */

#define ZC0_NUL   0x00u  /* Null / filler / typed keep-alive */
#define ZC0_SOH   0x01u  /* Start of Heading — session open */
#define ZC0_STX   0x02u  /* Start of Text — content open */
#define ZC0_ETX   0x03u  /* End of Text — content close */
#define ZC0_EOT   0x04u  /* End of Transmission — session/batch end */
#define ZC0_ENQ   0x05u  /* Enquiry — status request / handshake */
#define ZC0_ACK   0x06u  /* Acknowledge — positive confirmation */
#define ZC0_BEL   0x07u  /* Bell — alert / priority notification */
#define ZC0_BS    0x08u  /* Backspace — correction / undo */
#define ZC0_HT    0x09u  /* Horizontal Tab — field separator */
#define ZC0_LF    0x0Au  /* Line Feed — batch boundary */
#define ZC0_VT    0x0Bu  /* Vertical Tab — layer boundary */
#define ZC0_FF    0x0Cu  /* Form Feed — session separator */
#define ZC0_CR    0x0Du  /* Carriage Return — record reset */
#define ZC0_SO    0x0Eu  /* Shift Out — codebook shift (CONDITIONAL) */
#define ZC0_SI    0x0Fu  /* Shift In — codebook restore (CONDITIONAL) */
#define ZC0_DLE   0x10u  /* Data Link Escape — extension prefix */
#define ZC0_DC1   0x11u  /* Device Control 1 (XON / start) */
#define ZC0_DC2   0x12u  /* Device Control 2 (set parameter) */
#define ZC0_DC3   0x13u  /* Device Control 3 (XOFF / pause) */
#define ZC0_DC4   0x14u  /* Device Control 4 (stop) */
#define ZC0_NAK   0x15u  /* Negative Acknowledge — rejection */
#define ZC0_SYN   0x16u  /* Synchronisation marker */
#define ZC0_ETB   0x17u  /* End of Transmission Block */
#define ZC0_CAN   0x18u  /* Cancel — discard current record */
#define ZC0_EM    0x19u  /* End of Medium — storage boundary */
#define ZC0_SUB   0x1Au  /* Substitute — replacement marker */
#define ZC0_ESC   0x1Bu  /* Escape — extension prefix (CONDITIONAL) */
#define ZC0_FS    0x1Cu  /* File Separator — top-level boundary */
#define ZC0_GS    0x1Du  /* Group Separator — group boundary */
#define ZC0_RS    0x1Eu  /* Record Separator — record boundary */
#define ZC0_US    0x1Fu  /* Unit Separator — component boundary */

/* ========================================================================
 * FLAG MASKS (upper 3 bits)
 * ======================================================================== */

#define ZC0_FLAG_P    0x80u  /* Bit 8: Priority */
#define ZC0_FLAG_A    0x40u  /* Bit 7: Acknowledge Request */
#define ZC0_FLAG_C    0x20u  /* Bit 6: Continuation */
#define ZC0_FLAGS_MASK 0xE0u /* All three flag bits */
#define ZC0_CODE_MASK  0x1Fu /* Lower 5 bits: C0 code identity */

/* Flag combination offsets (add to base C0 code) */
#define ZC0_PLAIN     0x00u  /* 000: no flags */
#define ZC0_CONT      0x20u  /* 001: continuation only */
#define ZC0_AREQ      0x40u  /* 010: acknowledge request */
#define ZC0_AREQ_CONT 0x60u  /* 011: ack + continuation */
#define ZC0_PRIO      0x80u  /* 100: priority only */
#define ZC0_PRIO_CONT 0xA0u  /* 101: priority + continuation */
#define ZC0_PRIO_AREQ 0xC0u  /* 110: priority + acknowledge */
#define ZC0_ALL_FLAGS 0xE0u  /* 111: priority + ack + continuation */

/* ========================================================================
 * SIGNAL SLOT POSITIONS (P1–P13)
 * ======================================================================== */

#define ZC0_SLOT_P1   1u   /* Session: before Layer 1 */
#define ZC0_SLOT_P2   2u   /* Session: after Layer 1 */
#define ZC0_SLOT_P3   3u   /* Batch: before Layer 2 */
#define ZC0_SLOT_P4   4u   /* Batch: after Layer 2 */
#define ZC0_SLOT_P5   5u   /* Record: before record body */
#define ZC0_SLOT_P6   6u   /* Record: after record body */
#define ZC0_SLOT_P7   7u   /* Wave: before Wave content */
#define ZC0_SLOT_P8   8u   /* Wave: after Wave content */
#define ZC0_SLOT_P9   9u   /* Stream: stream open */
#define ZC0_SLOT_P10  10u  /* Stream: stream close */
#define ZC0_SLOT_P11  11u  /* Record: component boundary */
#define ZC0_SLOT_P12  12u  /* Batch: batch close */
#define ZC0_SLOT_P13  13u  /* Session: session close */

#define ZC0_SLOT_COUNT 13u

/* ========================================================================
 * CONTROL VERDICT (inclusion status)
 * ======================================================================== */

#define ZC0_VERDICT_UNCONDITIONAL 0u  /* Always safe to use */
#define ZC0_VERDICT_CONDITIONAL   1u  /* Requires non-text channel typing */
#define ZC0_VERDICT_CORE          2u  /* Core protocol control */

/* ========================================================================
 * ERROR CODES
 * ======================================================================== */

#define ZC0_OK           0
#define ZC0_ERR_NULL    (-1)
#define ZC0_ERR_INVALID (-2)  /* Code > 31 or slot > 13 */

/* ========================================================================
 * DECODED STRUCTURES
 * ======================================================================== */

/* Decoded enhanced C0 byte */
typedef struct {
    uint8_t raw;        /* Original byte value */
    uint8_t code;       /* Lower 5 bits: C0 code (0–31) */
    uint8_t priority;   /* Bit 8: P flag */
    uint8_t ack_req;    /* Bit 7: A flag */
    uint8_t cont;       /* Bit 6: C flag */
    uint8_t flags;      /* Upper 3 bits combined (P·A·C) */
} zc0_enhanced_t;

/* Signal Slot Presence Byte decoded */
typedef struct {
    uint8_t raw;
    uint8_t p1;   /* Bit 8: P1 active (session open) */
    uint8_t p2;   /* Bit 7: P2 active (after L1) */
    uint8_t p3;   /* Bit 6: P3 active (before L2) */
    uint8_t p4;   /* Bit 5: P4 active (after L2) */
    uint8_t p5;   /* Bit 4: P5 active (before record) */
    uint8_t p6;   /* Bit 3: P6 active (after record) */
    uint8_t p7;   /* Bit 2: P7 active (before Wave) */
    uint8_t p8;   /* Bit 1: P8 active (after Wave) */
} zc0_ssp_t;

/* Signal slot info */
typedef struct {
    uint8_t     slot_id;     /* 1–13 */
    const char *layer;       /* "Session", "Batch", "Record", "Wave", "Stream" */
    const char *position;    /* Human-readable position description */
} zc0_slot_info_t;

/* ========================================================================
 * PUBLIC API — ENHANCED C0 BYTE
 * ======================================================================== */

/*
 * zc0_decode — Decode an enhanced C0 byte into code + flags.
 */
int zc0_decode(uint8_t byte, zc0_enhanced_t *out);

/*
 * zc0_encode — Encode a C0 code with P·A·C flags.
 *
 * @param code  C0 code (0–31)
 * @param p     Priority flag (0 or 1)
 * @param a     Acknowledge flag (0 or 1)
 * @param c     Continuation flag (0 or 1)
 * @return The encoded byte
 */
uint8_t zc0_encode(uint8_t code, uint8_t p, uint8_t a, uint8_t c);

/* ========================================================================
 * PUBLIC API — SIGNAL SLOT PRESENCE BYTE
 * ======================================================================== */

/*
 * zc0_ssp_decode — Decode the Signal Slot Presence Byte.
 */
int zc0_ssp_decode(uint8_t byte, zc0_ssp_t *out);

/*
 * zc0_ssp_encode — Encode a Signal Slot Presence Byte from P1-P8 flags.
 */
uint8_t zc0_ssp_encode(uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4,
                       uint8_t p5, uint8_t p6, uint8_t p7, uint8_t p8);

/*
 * zc0_ssp_is_active — Check if a specific slot (1-8) is active in an SSP byte.
 *
 * @param ssp_byte  The Signal Slot Presence Byte
 * @param slot      Slot number (1-8)
 * @return 1 if active, 0 if not (or invalid slot)
 */
int zc0_ssp_is_active(uint8_t ssp_byte, uint8_t slot);

/* ========================================================================
 * PUBLIC API — LOOKUP TABLES
 * ======================================================================== */

/*
 * zc0_code_name — Name of a C0 control code.
 * @param code  0–31
 * @return Static string ("NUL", "SOH", ..., "US"), or "UNKNOWN"
 */
const char *zc0_code_name(uint8_t code);

/*
 * zc0_code_verdict — Inclusion verdict for a C0 code.
 * @param code  0–31
 * @return ZC0_VERDICT_UNCONDITIONAL, ZC0_VERDICT_CONDITIONAL, or ZC0_VERDICT_CORE
 */
uint8_t zc0_code_verdict(uint8_t code);

/*
 * zc0_slot_info — Get information about a signal slot.
 * @param slot  1–13
 * @param out   Output structure
 * @return ZC0_OK or ZC0_ERR_INVALID
 */
int zc0_slot_info(uint8_t slot, zc0_slot_info_t *out);

/*
 * zc0_slot_layer — Layer name for a slot.
 * @param slot  1–13
 * @return Static string or "UNKNOWN"
 */
const char *zc0_slot_layer(uint8_t slot);

#ifdef __cplusplus
}
#endif

#endif /* ZAKO_C0_H */
