/*
 * identd_store.c — Identity Store Implementation
 *
 * SQLite-backed key management, capability grants, identity lock.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "identd_store.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ========================================================================
 * SCHEMA
 * ======================================================================== */

static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS keys ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  label TEXT NOT NULL,"
    "  pubkey BLOB NOT NULL,"
    "  seckey BLOB NOT NULL,"
    "  did TEXT NOT NULL UNIQUE,"
    "  created INTEGER NOT NULL,"
    "  is_sovereign INTEGER DEFAULT 0,"
    "  locked INTEGER DEFAULT 0"
    ");"
    "CREATE TABLE IF NOT EXISTS capabilities ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  grantor_did TEXT NOT NULL,"
    "  grantee_did TEXT NOT NULL,"
    "  capability TEXT NOT NULL,"
    "  depth INTEGER NOT NULL,"
    "  granted INTEGER NOT NULL,"
    "  revoked INTEGER DEFAULT 0,"
    "  sig BLOB NOT NULL"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_keys_did ON keys(did);"
    "CREATE INDEX IF NOT EXISTS idx_keys_label ON keys(label);"
    "CREATE INDEX IF NOT EXISTS idx_caps_grantee ON capabilities(grantee_did);"
    "CREATE INDEX IF NOT EXISTS idx_caps_grantor ON capabilities(grantor_did);";

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

static int exec_sql(sqlite3 *db, const char *sql)
{
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (err) { sqlite3_free(err); }
    return (rc == SQLITE_OK) ? IDS_OK : IDS_ERR_DB;
}

/* Find sovereign key ID on open */
static void load_sovereign_id(ids_store_t *store)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id FROM keys WHERE is_sovereign=1 LIMIT 1";

    store->sovereign_key_id = -1;

    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            store->sovereign_key_id = sqlite3_column_int64(stmt, 0);
        }
    }
    if (stmt) { sqlite3_finalize(stmt); }
}

/* Insert a key record into the database */
static int insert_key(ids_store_t *store, const ids_key_t *key)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO keys (label, pubkey, seckey, did, created, is_sovereign, locked) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)";
    int rc;

    rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { return IDS_ERR_DB; }

    sqlite3_bind_text(stmt, 1, key->label, -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, key->pubkey, ZAKO_SIGN_PUBKEY_LEN, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, key->seckey, ZAKO_SIGN_SECKEY_LEN, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, key->did, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, key->created);
    sqlite3_bind_int(stmt, 6, key->is_sovereign);
    sqlite3_bind_int(stmt, 7, key->locked);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? IDS_OK : IDS_ERR_DB;
}

/* Load a key from a result row */
static void row_to_key(sqlite3_stmt *stmt, ids_key_t *key)
{
    const void *blob;
    int blen;
    const char *txt;

    key->id = sqlite3_column_int64(stmt, 0);

    txt = (const char *)sqlite3_column_text(stmt, 1);
    if (txt) {
        strncpy(key->label, txt, IDS_LABEL_MAX - 1);
        key->label[IDS_LABEL_MAX - 1] = '\0';
    }

    blob = sqlite3_column_blob(stmt, 2);
    blen = sqlite3_column_bytes(stmt, 2);
    if (blob && blen == ZAKO_SIGN_PUBKEY_LEN) {
        memcpy(key->pubkey, blob, ZAKO_SIGN_PUBKEY_LEN);
    }

    blob = sqlite3_column_blob(stmt, 3);
    blen = sqlite3_column_bytes(stmt, 3);
    if (blob && blen == ZAKO_SIGN_SECKEY_LEN) {
        memcpy(key->seckey, blob, ZAKO_SIGN_SECKEY_LEN);
    }

    txt = (const char *)sqlite3_column_text(stmt, 4);
    if (txt) {
        strncpy(key->did, txt, ZAKO_DID_STR_MAX - 1);
        key->did[ZAKO_DID_STR_MAX - 1] = '\0';
    }

    key->created = sqlite3_column_int64(stmt, 5);
    key->is_sovereign = (uint8_t)sqlite3_column_int(stmt, 6);
    key->locked = (uint8_t)sqlite3_column_int(stmt, 7);
}

/* Compute depth for a new grant from a grantor */
static int compute_grant_depth(ids_store_t *store, const char *grantor_did,
                               uint8_t *out_depth)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql;
    int rc;

    /* If grantor is our sovereign, depth = 0 */
    if (store->sovereign_key_id >= 0) {
        ids_key_t sov;
        rc = ids_get_sovereign(store, &sov);
        if (rc == IDS_OK && strcmp(sov.did, grantor_did) == 0) {
            *out_depth = 0;
            return IDS_OK;
        }
    }

    /* Otherwise, grantor's depth = max depth of their received grants + 1 */
    sql = "SELECT MAX(depth) FROM capabilities "
          "WHERE grantee_did=? AND revoked=0";

    rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { return IDS_ERR_DB; }

    sqlite3_bind_text(stmt, 1, grantor_did, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            int parent_depth = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            if ((uint8_t)(parent_depth + 1) > IDS_MAX_DEPTH) {
                return IDS_ERR_DEPTH;
            }
            *out_depth = (uint8_t)(parent_depth + 1);
            return IDS_OK;
        }
    }
    sqlite3_finalize(stmt);

    /* Grantor has no incoming grants — they must be sovereign or this is invalid */
    return IDS_ERR_DEPTH;
}

/* ========================================================================
 * PUBLIC API — LIFECYCLE
 * ======================================================================== */

int ids_open(ids_store_t *store, const char *db_path)
{
    int rc;

    if (store == NULL || db_path == NULL) { return IDS_ERR_NULL; }

    memset(store, 0, sizeof(*store));
    store->sovereign_key_id = -1;
    store->locked = 0;

    rc = sqlite3_open(db_path, &store->db);
    if (rc != SQLITE_OK) {
        store->db = NULL;
        return IDS_ERR_DB;
    }

    exec_sql(store->db, "PRAGMA journal_mode=WAL;");
    exec_sql(store->db, "PRAGMA synchronous=FULL;");

    rc = exec_sql(store->db, SCHEMA_SQL);
    if (rc != IDS_OK) {
        sqlite3_close(store->db);
        store->db = NULL;
        return IDS_ERR_DB;
    }

    load_sovereign_id(store);
    return IDS_OK;
}

void ids_close(ids_store_t *store)
{
    if (store == NULL) { return; }
    if (store->db != NULL) {
        sqlite3_close(store->db);
        store->db = NULL;
    }
}

/* ========================================================================
 * PUBLIC API — KEY MANAGEMENT
 * ======================================================================== */

int ids_generate_sovereign(ids_store_t *store, ids_key_t *out_key)
{
    ids_key_t key;
    int rc;

    if (store == NULL || out_key == NULL) { return IDS_ERR_NULL; }

    /* Check if sovereign already exists */
    if (store->sovereign_key_id >= 0) {
        return IDS_ERR_EXISTS;
    }

    memset(&key, 0, sizeof(key));
    strncpy(key.label, "sovereign", IDS_LABEL_MAX - 1);
    key.is_sovereign = 1;
    key.locked = 0;
    key.created = (int64_t)time(NULL);

    /* Generate keypair */
    rc = zako_sign_keypair(key.pubkey, key.seckey);
    if (rc != ZAKO_SIGN_OK) { return IDS_ERR_DB; }

    /* Format DID */
    rc = zako_did_from_pubkey(key.pubkey, key.did, sizeof(key.did));
    if (rc != ZAKO_DID_OK) { return IDS_ERR_DB; }

    /* Store */
    rc = insert_key(store, &key);
    if (rc != IDS_OK) { return rc; }

    /* Update sovereign ID */
    store->sovereign_key_id = sqlite3_last_insert_rowid(store->db);
    key.id = store->sovereign_key_id;

    *out_key = key;
    return IDS_OK;
}

int ids_generate_key(ids_store_t *store, const char *label, ids_key_t *out_key)
{
    ids_key_t key;
    int rc;

    if (store == NULL || label == NULL || out_key == NULL) { return IDS_ERR_NULL; }

    /* Check key count limit */
    if (ids_get_key_count(store) >= IDS_MAX_KEYS) {
        return IDS_ERR_FULL;
    }

    memset(&key, 0, sizeof(key));
    strncpy(key.label, label, IDS_LABEL_MAX - 1);
    key.label[IDS_LABEL_MAX - 1] = '\0';
    key.is_sovereign = 0;
    key.locked = 0;
    key.created = (int64_t)time(NULL);

    rc = zako_sign_keypair(key.pubkey, key.seckey);
    if (rc != ZAKO_SIGN_OK) { return IDS_ERR_DB; }

    rc = zako_did_from_pubkey(key.pubkey, key.did, sizeof(key.did));
    if (rc != ZAKO_DID_OK) { return IDS_ERR_DB; }

    rc = insert_key(store, &key);
    if (rc != IDS_OK) { return rc; }

    key.id = sqlite3_last_insert_rowid(store->db);
    *out_key = key;
    return IDS_OK;
}

int ids_generate_key_from_seed(ids_store_t *store, const char *label,
                               const uint8_t seed[ZAKO_SIGN_SEED_LEN],
                               ids_key_t *out_key)
{
    ids_key_t key;
    int rc;

    if (store == NULL || label == NULL || seed == NULL || out_key == NULL) {
        return IDS_ERR_NULL;
    }

    if (ids_get_key_count(store) >= IDS_MAX_KEYS) {
        return IDS_ERR_FULL;
    }

    memset(&key, 0, sizeof(key));
    strncpy(key.label, label, IDS_LABEL_MAX - 1);
    key.label[IDS_LABEL_MAX - 1] = '\0';
    key.is_sovereign = 0;
    key.locked = 0;
    key.created = (int64_t)time(NULL);

    rc = zako_sign_keypair_from_seed(seed, key.pubkey, key.seckey);
    if (rc != ZAKO_SIGN_OK) { return IDS_ERR_DB; }

    rc = zako_did_from_pubkey(key.pubkey, key.did, sizeof(key.did));
    if (rc != ZAKO_DID_OK) { return IDS_ERR_DB; }

    rc = insert_key(store, &key);
    if (rc != IDS_OK) { return rc; }

    key.id = sqlite3_last_insert_rowid(store->db);
    *out_key = key;
    return IDS_OK;
}

int ids_get_sovereign(ids_store_t *store, ids_key_t *out_key)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, label, pubkey, seckey, did, created, "
                      "is_sovereign, locked FROM keys WHERE is_sovereign=1 LIMIT 1";
    int rc;

    if (store == NULL || out_key == NULL) { return IDS_ERR_NULL; }
    if (store->sovereign_key_id < 0) { return IDS_ERR_NOT_FOUND; }

    rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { return IDS_ERR_DB; }

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return IDS_ERR_NOT_FOUND;
    }

    memset(out_key, 0, sizeof(*out_key));
    row_to_key(stmt, out_key);
    sqlite3_finalize(stmt);
    return IDS_OK;
}

int ids_get_key_by_did(ids_store_t *store, const char *did, ids_key_t *out_key)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, label, pubkey, seckey, did, created, "
                      "is_sovereign, locked FROM keys WHERE did=?";
    int rc;

    if (store == NULL || did == NULL || out_key == NULL) { return IDS_ERR_NULL; }

    rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { return IDS_ERR_DB; }

    sqlite3_bind_text(stmt, 1, did, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return IDS_ERR_NOT_FOUND;
    }

    memset(out_key, 0, sizeof(*out_key));
    row_to_key(stmt, out_key);
    sqlite3_finalize(stmt);
    return IDS_OK;
}

int ids_get_key_by_label(ids_store_t *store, const char *label, ids_key_t *out_key)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, label, pubkey, seckey, did, created, "
                      "is_sovereign, locked FROM keys WHERE label=?";
    int rc;

    if (store == NULL || label == NULL || out_key == NULL) { return IDS_ERR_NULL; }

    rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { return IDS_ERR_DB; }

    sqlite3_bind_text(stmt, 1, label, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return IDS_ERR_NOT_FOUND;
    }

    memset(out_key, 0, sizeof(*out_key));
    row_to_key(stmt, out_key);
    sqlite3_finalize(stmt);
    return IDS_OK;
}

int64_t ids_get_key_count(ids_store_t *store)
{
    sqlite3_stmt *stmt = NULL;
    int64_t count = 0;

    if (store == NULL || store->db == NULL) { return 0; }

    if (sqlite3_prepare_v2(store->db, "SELECT COUNT(*) FROM keys", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int64(stmt, 0);
        }
    }
    if (stmt) { sqlite3_finalize(stmt); }
    return count;
}

/* ========================================================================
 * PUBLIC API — SIGNING SERVICE
 * ======================================================================== */

int ids_sign(ids_store_t *store, const char *did,
             const uint8_t *message, size_t msg_len,
             uint8_t out_sig[ZAKO_SIGN_SIG_LEN])
{
    ids_key_t key;
    int rc;

    if (store == NULL || did == NULL || message == NULL || out_sig == NULL) {
        return IDS_ERR_NULL;
    }

    /* Check global lock */
    if (store->locked) { return IDS_ERR_LOCKED; }

    /* Find key */
    rc = ids_get_key_by_did(store, did, &key);
    if (rc != IDS_OK) { return rc; }

    /* Check per-key lock */
    if (key.locked) { return IDS_ERR_LOCKED; }

    /* Sign */
    rc = zako_sign(message, msg_len, key.seckey, out_sig);

    /* Zero secret key material from stack copy */
    zako_sign_seckey_zero(key.seckey);

    return (rc == ZAKO_SIGN_OK) ? IDS_OK : IDS_ERR_VERIFY;
}

int ids_verify(const char *did,
               const uint8_t *message, size_t msg_len,
               const uint8_t sig[ZAKO_SIGN_SIG_LEN])
{
    uint8_t pubkey[ZAKO_SIGN_PUBKEY_LEN];
    int rc;

    if (did == NULL || message == NULL || sig == NULL) { return IDS_ERR_NULL; }

    /* Extract public key from DID (self-resolving) */
    rc = zako_did_to_pubkey(did, pubkey);
    if (rc != ZAKO_DID_OK) { return IDS_ERR_VERIFY; }

    /* Verify signature */
    rc = zako_sign_verify(message, msg_len, sig, pubkey);
    return (rc == ZAKO_SIGN_OK) ? IDS_OK : IDS_ERR_VERIFY;
}

/* ========================================================================
 * PUBLIC API — IDENTITY LOCK
 * ======================================================================== */

int ids_lock(ids_store_t *store)
{
    if (store == NULL) { return IDS_ERR_NULL; }
    store->locked = 1;
    /* Persist lock state */
    exec_sql(store->db, "UPDATE keys SET locked=1 WHERE is_sovereign=1");
    return IDS_OK;
}

int ids_unlock(ids_store_t *store)
{
    if (store == NULL) { return IDS_ERR_NULL; }
    store->locked = 0;
    exec_sql(store->db, "UPDATE keys SET locked=0");
    return IDS_OK;
}

int ids_is_locked(ids_store_t *store)
{
    if (store == NULL) { return 1; }
    return store->locked ? 1 : 0;
}

/* ========================================================================
 * PUBLIC API — CAPABILITY GRANTS
 * ======================================================================== */

int ids_grant(ids_store_t *store,
              const char *grantor_did,
              const char *grantee_did,
              const char *capability,
              ids_cap_t *out_cap)
{
    ids_cap_t cap;
    uint8_t depth;
    uint8_t sig[ZAKO_SIGN_SIG_LEN];
    sqlite3_stmt *stmt = NULL;
    int rc;
    const char *sql;

    /* Grant payload for signing: "GRANT:<capability>:<grantee_did>" */
    char payload[256];
    size_t payload_len;

    if (store == NULL || grantor_did == NULL || grantee_did == NULL ||
        capability == NULL || out_cap == NULL) {
        return IDS_ERR_NULL;
    }

    if (store->locked) { return IDS_ERR_LOCKED; }

    /* Compute depth */
    rc = compute_grant_depth(store, grantor_did, &depth);
    if (rc != IDS_OK) { return rc; }

    /* Build signing payload */
    payload_len = (size_t)snprintf(payload, sizeof(payload),
                                   "GRANT:%s:%s", capability, grantee_did);
    if (payload_len >= sizeof(payload)) { return IDS_ERR_DB; }

    /* Sign with grantor's key */
    rc = ids_sign(store, grantor_did,
                  (const uint8_t *)payload, payload_len, sig);
    if (rc != IDS_OK) { return rc; }

    /* Build capability record */
    memset(&cap, 0, sizeof(cap));
    strncpy(cap.grantor_did, grantor_did, ZAKO_DID_STR_MAX - 1);
    strncpy(cap.grantee_did, grantee_did, ZAKO_DID_STR_MAX - 1);
    strncpy(cap.capability, capability, IDS_CAP_MAX - 1);
    cap.depth = depth;
    cap.granted = (int64_t)time(NULL);
    cap.revoked = 0;
    memcpy(cap.sig, sig, ZAKO_SIGN_SIG_LEN);

    /* Insert */
    sql = "INSERT INTO capabilities (grantor_did, grantee_did, capability, "
          "depth, granted, revoked, sig) VALUES (?, ?, ?, ?, ?, 0, ?)";
    rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { return IDS_ERR_DB; }

    sqlite3_bind_text(stmt, 1, cap.grantor_did, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, cap.grantee_did, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, cap.capability, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, cap.depth);
    sqlite3_bind_int64(stmt, 5, cap.granted);
    sqlite3_bind_blob(stmt, 6, cap.sig, ZAKO_SIGN_SIG_LEN, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { return IDS_ERR_DB; }

    cap.id = sqlite3_last_insert_rowid(store->db);
    *out_cap = cap;
    return IDS_OK;
}

int ids_revoke(ids_store_t *store, int64_t cap_id)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql;
    char grantee_did[ZAKO_DID_STR_MAX] = {0};
    int rc;
    int64_t now;

    if (store == NULL) { return IDS_ERR_NULL; }

    now = (int64_t)time(NULL);

    /* Get the grantee of this capability (for cascade) */
    sql = "SELECT grantee_did FROM capabilities WHERE id=? AND revoked=0";
    rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { return IDS_ERR_DB; }

    sqlite3_bind_int64(stmt, 1, cap_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return IDS_ERR_NOT_FOUND;
    }

    const char *txt = (const char *)sqlite3_column_text(stmt, 0);
    if (txt) {
        strncpy(grantee_did, txt, ZAKO_DID_STR_MAX - 1);
    }
    sqlite3_finalize(stmt);

    /* Revoke the target */
    char upd[128];
    snprintf(upd, sizeof(upd),
             "UPDATE capabilities SET revoked=%lld WHERE id=%lld",
             (long long)now, (long long)cap_id);
    exec_sql(store->db, upd);

    /* Cascade: revoke all grants where this grantee was the grantor */
    if (grantee_did[0] != '\0') {
        char cascade[256];
        snprintf(cascade, sizeof(cascade),
                 "UPDATE capabilities SET revoked=%lld "
                 "WHERE grantor_did='%s' AND revoked=0",
                 (long long)now, grantee_did);
        exec_sql(store->db, cascade);
    }

    return IDS_OK;
}

int ids_check_capability(ids_store_t *store,
                         const char *did,
                         const char *capability)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql;
    int rc;

    if (store == NULL || did == NULL || capability == NULL) {
        return IDS_ERR_NULL;
    }

    sql = "SELECT id, revoked FROM capabilities "
          "WHERE grantee_did=? AND capability=? ORDER BY granted DESC LIMIT 1";

    rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { return IDS_ERR_DB; }

    sqlite3_bind_text(stmt, 1, did, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, capability, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return IDS_ERR_NOT_FOUND;
    }

    int64_t revoked = sqlite3_column_int64(stmt, 1);
    sqlite3_finalize(stmt);

    return (revoked == 0) ? IDS_OK : IDS_ERR_REVOKED;
}

int ids_get_grants_for(ids_store_t *store, const char *did,
                       ids_cap_t *out_caps, size_t max_caps,
                       size_t *out_count)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql;
    int rc;
    size_t count = 0;

    if (store == NULL || did == NULL || out_caps == NULL || out_count == NULL) {
        return IDS_ERR_NULL;
    }

    sql = "SELECT id, grantor_did, grantee_did, capability, depth, "
          "granted, revoked, sig FROM capabilities "
          "WHERE grantee_did=? AND revoked=0";

    rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { return IDS_ERR_DB; }

    sqlite3_bind_text(stmt, 1, did, -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_caps) {
        ids_cap_t *cap = &out_caps[count];
        const char *txt;
        const void *blob;

        memset(cap, 0, sizeof(*cap));
        cap->id = sqlite3_column_int64(stmt, 0);

        txt = (const char *)sqlite3_column_text(stmt, 1);
        if (txt) strncpy(cap->grantor_did, txt, ZAKO_DID_STR_MAX - 1);

        txt = (const char *)sqlite3_column_text(stmt, 2);
        if (txt) strncpy(cap->grantee_did, txt, ZAKO_DID_STR_MAX - 1);

        txt = (const char *)sqlite3_column_text(stmt, 3);
        if (txt) strncpy(cap->capability, txt, IDS_CAP_MAX - 1);

        cap->depth = (uint8_t)sqlite3_column_int(stmt, 4);
        cap->granted = sqlite3_column_int64(stmt, 5);
        cap->revoked = sqlite3_column_int64(stmt, 6);

        blob = sqlite3_column_blob(stmt, 7);
        if (blob && sqlite3_column_bytes(stmt, 7) == ZAKO_SIGN_SIG_LEN) {
            memcpy(cap->sig, blob, ZAKO_SIGN_SIG_LEN);
        }

        count++;
    }

    sqlite3_finalize(stmt);
    *out_count = count;
    return IDS_OK;
}
