/*
 * zako_padsurl.h — pads-v1 URL Codec for ZAKO OS
 *
 * Encodes BitPads/BitLedger frames as compact URL strings for SMS transport.
 * Target: <300 characters total, suitable for a single SMS segment.
 *
 * URL format:
 *   #1pa/<version><fields>
 *
 * Where fields are base64url-encoded binary:
 *   [flags(1)][value(3)][account_pair(1)][timestamp(4)][sender_id(4)]
 *   [optional: note_len(1)][note_bytes(N)]
 *   [optional: sig(64)]
 *
 * Flags byte layout:
 *   bit 7: direction (0=in, 1=out)
 *   bit 6: status (0=paid, 1=debt)
 *   bit 5: debit/credit (0=credit, 1=debit)
 *   bit 4: has_note (0=no, 1=note follows)
 *   bit 3: has_sig (0=unsigned, 1=ed25519 sig appended)
 *   bit 2: rounding (0=exact, 1=rounded)
 *   bit 1: round_dir (0=down, 1=up)
 *   bit 0: reserved
 *
 * Maximum payload: 13 bytes base (flags+value+pair+ts+sender)
 *   + 1+127 bytes note + 64 bytes sig = 205 bytes max
 *   Base64url of 205 bytes = ~274 chars + "#1pa/1" prefix = ~280 chars < 300 ✓
 *
 * MISRA-C:2012 compliance:
 * - No dynamic allocation (caller provides all buffers)
 * - All functions return explicit error codes
 * - Fixed-size buffers
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef ZAKO_PADSURL_H
#define ZAKO_PADSURL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

#define ZPU_URL_MAX       300u  /* Max URL string length (including null) */
#define ZPU_PREFIX        "#1pa/1"
#define ZPU_PREFIX_LEN    6u
#define ZPU_NOTE_MAX      127u  /* Max note bytes */
#define ZPU_PAYLOAD_MAX   205u  /* Max binary payload before base64 */
#define ZPU_SIG_LEN       64u   /* ed25519 signature */

/* Version byte */
#define ZPU_VERSION       0x01u

/* Error codes */
#define ZPU_OK            0
#define ZPU_ERR_NULL     (-1)
#define ZPU_ERR_SIZE     (-2)   /* Buffer too small or data too large */
#define ZPU_ERR_FORMAT   (-3)   /* Invalid URL format */
#define ZPU_ERR_DECODE   (-4)   /* Base64 decode failure */
#define ZPU_ERR_VERSION  (-5)   /* Unsupported version */

/* ========================================================================
 * RECORD STRUCTURE (for encoding/decoding)
 * ======================================================================== */

typedef struct {
    /* Core fields (always present) */
    uint32_t value_n;        /* 25-bit value (from Layer 3) */
    uint8_t  direction;      /* 0=Plus/In, 1=Minus/Out */
    uint8_t  status;         /* 0=Paid, 1=Debt */
    uint8_t  debit_credit;   /* 0=Credit, 1=Debit */
    uint8_t  rounding;       /* 0=exact, 1=rounded */
    uint8_t  round_dir;      /* 0=down, 1=up */
    uint8_t  account_pair;   /* 4-bit account pair code */
    uint32_t timestamp;      /* Unix epoch seconds (32-bit, good until 2106) */
    uint32_t sender_id;      /* Layer 1 sender identity */

    /* Optional note */
    uint8_t  has_note;
    uint8_t  note[ZPU_NOTE_MAX];
    uint8_t  note_len;       /* 0-127 */

    /* Optional signature */
    uint8_t  has_sig;
    uint8_t  sig[ZPU_SIG_LEN];
} zpu_record_t;

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

/*
 * zpu_encode — Encode a record as a pads-v1 URL string.
 *
 * @param record    Input record
 * @param out_url   Output buffer (must be at least ZPU_URL_MAX bytes)
 * @param out_len   Size of output buffer
 * @param written   Output: number of chars written (excluding null)
 * @return ZPU_OK on success
 */
int zpu_encode(const zpu_record_t *record,
               char *out_url, size_t out_len,
               size_t *written);

/*
 * zpu_decode — Decode a pads-v1 URL string into a record.
 *
 * @param url       Input null-terminated URL string
 * @param out       Output record structure
 * @return ZPU_OK on success
 */
int zpu_decode(const char *url, zpu_record_t *out);

/*
 * zpu_is_padsurl — Quick check: does this string look like a pads-v1 URL?
 *
 * @param str  Input string
 * @return 1 if starts with "#1pa/", 0 otherwise
 */
int zpu_is_padsurl(const char *str);

/* ========================================================================
 * PUBLIC API — BASE64URL (exposed for testing)
 * ======================================================================== */

/*
 * zpu_base64url_encode — Encode binary to base64url (no padding).
 */
int zpu_base64url_encode(const uint8_t *data, size_t data_len,
                         char *out, size_t out_max, size_t *written);

/*
 * zpu_base64url_decode — Decode base64url to binary.
 */
int zpu_base64url_decode(const char *str, size_t str_len,
                         uint8_t *out, size_t out_max, size_t *written);

#ifdef __cplusplus
}
#endif

#endif /* ZAKO_PADSURL_H */
