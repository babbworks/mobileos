/*
 * identd_store.h — Identity Store for telux-identd
 *
 * Manages persistent identity data:
 *   - Sovereign keypair (one per device, generated at first-run)
 *   - Derived key slots (per-Island, per-capability)
 *   - DID document cache
 *   - Capability grants (GRANT/REVOKE/DELEGATE records)
 *   - Identity lock state (hardware-backed lockout)
 *
 * Storage: SQLite database at /data/zako/identd.db
 * All secret key material is stored encrypted (stretch goal) or
 * in secure storage (TrustZone on target). For Phase 2, keys are
 * stored in cleartext in the SQLite database (acceptable for dev).
 *
 * Schema:
 *   keys(id INTEGER PRIMARY KEY, label TEXT, pubkey BLOB, seckey BLOB,
 *        did TEXT, created INTEGER, is_sovereign INTEGER, locked INTEGER)
 *
 *   capabilities(id INTEGER PRIMARY KEY, grantor_did TEXT, grantee_did TEXT,
 *                capability TEXT, depth INTEGER, granted INTEGER,
 *                revoked INTEGER, sig BLOB)
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef IDENTD_STORE_H
#define IDENTD_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "../libzako-sign/zako_sign.h"
#include "../libzako-did/zako_did.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declare sqlite3 */
typedef struct sqlite3 sqlite3;

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

#define IDS_LABEL_MAX       64u   /* Max label string length */
#define IDS_CAP_MAX         64u   /* Max capability name length */
#define IDS_MAX_DEPTH       3u    /* Max delegation depth */
#define IDS_MAX_KEYS        32u   /* Max key slots */

/* Error codes */
#define IDS_OK              0
#define IDS_ERR_NULL       (-1)
#define IDS_ERR_DB         (-2)   /* SQLite operation failed */
#define IDS_ERR_NOT_FOUND  (-3)   /* Key or capability not found */
#define IDS_ERR_LOCKED     (-4)   /* Identity is locked */
#define IDS_ERR_EXISTS     (-5)   /* Key/capability already exists */
#define IDS_ERR_DEPTH      (-6)   /* Delegation depth exceeded */
#define IDS_ERR_REVOKED    (-7)   /* Capability has been revoked */
#define IDS_ERR_FULL       (-8)   /* Key slot table full */
#define IDS_ERR_VERIFY     (-9)   /* Signature verification failed */

/* ========================================================================
 * STRUCTURES
 * ======================================================================== */

/* Key record */
typedef struct {
    int64_t  id;
    char     label[IDS_LABEL_MAX];
    uint8_t  pubkey[ZAKO_SIGN_PUBKEY_LEN];
    uint8_t  seckey[ZAKO_SIGN_SECKEY_LEN];
    char     did[ZAKO_DID_STR_MAX];
    int64_t  created;       /* Unix timestamp */
    uint8_t  is_sovereign;  /* 1 = device root key */
    uint8_t  locked;        /* 1 = signing disabled until unlock */
} ids_key_t;

/* Capability grant */
typedef struct {
    int64_t  id;
    char     grantor_did[ZAKO_DID_STR_MAX];
    char     grantee_did[ZAKO_DID_STR_MAX];
    char     capability[IDS_CAP_MAX];
    uint8_t  depth;         /* Delegation depth (0=direct from sovereign) */
    int64_t  granted;       /* Unix timestamp */
    int64_t  revoked;       /* 0 if active, timestamp if revoked */
    uint8_t  sig[ZAKO_SIGN_SIG_LEN]; /* Grantor's signature over grant */
} ids_cap_t;

/* Store handle */
typedef struct {
    sqlite3 *db;
    int64_t  sovereign_key_id;  /* ID of sovereign key (-1 if none) */
    uint8_t  locked;            /* 1 if identity locked globally */
} ids_store_t;

/* ========================================================================
 * PUBLIC API — STORE LIFECYCLE
 * ======================================================================== */

int ids_open(ids_store_t *store, const char *db_path);
void ids_close(ids_store_t *store);

/* ========================================================================
 * PUBLIC API — KEY MANAGEMENT
 * ======================================================================== */

/*
 * ids_generate_sovereign — Generate the device sovereign keypair.
 *
 * Only one sovereign key per device. Returns IDS_ERR_EXISTS if already set.
 * The sovereign key is the root of all trust on this device.
 *
 * @param store    Open store
 * @param out_key  Output: generated key record
 * @return IDS_OK on success
 */
int ids_generate_sovereign(ids_store_t *store, ids_key_t *out_key);

/*
 * ids_generate_key — Generate a new derived key (per-Island, per-purpose).
 *
 * @param store    Open store
 * @param label    Human-readable label (e.g., "island:work", "exchange:peer1")
 * @param out_key  Output: generated key record
 * @return IDS_OK on success
 */
int ids_generate_key(ids_store_t *store, const char *label, ids_key_t *out_key);

/*
 * ids_generate_key_from_seed — Generate a key deterministically from seed.
 *
 * Used for testing and key recovery scenarios.
 */
int ids_generate_key_from_seed(ids_store_t *store, const char *label,
                               const uint8_t seed[ZAKO_SIGN_SEED_LEN],
                               ids_key_t *out_key);

/*
 * ids_get_sovereign — Retrieve the sovereign key.
 */
int ids_get_sovereign(ids_store_t *store, ids_key_t *out_key);

/*
 * ids_get_key_by_did — Look up a key by its DID string.
 */
int ids_get_key_by_did(ids_store_t *store, const char *did, ids_key_t *out_key);

/*
 * ids_get_key_by_label — Look up a key by its label.
 */
int ids_get_key_by_label(ids_store_t *store, const char *label, ids_key_t *out_key);

/*
 * ids_get_key_count — Number of keys stored.
 */
int64_t ids_get_key_count(ids_store_t *store);

/* ========================================================================
 * PUBLIC API — SIGNING SERVICE
 * ======================================================================== */

/*
 * ids_sign — Sign a message using a stored key (identified by DID).
 *
 * Refuses if the key is locked.
 *
 * @param store     Open store
 * @param did       DID of the signing key
 * @param message   Message to sign
 * @param msg_len   Message length
 * @param out_sig   Output: signature
 * @return IDS_OK, IDS_ERR_LOCKED, or IDS_ERR_NOT_FOUND
 */
int ids_sign(ids_store_t *store, const char *did,
             const uint8_t *message, size_t msg_len,
             uint8_t out_sig[ZAKO_SIGN_SIG_LEN]);

/*
 * ids_verify — Verify a signature against a known DID.
 *
 * Extracts pubkey from DID and verifies. Works even for external DIDs
 * (doesn't require the key to be in our store — DID self-resolves).
 */
int ids_verify(const char *did,
               const uint8_t *message, size_t msg_len,
               const uint8_t sig[ZAKO_SIGN_SIG_LEN]);

/* ========================================================================
 * PUBLIC API — IDENTITY LOCK
 * ======================================================================== */

/*
 * ids_lock — Lock all signing operations (identity freeze).
 *
 * Used when device enters DORMANT mode or on security alert.
 * No signatures can be produced until ids_unlock() is called.
 */
int ids_lock(ids_store_t *store);

/*
 * ids_unlock — Unlock signing operations.
 *
 * In production: requires hardware attestation (fingerprint, PIN).
 * For Phase 2 dev: unconditional unlock.
 */
int ids_unlock(ids_store_t *store);

/*
 * ids_is_locked — Query lock state.
 */
int ids_is_locked(ids_store_t *store);

/* ========================================================================
 * PUBLIC API — CAPABILITY GRANTS
 * ======================================================================== */

/*
 * ids_grant — Create a capability grant from grantor to grantee.
 *
 * The grant is signed by the grantor's key. Depth is checked:
 * - Sovereign can grant at depth 0
 * - Depth-0 grantee can delegate at depth 1
 * - Max depth is IDS_MAX_DEPTH (3)
 *
 * @param store       Open store
 * @param grantor_did DID of the granting entity (must be in our store)
 * @param grantee_did DID of the receiving entity (can be external)
 * @param capability  Capability name (e.g., "ledger:write", "sign:records")
 * @param out_cap     Output: created capability record
 * @return IDS_OK on success
 */
int ids_grant(ids_store_t *store,
              const char *grantor_did,
              const char *grantee_did,
              const char *capability,
              ids_cap_t *out_cap);

/*
 * ids_revoke — Revoke a capability grant.
 *
 * Also cascades: all grants derived from this one are revoked.
 *
 * @param store   Open store
 * @param cap_id  ID of the capability to revoke
 * @return IDS_OK on success
 */
int ids_revoke(ids_store_t *store, int64_t cap_id);

/*
 * ids_check_capability — Check if a DID holds an active capability.
 *
 * @return IDS_OK if granted and not revoked, IDS_ERR_NOT_FOUND or IDS_ERR_REVOKED
 */
int ids_check_capability(ids_store_t *store,
                         const char *did,
                         const char *capability);

/*
 * ids_get_grants_for — Get all active capabilities for a DID.
 *
 * @param store       Open store
 * @param did         Target DID
 * @param out_caps    Output array
 * @param max_caps    Max entries in output array
 * @param out_count   Output: number of entries written
 * @return IDS_OK
 */
int ids_get_grants_for(ids_store_t *store, const char *did,
                       ids_cap_t *out_caps, size_t max_caps,
                       size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* IDENTD_STORE_H */
