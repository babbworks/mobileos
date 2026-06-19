/*
 * zako_bitledger.h — BitLedger v3.0 Codec for ZAKO OS
 *
 * Implements the double-entry accounting wire protocol:
 *   - Layer 2: Batch Header (48 bits / 6 bytes)
 *   - Layer 3: Transaction Record (40 bits / 5 bytes)
 *   - Control Records (8 bits / 1 byte)
 *   - Cross-layer validation (bit29=bit37, bit30=bit38)
 *   - Conservation invariant (batch balance check)
 *   - Account pair table (14 valid pairs + compound continuation)
 *   - Value encoding: N = A × 2^S + r
 *
 * Layer 3 — 40-Bit Transaction Record:
 *   Bits 1-25:  Value Block (25 bits) — N = A × 2^S + r
 *   Bits 26-32: Flag Bits (7 bits):
 *     26: Rounding Flag (0=exact, 1=rounded)
 *     27: Rounding Direction (0=down, 1=up) [invalid if bit26=0,bit27=1]
 *     28: Split Order (0=session default, 1=reverse)
 *     29: Direction ± (0=Plus/In, 1=Minus/Out) [MUST = bit37]
 *     30: Status P/F (0=Paid/Past, 1=Debt/Future) [MUST = bit38]
 *     31: Debit/Credit (0=Credit primary, 1=Debit primary)
 *     32: Quantity Present (0=flat total, 1=optimal split active)
 *   Bits 33-40: Accounting Block (8 bits):
 *     33-36: Account Pair (4-bit code, 14 valid + 1110 + 1111)
 *     37: Direction echo [MUST = bit29]
 *     38: Status echo [MUST = bit30]
 *     39: Completeness (0=complete, 1=partial/continuation)
 *     40: Extension Flag (0=done, 1=extension byte follows)
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef ZAKO_BITLEDGER_H
#define ZAKO_BITLEDGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * SIZES
 * ======================================================================== */

#define ZBL_LAYER2_SIZE   6u   /* 48 bits = 6 bytes */
#define ZBL_LAYER3_SIZE   5u   /* 40 bits = 5 bytes */
#define ZBL_CONTROL_SIZE  1u   /* 8 bits = 1 byte */

/* ========================================================================
 * ERROR CODES
 * ======================================================================== */

#define ZBL_OK                0
#define ZBL_ERR_NULL         (-1)
#define ZBL_ERR_SIZE         (-2)
#define ZBL_ERR_INVALID      (-3)  /* Invalid field or protocol error */
#define ZBL_ERR_CROSSLAYER   (-4)  /* bit29 != bit37 or bit30 != bit38 */
#define ZBL_ERR_CONSERVATION (-5)  /* Batch balance != 0 */
#define ZBL_ERR_OVERFLOW     (-6)  /* Value exceeds 25-bit capacity */

/* ========================================================================
 * ACCOUNT PAIR CONSTANTS (4-bit codes, bits 33-36)
 * ======================================================================== */

#define ZBL_AP_OP_EXPENSE_ASSET      0x00u  /* Operating Expense / Asset */
#define ZBL_AP_OP_EXPENSE_LIABILITY  0x01u  /* Operating Expense / Liability */
#define ZBL_AP_NONOP_EXPENSE_ASSET   0x02u  /* Non-Op Expense / Asset */
#define ZBL_AP_NONOP_EXPENSE_LIAB    0x03u  /* Non-Op Expense / Liability */
#define ZBL_AP_OP_INCOME_ASSET       0x04u  /* Operating Income / Asset */
#define ZBL_AP_OP_INCOME_LIABILITY   0x05u  /* Operating Income / Liability */
#define ZBL_AP_NONOP_INCOME_ASSET    0x06u  /* Non-Op Income / Asset */
#define ZBL_AP_NONOP_INCOME_LIAB     0x07u  /* Non-Op Income / Liability */
#define ZBL_AP_ASSET_LIABILITY       0x08u  /* Asset / Liability */
#define ZBL_AP_ASSET_EQUITY          0x09u  /* Asset / Equity */
#define ZBL_AP_LIABILITY_EQUITY      0x0Au  /* Liability / Equity */
#define ZBL_AP_ASSET_ASSET           0x0Bu  /* Asset / Asset (internal xfer) */
#define ZBL_AP_SUSPENSE_CLEARING     0x0Cu  /* Suspense / Clearing */
#define ZBL_AP_INTER_COMPANY         0x0Du  /* Inter-Company */
#define ZBL_AP_CORRECTION            0x0Eu  /* Correction / Netting */
#define ZBL_AP_COMPOUND_CONT         0x0Fu  /* Compound Continuation */

/* ========================================================================
 * CONTROL RECORD TYPES (3-bit code in upper bits)
 * ======================================================================== */

#define ZBL_CTRL_SESSION_OPEN   0x00u  /* 000 */
#define ZBL_CTRL_SESSION_CLOSE  0x01u  /* 001 */
#define ZBL_CTRL_BATCH_OPEN     0x02u  /* 010 */
#define ZBL_CTRL_BATCH_CLOSE    0x03u  /* 011 */
#define ZBL_CTRL_ACK            0x04u  /* 100 */
#define ZBL_CTRL_NAK            0x05u  /* 101 */
#define ZBL_CTRL_STATUS         0x06u  /* 110 */
#define ZBL_CTRL_EXTENSION      0x07u  /* 111 */

/* ========================================================================
 * LAYER 2 — BATCH HEADER (48 bits)
 * ======================================================================== */

typedef struct {
    uint8_t  raw[ZBL_LAYER2_SIZE];
    uint8_t  transmission_type;  /* bits 1-2: 01/10/11 (00=error) */
    uint8_t  scaling_factor;     /* bits 3-9: 0-127 (power of 10 index) */
    uint8_t  optimal_split;      /* bits 10-13: 0-15 (S in N=A×2^S+r) */
    uint8_t  decimal_pos;        /* bits 14-16: 0-7 */
    uint8_t  enquiry_bell;       /* bit 17 */
    uint8_t  ack_bell;           /* bit 18 */
    uint8_t  group_sep;          /* bits 19-22: 0-15 */
    uint8_t  record_sep;         /* bits 23-27: 0-31 */
    uint8_t  file_sep;           /* bits 28-30: 0-7 */
    uint8_t  entity_id;          /* bits 31-35: 0-31 */
    uint8_t  currency_code;      /* bits 36-41: 0-63 */
    int8_t   rounding_balance;   /* bits 42-45: 4-bit signed (sign-magnitude) */
    uint8_t  compound_prefix;    /* bits 46-47: 0-3 */
    /* bit 48: reserved = 1 */
} zbl_layer2_t;

/* ========================================================================
 * LAYER 3 — TRANSACTION RECORD (40 bits)
 * ======================================================================== */

typedef struct {
    uint8_t  raw[ZBL_LAYER3_SIZE];

    /* Value Block (bits 1-25) */
    uint32_t value_n;        /* Full 25-bit value N */
    uint32_t value_a;        /* Upper field A (depends on split S) */
    uint32_t value_r;        /* Lower field r (depends on split S) */

    /* Flag Bits (bits 26-32) */
    uint8_t  rounding;       /* bit 26: 0=exact, 1=rounded */
    uint8_t  round_dir;      /* bit 27: 0=down, 1=up */
    uint8_t  split_order;    /* bit 28: 0=session default, 1=reverse */
    uint8_t  direction;      /* bit 29: 0=Plus/In, 1=Minus/Out */
    uint8_t  status;         /* bit 30: 0=Paid, 1=Debt */
    uint8_t  debit_credit;   /* bit 31: 0=Credit, 1=Debit */
    uint8_t  qty_present;    /* bit 32: 0=flat, 1=quantity split */

    /* Accounting Block (bits 33-40) */
    uint8_t  account_pair;   /* bits 33-36: 4-bit code */
    uint8_t  dir_echo;       /* bit 37: MUST = bit 29 */
    uint8_t  status_echo;    /* bit 38: MUST = bit 30 */
    uint8_t  completeness;   /* bit 39: 0=complete, 1=partial */
    uint8_t  extension;      /* bit 40: 0=done, 1=ext byte follows */

    /* Validation state */
    uint8_t  crosslayer_valid;  /* 1 if bit29==bit37 AND bit30==bit38 */
    uint8_t  rounding_valid;    /* 1 if NOT (bit26=0 AND bit27=1) */
} zbl_record_t;

/* ========================================================================
 * CONTROL RECORD (8 bits)
 * ======================================================================== */

typedef struct {
    uint8_t raw;
    uint8_t type;       /* bits 1-3: control type (0-7) */
    uint8_t payload;    /* bits 4-8: type-specific payload (5 bits) */
} zbl_control_t;

/* ========================================================================
 * PUBLIC API — LAYER 2 (BATCH HEADER)
 * ======================================================================== */

int zbl_layer2_decode(const uint8_t data[ZBL_LAYER2_SIZE], zbl_layer2_t *out);

int zbl_layer2_encode(const zbl_layer2_t *fields, uint8_t out[ZBL_LAYER2_SIZE]);

/* ========================================================================
 * PUBLIC API — LAYER 3 (TRANSACTION RECORD)
 * ======================================================================== */

/*
 * zbl_record_decode — Decode 5 bytes into a transaction record.
 *
 * Performs cross-layer validation (bit29=bit37, bit30=bit38)
 * and rounding validity check (bit26=0,bit27=1 is protocol error).
 *
 * @param data    5-byte input
 * @param split_s Optimal Split from Layer 2 (default 8)
 * @param out     Output structure
 * @return ZBL_OK on success
 */
int zbl_record_decode(const uint8_t data[ZBL_LAYER3_SIZE],
                      uint8_t split_s, zbl_record_t *out);

/*
 * zbl_record_encode — Encode a transaction record into 5 bytes.
 *
 * Automatically sets bit37=bit29 and bit38=bit30 (cross-layer echo).
 * Returns ZBL_ERR_OVERFLOW if value_n exceeds 25-bit capacity.
 *
 * @param value_n      25-bit value (0 to 33,554,431)
 * @param rounding     0=exact, 1=rounded
 * @param round_dir    0=down, 1=up (only valid when rounding=1)
 * @param split_order  0=session default, 1=reverse
 * @param direction    0=Plus/In, 1=Minus/Out
 * @param status       0=Paid, 1=Debt
 * @param debit_credit 0=Credit, 1=Debit
 * @param qty_present  0=flat, 1=quantity split
 * @param account_pair 4-bit account pair code
 * @param completeness 0=complete, 1=partial
 * @param extension    0=done, 1=ext byte follows
 * @param out          5-byte output buffer
 * @return ZBL_OK on success
 */
int zbl_record_encode(uint32_t value_n,
                      uint8_t rounding, uint8_t round_dir,
                      uint8_t split_order, uint8_t direction,
                      uint8_t status, uint8_t debit_credit,
                      uint8_t qty_present, uint8_t account_pair,
                      uint8_t completeness, uint8_t extension,
                      uint8_t out[ZBL_LAYER3_SIZE]);

/* ========================================================================
 * PUBLIC API — CONTROL RECORDS
 * ======================================================================== */

int zbl_control_decode(uint8_t byte, zbl_control_t *out);

uint8_t zbl_control_encode(uint8_t type, uint8_t payload);

/* ========================================================================
 * PUBLIC API — CONSERVATION CHECK
 * ======================================================================== */

/*
 * zbl_conservation_check — Verify batch balance equals zero.
 *
 * Sums all records: Plus/In adds value, Minus/Out subtracts.
 * Returns ZBL_OK if sum == 0, ZBL_ERR_CONSERVATION otherwise.
 *
 * @param records     Array of decoded records
 * @param count       Number of records in the batch
 * @param out_balance Output: the computed balance (can be NULL)
 * @return ZBL_OK if balanced, ZBL_ERR_CONSERVATION if not
 */
int zbl_conservation_check(const zbl_record_t *records, size_t count,
                           int64_t *out_balance);

/* ========================================================================
 * PUBLIC API — VALUE HELPERS
 * ======================================================================== */

/*
 * zbl_value_split — Split N into A and r given optimal split S.
 *
 * A = N / 2^S  (integer division)
 * r = N mod 2^S
 */
void zbl_value_split(uint32_t n, uint8_t split_s,
                     uint32_t *out_a, uint32_t *out_r);

/*
 * zbl_value_join — Reconstruct N from A and r given optimal split S.
 *
 * N = A × 2^S + r
 */
uint32_t zbl_value_join(uint32_t a, uint32_t r, uint8_t split_s);

/*
 * zbl_account_pair_name — Human-readable name for an account pair code.
 *
 * @param code  4-bit account pair (0-15)
 * @return Static string, or "UNKNOWN" for invalid codes
 */
const char *zbl_account_pair_name(uint8_t code);

#ifdef __cplusplus
}
#endif

#endif /* ZAKO_BITLEDGER_H */
