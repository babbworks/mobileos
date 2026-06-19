/*
 * zako_pictography.h — 4-Bit Symbol Codec for ZAKO OS
 *
 * Implements the ZAKO Pictography Standard v1.0:
 *   - 4-bit symbol encoding/decoding (packed 2 per byte, MSB first)
 *   - Codebook management (16 codebooks, 16 symbols each)
 *   - Context Declaration Wave generation/parsing
 *   - Symbol sequence encode/decode (max 8 symbols / 4 bytes)
 *   - ALERT promotion (symbol 0xF triggers priority escalation)
 *   - Codebook lookup (symbol → name, symbol → meaning)
 *
 * Symbol encoding:
 *   One byte = two 4-bit symbols: [high nibble][low nibble]
 *   Max sequence: 8 symbols = 4 bytes (FIXED protocol constant)
 *
 * Codebooks defined:
 *   0x00: Null (no pictography)
 *   0x01: Exchange Domain
 *   0x02: Work / PADS Domain
 *   0x03: Health Domain
 *   0x04: Academy Domain
 *   0x05–0x08: Location, Finance, Energy, Social (reserved)
 *   0x09–0x0E: Reserved
 *   0x0F: Core ZAKO (always available)
 *
 * MISRA-C:2012 compliance:
 * - No dynamic allocation
 * - All functions return explicit error codes
 * - Fixed-size buffers
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef ZAKO_PICTOGRAPHY_H
#define ZAKO_PICTOGRAPHY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

#define ZPI_SYMBOL_BITS       4u
#define ZPI_SYMBOLS_PER_BYTE  2u
#define ZPI_MAX_SEQUENCE      8u    /* Max symbols in a sequence */
#define ZPI_MAX_SEQ_BYTES     4u    /* 8 symbols / 2 per byte = 4 bytes */
#define ZPI_CODEBOOK_SIZE     16u   /* Symbols per codebook */
#define ZPI_CODEBOOK_COUNT    16u   /* Number of codebook slots */
#define ZPI_NAME_MAX          16u   /* Max symbol name length */

/* Context Declaration Wave size */
#define ZPI_CONTEXT_WAVE_SIZE 4u

/* Codebook IDs */
#define ZPI_CB_NULL           0x00u
#define ZPI_CB_EXCHANGE       0x01u
#define ZPI_CB_WORK           0x02u
#define ZPI_CB_HEALTH         0x03u
#define ZPI_CB_ACADEMY        0x04u
#define ZPI_CB_LOCATION       0x05u
#define ZPI_CB_CORE           0x0Fu  /* Core ZAKO — always available */

/* Core codebook symbols (0x0F) */
#define ZPI_SYM_FULL          0x0u
#define ZPI_SYM_NORMAL        0x1u
#define ZPI_SYM_CONSERVE      0x2u
#define ZPI_SYM_CRITICAL      0x3u
#define ZPI_SYM_EMERGENCY     0x4u
#define ZPI_SYM_GRANT         0x5u
#define ZPI_SYM_REVOKE        0x6u
#define ZPI_SYM_JOIN          0x7u
#define ZPI_SYM_LEAVE         0x8u
#define ZPI_SYM_COMMIT        0x9u
#define ZPI_SYM_AMEND         0xAu
#define ZPI_SYM_DISPUTE       0xBu
#define ZPI_SYM_ACK           0xCu
#define ZPI_SYM_SUSPEND       0xDu
#define ZPI_SYM_RESUME        0xEu
#define ZPI_SYM_ALERT         0xFu  /* Always promotes to INTERACTIVE priority */

/* Error codes */
#define ZPI_OK                0
#define ZPI_ERR_NULL         (-1)
#define ZPI_ERR_SIZE         (-2)   /* Sequence too long */
#define ZPI_ERR_INVALID      (-3)   /* Invalid codebook or symbol */
#define ZPI_ERR_FORMAT       (-4)   /* Bad Context Declaration Wave */

/* ========================================================================
 * STRUCTURES
 * ======================================================================== */

/* Symbol sequence (decoded) */
typedef struct {
    uint8_t symbols[ZPI_MAX_SEQUENCE];
    uint8_t count;       /* Number of symbols (1-8) */
    uint8_t codebook;    /* Active codebook at time of decode */
    uint8_t has_alert;   /* 1 if any symbol == 0xF (ALERT promotion) */
} zpi_sequence_t;

/* Context Declaration Wave (decoded) */
typedef struct {
    uint8_t raw[ZPI_CONTEXT_WAVE_SIZE];
    uint8_t codebook_id;  /* 4-bit category identity */
    uint8_t version;      /* 4-bit version */
    uint8_t checksum;     /* XOR of bytes 0-2 */
    uint8_t valid;        /* 1 if checksum matches */
} zpi_context_t;

/* ========================================================================
 * PUBLIC API — SYMBOL ENCODING
 * ======================================================================== */

/*
 * zpi_pack — Pack symbols into bytes (2 per byte, MSB first).
 *
 * @param symbols    Array of 4-bit symbol values (0-15)
 * @param count      Number of symbols (1-8)
 * @param out_bytes  Output byte buffer (ceil(count/2) bytes written)
 * @param out_len    Output: number of bytes written
 * @return ZPI_OK on success
 */
int zpi_pack(const uint8_t *symbols, uint8_t count,
             uint8_t *out_bytes, size_t *out_len);

/*
 * zpi_unpack — Unpack bytes into individual 4-bit symbols.
 *
 * @param bytes      Packed symbol bytes
 * @param byte_count Number of input bytes
 * @param sym_count  Expected number of symbols (caller must know from context)
 * @param out        Output sequence
 * @return ZPI_OK on success
 */
int zpi_unpack(const uint8_t *bytes, size_t byte_count,
               uint8_t sym_count, zpi_sequence_t *out);

/* ========================================================================
 * PUBLIC API — CONTEXT DECLARATION
 * ======================================================================== */

/*
 * zpi_context_encode — Generate a Context Declaration Wave frame.
 *
 * @param codebook_id  Category Identity (0-15)
 * @param version      Codebook version (0-15, typically 1)
 * @param out          Output buffer (exactly 4 bytes)
 * @return ZPI_OK on success
 */
int zpi_context_encode(uint8_t codebook_id, uint8_t version,
                       uint8_t out[ZPI_CONTEXT_WAVE_SIZE]);

/*
 * zpi_context_decode — Parse a Context Declaration Wave frame.
 *
 * @param data  Input buffer (exactly 4 bytes)
 * @param out   Output structure
 * @return ZPI_OK if valid, ZPI_ERR_FORMAT if checksum fails
 */
int zpi_context_decode(const uint8_t data[ZPI_CONTEXT_WAVE_SIZE],
                       zpi_context_t *out);

/* ========================================================================
 * PUBLIC API — CODEBOOK LOOKUP
 * ======================================================================== */

/*
 * zpi_symbol_name — Get the name of a symbol in a codebook.
 *
 * @param codebook_id  Codebook (0x00-0x0F)
 * @param symbol       Symbol value (0x0-0xF)
 * @return Static string name, or "UNKNOWN"
 */
const char *zpi_symbol_name(uint8_t codebook_id, uint8_t symbol);

/*
 * zpi_is_alert — Check if a symbol triggers ALERT promotion.
 *
 * Symbol 0xF in ANY codebook is defined as ALERT.
 */
int zpi_is_alert(uint8_t symbol);

/*
 * zpi_sequence_has_alert — Check if any symbol in a sequence is ALERT.
 */
int zpi_sequence_has_alert(const zpi_sequence_t *seq);

#ifdef __cplusplus
}
#endif

#endif /* ZAKO_PICTOGRAPHY_H */
