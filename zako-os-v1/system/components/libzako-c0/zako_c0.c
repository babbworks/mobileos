/*
 * zako_c0.c — C0 Enhancement Grammar Implementation
 *
 * Enhanced C0 byte codec, Signal Slot Presence, lookup tables.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "zako_c0.h"

/* ========================================================================
 * ENHANCED C0 BYTE — DECODE / ENCODE
 * ======================================================================== */

int zc0_decode(uint8_t byte, zc0_enhanced_t *out)
{
    if (out == NULL) {
        return ZC0_ERR_NULL;
    }

    out->raw      = byte;
    out->code     = (uint8_t)(byte & ZC0_CODE_MASK);
    out->priority = (uint8_t)((byte & ZC0_FLAG_P) ? 1u : 0u);
    out->ack_req  = (uint8_t)((byte & ZC0_FLAG_A) ? 1u : 0u);
    out->cont     = (uint8_t)((byte & ZC0_FLAG_C) ? 1u : 0u);
    out->flags    = (uint8_t)(byte & ZC0_FLAGS_MASK);

    return ZC0_OK;
}

uint8_t zc0_encode(uint8_t code, uint8_t p, uint8_t a, uint8_t c)
{
    uint8_t byte = (uint8_t)(code & ZC0_CODE_MASK);

    if (p) { byte |= ZC0_FLAG_P; }
    if (a) { byte |= ZC0_FLAG_A; }
    if (c) { byte |= ZC0_FLAG_C; }

    return byte;
}

/* ========================================================================
 * SIGNAL SLOT PRESENCE BYTE
 *
 * Layout (MSB = P1, LSB = P8):
 *   Bit 8: P1 active (session open)
 *   Bit 7: P2 active (after L1)
 *   Bit 6: P3 active (before L2)
 *   Bit 5: P4 active (after L2)
 *   Bit 4: P5 active (before record)
 *   Bit 3: P6 active (after record)
 *   Bit 2: P7 active (before Wave)
 *   Bit 1: P8 active (after Wave)
 * ======================================================================== */

int zc0_ssp_decode(uint8_t byte, zc0_ssp_t *out)
{
    if (out == NULL) {
        return ZC0_ERR_NULL;
    }

    out->raw = byte;
    out->p1 = (uint8_t)((byte >> 7u) & 1u);
    out->p2 = (uint8_t)((byte >> 6u) & 1u);
    out->p3 = (uint8_t)((byte >> 5u) & 1u);
    out->p4 = (uint8_t)((byte >> 4u) & 1u);
    out->p5 = (uint8_t)((byte >> 3u) & 1u);
    out->p6 = (uint8_t)((byte >> 2u) & 1u);
    out->p7 = (uint8_t)((byte >> 1u) & 1u);
    out->p8 = (uint8_t)(byte & 1u);

    return ZC0_OK;
}

uint8_t zc0_ssp_encode(uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4,
                       uint8_t p5, uint8_t p6, uint8_t p7, uint8_t p8)
{
    uint8_t byte = 0u;

    if (p1) { byte |= 0x80u; }
    if (p2) { byte |= 0x40u; }
    if (p3) { byte |= 0x20u; }
    if (p4) { byte |= 0x10u; }
    if (p5) { byte |= 0x08u; }
    if (p6) { byte |= 0x04u; }
    if (p7) { byte |= 0x02u; }
    if (p8) { byte |= 0x01u; }

    return byte;
}

int zc0_ssp_is_active(uint8_t ssp_byte, uint8_t slot)
{
    if (slot < 1u || slot > 8u) {
        return 0;
    }
    /* Slot 1 is bit 8 (MSB), slot 8 is bit 1 (LSB) */
    return (ssp_byte >> (8u - slot)) & 1u;
}

/* ========================================================================
 * LOOKUP TABLES
 * ======================================================================== */

static const char *c0_names[32] = {
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
    "BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US"
};

const char *zc0_code_name(uint8_t code)
{
    if (code > 31u) { return "UNKNOWN"; }
    return c0_names[code];
}

uint8_t zc0_code_verdict(uint8_t code)
{
    /*
     * Per Enhancement Grammar §4:
     * CORE: SOH(1), STX(2), ETX(3), EOT(4), ENQ(5), ACK(6), NAK(21)
     * CONDITIONAL: SO(14), SI(15), ESC(27), DEL(127 — not in C0)
     *   → SO(14), SI(15), ESC(27) are the 3 conditional C0 codes
     *   → The spec says 4 conditional but the 4th is context-dependent
     *     interpretation of DC1-DC4 as a group. We mark SO/SI/ESC.
     * UNCONDITIONAL: everything else
     */
    switch (code) {
        case ZC0_SOH:
        case ZC0_STX:
        case ZC0_ETX:
        case ZC0_EOT:
        case ZC0_ENQ:
        case ZC0_ACK:
        case ZC0_NAK:
            return ZC0_VERDICT_CORE;

        case ZC0_SO:
        case ZC0_SI:
        case ZC0_ESC:
            return ZC0_VERDICT_CONDITIONAL;

        default:
            return ZC0_VERDICT_UNCONDITIONAL;
    }
}

/* Signal slot table */
static const struct {
    const char *layer;
    const char *position;
} slot_table[13] = {
    { "Session", "Before Layer 1 (session open)" },       /* P1 */
    { "Session", "After Layer 1" },                        /* P2 */
    { "Batch",   "Before Layer 2" },                       /* P3 */
    { "Batch",   "After Layer 2" },                        /* P4 */
    { "Record",  "Before record body" },                   /* P5 */
    { "Record",  "After record body" },                    /* P6 */
    { "Wave",    "Before Wave content" },                  /* P7 */
    { "Wave",    "After Wave content" },                   /* P8 */
    { "Stream",  "Stream open" },                          /* P9 */
    { "Stream",  "Stream close" },                         /* P10 */
    { "Record",  "Component boundary" },                   /* P11 */
    { "Batch",   "Batch close" },                          /* P12 */
    { "Session", "Session close" },                        /* P13 */
};

int zc0_slot_info(uint8_t slot, zc0_slot_info_t *out)
{
    if (out == NULL) {
        return ZC0_ERR_NULL;
    }
    if (slot < 1u || slot > 13u) {
        return ZC0_ERR_INVALID;
    }

    out->slot_id  = slot;
    out->layer    = slot_table[slot - 1u].layer;
    out->position = slot_table[slot - 1u].position;

    return ZC0_OK;
}

const char *zc0_slot_layer(uint8_t slot)
{
    if (slot < 1u || slot > 13u) { return "UNKNOWN"; }
    return slot_table[slot - 1u].layer;
}
