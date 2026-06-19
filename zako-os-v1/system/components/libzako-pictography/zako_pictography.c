/*
 * zako_pictography.c — 4-Bit Symbol Codec Implementation
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "zako_pictography.h"
#include <string.h>

/* ========================================================================
 * CODEBOOK TABLES
 * ======================================================================== */

static const char *CORE_NAMES[16] = {
    "FULL", "NORMAL", "CONSERVE", "CRITICAL", "EMERGENCY",
    "GRANT", "REVOKE", "JOIN", "LEAVE", "COMMIT",
    "AMEND", "DISPUTE", "ACK", "SUSPEND", "RESUME", "ALERT"
};

static const char *EXCHANGE_NAMES[16] = {
    "SEND", "RECEIVE", "INVOICE", "PAY", "QUOTE",
    "AGREE", "COMPLETE", "RETURN", "HOLD", "RESUME",
    "CANCEL", "DISPUTE", "RETRY", "PENDING", "LOCKED", "ALERT"
};

static const char *WORK_NAMES[16] = {
    "ASSIGN", "START", "PROGRESS", "FINISH", "INSPECT",
    "REPORT", "EXPENSE", "TRAVEL", "OVERDUE", "DELEGATE",
    "BLOCK", "DELIVER", "ATTACH", "REJECT", "REVISE", "ALERT"
};

static const char *HEALTH_NAMES[16] = {
    "VITALS", "MEDICATE", "WEIGHT", "GLUCOSE", "SPO2",
    "TEMP", "STEPS", "SLEEP", "INTAKE", "MOOD",
    "REMIND", "HIGH", "LOW", "NORMAL", "TALLY", "ALERT"
};

static const char *ACADEMY_NAMES[16] = {
    "STUDY", "COMPLETE", "ASSESS", "MASTER", "REVISIT",
    "ASSIGN", "CERTIFY", "RECOMMEND", "COLLABORATE", "STREAK",
    "TIME", "GOAL", "BLOCK", "RESET", "PROGRESS", "ALERT"
};

/* ========================================================================
 * PUBLIC API — SYMBOL ENCODING
 * ======================================================================== */

int zpi_pack(const uint8_t *symbols, uint8_t count,
             uint8_t *out_bytes, size_t *out_len)
{
    size_t i;
    size_t byte_count;

    if (symbols == NULL || out_bytes == NULL || out_len == NULL) {
        return ZPI_ERR_NULL;
    }
    if (count == 0 || count > ZPI_MAX_SEQUENCE) {
        return ZPI_ERR_SIZE;
    }

    byte_count = ((size_t)count + 1u) / 2u;
    memset(out_bytes, 0, byte_count);

    for (i = 0; i < count; i++) {
        uint8_t sym = symbols[i] & 0x0Fu;
        size_t byte_idx = i / 2u;
        if ((i % 2u) == 0u) {
            /* High nibble */
            out_bytes[byte_idx] |= (uint8_t)(sym << 4u);
        } else {
            /* Low nibble */
            out_bytes[byte_idx] |= sym;
        }
    }

    *out_len = byte_count;
    return ZPI_OK;
}

int zpi_unpack(const uint8_t *bytes, size_t byte_count,
               uint8_t sym_count, zpi_sequence_t *out)
{
    size_t i;
    size_t needed_bytes;

    if (bytes == NULL || out == NULL) { return ZPI_ERR_NULL; }
    if (sym_count == 0 || sym_count > ZPI_MAX_SEQUENCE) { return ZPI_ERR_SIZE; }

    needed_bytes = ((size_t)sym_count + 1u) / 2u;
    if (byte_count < needed_bytes) { return ZPI_ERR_SIZE; }

    memset(out, 0, sizeof(*out));
    out->count = sym_count;
    out->has_alert = 0;

    for (i = 0; i < sym_count; i++) {
        size_t byte_idx = i / 2u;
        uint8_t sym;
        if ((i % 2u) == 0u) {
            sym = (bytes[byte_idx] >> 4u) & 0x0Fu;
        } else {
            sym = bytes[byte_idx] & 0x0Fu;
        }
        out->symbols[i] = sym;
        if (sym == ZPI_SYM_ALERT) {
            out->has_alert = 1;
        }
    }

    return ZPI_OK;
}

/* ========================================================================
 * PUBLIC API — CONTEXT DECLARATION
 * ======================================================================== */

int zpi_context_encode(uint8_t codebook_id, uint8_t version,
                       uint8_t out[ZPI_CONTEXT_WAVE_SIZE])
{
    if (out == NULL) { return ZPI_ERR_NULL; }

    /* Byte 0: Meta byte for Anonymous Wave (frame_type=01, priority=01, ack=0, cont=1)
     * Using a simplified encoding: 0x50 (Wave/RoleA, continuation=1, profile=0) */
    out[0] = 0x30u; /* Wave mode(0), ack=0, cont=1, treatment=1, cat=0000 → 0x30
                      * Actually let's use: mode=0, ack=0, cont=1(0x20), treatment=1(0x10), cat=0
                      * = 0x30 */

    /* Byte 1: [codebook_id:4][version:4] */
    out[1] = (uint8_t)(((codebook_id & 0x0Fu) << 4u) | (version & 0x0Fu));

    /* Byte 2: Reserved = 0x00 */
    out[2] = 0x00u;

    /* Byte 3: Checksum = XOR of bytes 0-2 */
    out[3] = out[0] ^ out[1] ^ out[2];

    return ZPI_OK;
}

int zpi_context_decode(const uint8_t data[ZPI_CONTEXT_WAVE_SIZE],
                       zpi_context_t *out)
{
    uint8_t computed_checksum;

    if (data == NULL || out == NULL) { return ZPI_ERR_NULL; }

    memcpy(out->raw, data, ZPI_CONTEXT_WAVE_SIZE);

    out->codebook_id = (data[1] >> 4u) & 0x0Fu;
    out->version = data[1] & 0x0Fu;
    out->checksum = data[3];

    computed_checksum = data[0] ^ data[1] ^ data[2];
    out->valid = (computed_checksum == data[3]) ? 1u : 0u;

    if (!out->valid) { return ZPI_ERR_FORMAT; }
    return ZPI_OK;
}

/* ========================================================================
 * PUBLIC API — CODEBOOK LOOKUP
 * ======================================================================== */

const char *zpi_symbol_name(uint8_t codebook_id, uint8_t symbol)
{
    if (symbol > 0x0Fu) { return "UNKNOWN"; }

    switch (codebook_id) {
    case ZPI_CB_CORE:     return CORE_NAMES[symbol];
    case ZPI_CB_EXCHANGE: return EXCHANGE_NAMES[symbol];
    case ZPI_CB_WORK:     return WORK_NAMES[symbol];
    case ZPI_CB_HEALTH:   return HEALTH_NAMES[symbol];
    case ZPI_CB_ACADEMY:  return ACADEMY_NAMES[symbol];
    default:              return "UNKNOWN";
    }
}

int zpi_is_alert(uint8_t symbol)
{
    return (symbol == ZPI_SYM_ALERT) ? 1 : 0;
}

int zpi_sequence_has_alert(const zpi_sequence_t *seq)
{
    if (seq == NULL) { return 0; }
    return seq->has_alert ? 1 : 0;
}
