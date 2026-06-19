/*
 * zako_padsurl.c — pads-v1 URL Codec Implementation
 *
 * Encodes/decodes BitLedger records as compact base64url URLs.
 *
 * Binary payload layout:
 *   [flags(1)][value_hi(1)][value_mid(1)][value_lo(1)][account_pair(1)]
 *   [ts_3(1)][ts_2(1)][ts_1(1)][ts_0(1)][sender_3(1)][sender_2(1)][sender_1(1)][sender_0(1)]
 *   [if has_note: note_len(1)][note_bytes(note_len)]
 *   [if has_sig: sig(64)]
 *
 * Total base: 13 bytes (always present)
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "zako_padsurl.h"
#include <string.h>

/* ========================================================================
 * BASE64URL CODEC (RFC 4648 §5, no padding)
 * ======================================================================== */

static const char B64_ENCODE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static const int8_t B64_DECODE[128] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,
     52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
};

int zpu_base64url_encode(const uint8_t *data, size_t data_len,
                         char *out, size_t out_max, size_t *written)
{
    size_t i, o = 0;
    size_t full_triples = data_len / 3;
    size_t remainder = data_len % 3;
    size_t needed = full_triples * 4 + (remainder ? remainder + 1 : 0) + 1;

    if (data == NULL || out == NULL || written == NULL) { return ZPU_ERR_NULL; }
    if (needed > out_max) { return ZPU_ERR_SIZE; }

    for (i = 0; i < full_triples; i++) {
        size_t idx = i * 3;
        uint32_t triple = ((uint32_t)data[idx] << 16u) |
                          ((uint32_t)data[idx+1] << 8u) |
                          (uint32_t)data[idx+2];
        out[o++] = B64_ENCODE[(triple >> 18u) & 0x3Fu];
        out[o++] = B64_ENCODE[(triple >> 12u) & 0x3Fu];
        out[o++] = B64_ENCODE[(triple >> 6u) & 0x3Fu];
        out[o++] = B64_ENCODE[triple & 0x3Fu];
    }

    if (remainder == 1) {
        uint32_t val = (uint32_t)data[i * 3] << 16u;
        out[o++] = B64_ENCODE[(val >> 18u) & 0x3Fu];
        out[o++] = B64_ENCODE[(val >> 12u) & 0x3Fu];
    } else if (remainder == 2) {
        uint32_t val = ((uint32_t)data[i * 3] << 16u) |
                       ((uint32_t)data[i * 3 + 1] << 8u);
        out[o++] = B64_ENCODE[(val >> 18u) & 0x3Fu];
        out[o++] = B64_ENCODE[(val >> 12u) & 0x3Fu];
        out[o++] = B64_ENCODE[(val >> 6u) & 0x3Fu];
    }

    out[o] = '\0';
    *written = o;
    return ZPU_OK;
}

int zpu_base64url_decode(const char *str, size_t str_len,
                         uint8_t *out, size_t out_max, size_t *written)
{
    size_t i, o = 0;
    size_t full_quads = str_len / 4;
    size_t remainder = str_len % 4;
    size_t needed = full_quads * 3;

    if (str == NULL || out == NULL || written == NULL) { return ZPU_ERR_NULL; }

    if (remainder == 2) { needed += 1; }
    else if (remainder == 3) { needed += 2; }
    else if (remainder == 1) { return ZPU_ERR_DECODE; } /* invalid */

    if (needed > out_max) { return ZPU_ERR_SIZE; }

    for (i = 0; i < full_quads; i++) {
        size_t idx = i * 4;
        int8_t a = B64_DECODE[(unsigned char)str[idx]];
        int8_t b = B64_DECODE[(unsigned char)str[idx+1]];
        int8_t c = B64_DECODE[(unsigned char)str[idx+2]];
        int8_t d = B64_DECODE[(unsigned char)str[idx+3]];
        if (a < 0 || b < 0 || c < 0 || d < 0) { return ZPU_ERR_DECODE; }

        uint32_t triple = ((uint32_t)a << 18u) | ((uint32_t)b << 12u) |
                          ((uint32_t)c << 6u) | (uint32_t)d;
        out[o++] = (uint8_t)((triple >> 16u) & 0xFFu);
        out[o++] = (uint8_t)((triple >> 8u) & 0xFFu);
        out[o++] = (uint8_t)(triple & 0xFFu);
    }

    if (remainder == 2) {
        size_t idx = full_quads * 4;
        int8_t a = B64_DECODE[(unsigned char)str[idx]];
        int8_t b = B64_DECODE[(unsigned char)str[idx+1]];
        if (a < 0 || b < 0) { return ZPU_ERR_DECODE; }
        uint32_t val = ((uint32_t)a << 18u) | ((uint32_t)b << 12u);
        out[o++] = (uint8_t)((val >> 16u) & 0xFFu);
    } else if (remainder == 3) {
        size_t idx = full_quads * 4;
        int8_t a = B64_DECODE[(unsigned char)str[idx]];
        int8_t b = B64_DECODE[(unsigned char)str[idx+1]];
        int8_t c = B64_DECODE[(unsigned char)str[idx+2]];
        if (a < 0 || b < 0 || c < 0) { return ZPU_ERR_DECODE; }
        uint32_t val = ((uint32_t)a << 18u) | ((uint32_t)b << 12u) | ((uint32_t)c << 6u);
        out[o++] = (uint8_t)((val >> 16u) & 0xFFu);
        out[o++] = (uint8_t)((val >> 8u) & 0xFFu);
    }

    *written = o;
    return ZPU_OK;
}

/* ========================================================================
 * PAYLOAD SERIALIZATION
 * ======================================================================== */

static size_t serialize_payload(const zpu_record_t *rec, uint8_t *buf)
{
    size_t off = 0;

    /* Flags byte */
    uint8_t flags = 0;
    if (rec->direction)    flags |= 0x80u;
    if (rec->status)       flags |= 0x40u;
    if (rec->debit_credit) flags |= 0x20u;
    if (rec->has_note)     flags |= 0x10u;
    if (rec->has_sig)      flags |= 0x08u;
    if (rec->rounding)     flags |= 0x04u;
    if (rec->round_dir)    flags |= 0x02u;
    buf[off++] = flags;

    /* Value (3 bytes big-endian, 25 bits fits in 24 bits for values < 16M) */
    /* Actually 25 bits: max 33,554,431. Use 4 bytes? No — use 3 bytes for
     * values <= 16,777,215 and set high bit of first byte if > 16M.
     * Simpler: just use 3 bytes, truncating to 24-bit (max ~16.7M).
     * For pads-v1 (work records, ZMW), 16.7M kwacha is more than enough.
     * If value > 16M, we could use 4 bytes — but keep it simple for v1. */
    buf[off++] = (uint8_t)((rec->value_n >> 16u) & 0xFFu);
    buf[off++] = (uint8_t)((rec->value_n >> 8u) & 0xFFu);
    buf[off++] = (uint8_t)(rec->value_n & 0xFFu);

    /* Account pair (1 byte, only lower 4 bits used) */
    buf[off++] = rec->account_pair & 0x0Fu;

    /* Timestamp (4 bytes big-endian) */
    buf[off++] = (uint8_t)((rec->timestamp >> 24u) & 0xFFu);
    buf[off++] = (uint8_t)((rec->timestamp >> 16u) & 0xFFu);
    buf[off++] = (uint8_t)((rec->timestamp >> 8u) & 0xFFu);
    buf[off++] = (uint8_t)(rec->timestamp & 0xFFu);

    /* Sender ID (4 bytes big-endian) */
    buf[off++] = (uint8_t)((rec->sender_id >> 24u) & 0xFFu);
    buf[off++] = (uint8_t)((rec->sender_id >> 16u) & 0xFFu);
    buf[off++] = (uint8_t)((rec->sender_id >> 8u) & 0xFFu);
    buf[off++] = (uint8_t)(rec->sender_id & 0xFFu);

    /* Optional note */
    if (rec->has_note && rec->note_len > 0) {
        uint8_t nlen = (rec->note_len > ZPU_NOTE_MAX) ?
                       ZPU_NOTE_MAX : rec->note_len;
        buf[off++] = nlen;
        memcpy(buf + off, rec->note, nlen);
        off += nlen;
    }

    /* Optional signature */
    if (rec->has_sig) {
        memcpy(buf + off, rec->sig, ZPU_SIG_LEN);
        off += ZPU_SIG_LEN;
    }

    return off;
}

static int deserialize_payload(const uint8_t *buf, size_t len, zpu_record_t *rec)
{
    size_t off = 0;

    if (len < 13) { return ZPU_ERR_FORMAT; } /* minimum: flags+value+pair+ts+sender */

    memset(rec, 0, sizeof(*rec));

    /* Flags */
    uint8_t flags = buf[off++];
    rec->direction    = (flags & 0x80u) ? 1 : 0;
    rec->status       = (flags & 0x40u) ? 1 : 0;
    rec->debit_credit = (flags & 0x20u) ? 1 : 0;
    rec->has_note     = (flags & 0x10u) ? 1 : 0;
    rec->has_sig      = (flags & 0x08u) ? 1 : 0;
    rec->rounding     = (flags & 0x04u) ? 1 : 0;
    rec->round_dir    = (flags & 0x02u) ? 1 : 0;

    /* Value (3 bytes) */
    rec->value_n = ((uint32_t)buf[off] << 16u) |
                   ((uint32_t)buf[off+1] << 8u) |
                   (uint32_t)buf[off+2];
    off += 3;

    /* Account pair */
    rec->account_pair = buf[off++] & 0x0Fu;

    /* Timestamp */
    rec->timestamp = ((uint32_t)buf[off] << 24u) |
                     ((uint32_t)buf[off+1] << 16u) |
                     ((uint32_t)buf[off+2] << 8u) |
                     (uint32_t)buf[off+3];
    off += 4;

    /* Sender ID */
    rec->sender_id = ((uint32_t)buf[off] << 24u) |
                     ((uint32_t)buf[off+1] << 16u) |
                     ((uint32_t)buf[off+2] << 8u) |
                     (uint32_t)buf[off+3];
    off += 4;

    /* Optional note */
    if (rec->has_note) {
        if (off >= len) { return ZPU_ERR_FORMAT; }
        rec->note_len = buf[off++];
        if (rec->note_len > ZPU_NOTE_MAX || off + rec->note_len > len) {
            return ZPU_ERR_FORMAT;
        }
        memcpy(rec->note, buf + off, rec->note_len);
        off += rec->note_len;
    }

    /* Optional signature */
    if (rec->has_sig) {
        if (off + ZPU_SIG_LEN > len) { return ZPU_ERR_FORMAT; }
        memcpy(rec->sig, buf + off, ZPU_SIG_LEN);
        off += ZPU_SIG_LEN;
    }

    return ZPU_OK;
}

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

int zpu_encode(const zpu_record_t *record,
               char *out_url, size_t out_len,
               size_t *written)
{
    uint8_t payload[ZPU_PAYLOAD_MAX];
    char b64_buf[280];
    size_t payload_len, b64_len;
    int rc;

    if (record == NULL || out_url == NULL || written == NULL) {
        return ZPU_ERR_NULL;
    }
    if (out_len < ZPU_PREFIX_LEN + 1) { return ZPU_ERR_SIZE; }

    /* Serialize binary payload */
    payload_len = serialize_payload(record, payload);

    /* Base64url encode */
    rc = zpu_base64url_encode(payload, payload_len, b64_buf, sizeof(b64_buf), &b64_len);
    if (rc != ZPU_OK) { return rc; }

    /* Check total URL length */
    size_t total = ZPU_PREFIX_LEN + b64_len;
    if (total + 1 > out_len) { return ZPU_ERR_SIZE; }
    if (total >= ZPU_URL_MAX) { return ZPU_ERR_SIZE; }

    /* Assemble: prefix + base64 */
    memcpy(out_url, ZPU_PREFIX, ZPU_PREFIX_LEN);
    memcpy(out_url + ZPU_PREFIX_LEN, b64_buf, b64_len);
    out_url[total] = '\0';

    *written = total;
    return ZPU_OK;
}

int zpu_decode(const char *url, zpu_record_t *out)
{
    uint8_t payload[ZPU_PAYLOAD_MAX];
    size_t url_len, b64_len, payload_len;
    int rc;

    if (url == NULL || out == NULL) { return ZPU_ERR_NULL; }

    url_len = strlen(url);

    /* Verify prefix */
    if (url_len <= ZPU_PREFIX_LEN) { return ZPU_ERR_FORMAT; }
    if (memcmp(url, ZPU_PREFIX, ZPU_PREFIX_LEN) != 0) { return ZPU_ERR_FORMAT; }

    /* Decode base64url portion */
    const char *b64_part = url + ZPU_PREFIX_LEN;
    b64_len = url_len - ZPU_PREFIX_LEN;

    rc = zpu_base64url_decode(b64_part, b64_len, payload, sizeof(payload), &payload_len);
    if (rc != ZPU_OK) { return rc; }

    /* Deserialize */
    return deserialize_payload(payload, payload_len, out);
}

int zpu_is_padsurl(const char *str)
{
    if (str == NULL) { return 0; }
    if (strlen(str) <= ZPU_PREFIX_LEN) { return 0; }
    return (memcmp(str, ZPU_PREFIX, ZPU_PREFIX_LEN) == 0) ? 1 : 0;
}
