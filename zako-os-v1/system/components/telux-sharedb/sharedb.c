/*
 * sharedb.c — Telux Share Daemon Implementation
 *
 * Queue management, carrier selection, transport dispatch.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "sharedb.h"
#include <string.h>
#include <time.h>

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

static sdb_entry_t *find_empty_slot(sdb_state_t *state)
{
    size_t i;
    for (i = 0; i < SDB_MAX_QUEUE; i++) {
        if (state->queue[i].state == SDB_ENTRY_EMPTY) {
            return &state->queue[i];
        }
    }
    return NULL;
}

static sdb_entry_t *find_by_id(sdb_state_t *state, uint32_t id)
{
    size_t i;
    for (i = 0; i < SDB_MAX_QUEUE; i++) {
        if (state->queue[i].id == id &&
            state->queue[i].state != SDB_ENTRY_EMPTY) {
            return &state->queue[i];
        }
    }
    return NULL;
}

static sdb_carrier_t *find_carrier_by_type(sdb_state_t *state, uint8_t type)
{
    size_t i;
    for (i = 0; i < state->carrier_count; i++) {
        if (state->carriers[i].carrier_type == type) {
            return &state->carriers[i];
        }
    }
    return NULL;
}

/*
 * Transport selection logic:
 *
 * Priority varies by power mode:
 *   FULL/STD: IP > SMS > BLE > QR
 *   CONS/CRIT: SMS > BLE > QR (IP deprioritized for power)
 *   EMRG: SMS > BLE > QR (IP suspended)
 *
 * Filtered by: peer capabilities + carrier availability.
 */
static uint8_t select_best_carrier(sdb_state_t *state, const sdb_peer_t *peer)
{
    sdb_carrier_t *c;

    if (state->power_mode <= SDB_MODE_STD) {
        /* FULL or STANDARD: prefer IP */
        if (peer->has_ip) {
            c = find_carrier_by_type(state, SDB_CARRIER_IP);
            if (c && c->available && c->available(c->ctx)) {
                return SDB_CARRIER_IP;
            }
        }
    }

    /* SMS preferred in CONS/CRIT/EMRG, or fallback from IP */
    if (peer->has_sms) {
        c = find_carrier_by_type(state, SDB_CARRIER_SMS);
        if (c && c->available && c->available(c->ctx)) {
            return SDB_CARRIER_SMS;
        }
    }

    /* BLE */
    if (peer->has_ble) {
        c = find_carrier_by_type(state, SDB_CARRIER_BLE);
        if (c && c->available && c->available(c->ctx)) {
            return SDB_CARRIER_BLE;
        }
    }

    /* QR is always available (display-only) */
    c = find_carrier_by_type(state, SDB_CARRIER_QR);
    if (c) {
        return SDB_CARRIER_QR;
    }

    return SDB_CARRIER_NONE;
}

static const char *get_endpoint_for_carrier(const sdb_peer_t *peer, uint8_t carrier)
{
    switch (carrier) {
    case SDB_CARRIER_SMS: return peer->sms_endpoint;
    case SDB_CARRIER_IP:  return peer->ip_endpoint;
    case SDB_CARRIER_BLE: return peer->did; /* BLE uses DID as address */
    case SDB_CARRIER_QR:  return "display"; /* QR has no remote endpoint */
    default: return "";
    }
}

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

int sdb_init(sdb_state_t *state)
{
    if (state == NULL) { return SDB_ERR_NULL; }
    memset(state, 0, sizeof(*state));
    state->next_id = 1;
    state->power_mode = SDB_MODE_FULL;
    return SDB_OK;
}

int sdb_add_carrier(sdb_state_t *state, const sdb_carrier_t *carrier)
{
    if (state == NULL || carrier == NULL) { return SDB_ERR_NULL; }
    if (state->carrier_count >= SDB_MAX_CARRIERS) { return SDB_ERR_FULL; }

    state->carriers[state->carrier_count] = *carrier;
    state->carrier_count++;
    return SDB_OK;
}

int sdb_set_power_mode(sdb_state_t *state, uint8_t mode)
{
    if (state == NULL) { return SDB_ERR_NULL; }
    if (mode > SDB_MODE_EMRG) { return SDB_ERR_NULL; }
    state->power_mode = mode;
    return SDB_OK;
}

int sdb_enqueue(sdb_state_t *state,
                const uint8_t *frame, size_t frame_len,
                const sdb_peer_t *peer, uint32_t *out_id)
{
    sdb_entry_t *entry;
    size_t copy_len;

    if (state == NULL || frame == NULL || peer == NULL || out_id == NULL) {
        return SDB_ERR_NULL;
    }

    entry = find_empty_slot(state);
    if (entry == NULL) { return SDB_ERR_FULL; }

    memset(entry, 0, sizeof(*entry));
    entry->id = state->next_id++;
    entry->state = SDB_ENTRY_QUEUED;

    copy_len = (frame_len > SDB_FRAME_MAX) ? SDB_FRAME_MAX : frame_len;
    memcpy(entry->frame, frame, copy_len);
    entry->frame_len = copy_len;

    entry->peer = *peer;
    entry->retries = 0;
    entry->queued_at = (int64_t)time(NULL);
    entry->sent_at = 0;

    *out_id = entry->id;
    state->enqueued++;
    return SDB_OK;
}

int sdb_process_queue(sdb_state_t *state)
{
    size_t i;
    int sent_count = 0;

    if (state == NULL) { return SDB_ERR_NULL; }

    for (i = 0; i < SDB_MAX_QUEUE; i++) {
        sdb_entry_t *entry = &state->queue[i];
        if (entry->state != SDB_ENTRY_QUEUED) { continue; }

        /* Select carrier */
        uint8_t carrier_type = select_best_carrier(state, &entry->peer);
        if (carrier_type == SDB_CARRIER_NONE) {
            /* No carrier available — leave in queue */
            continue;
        }

        sdb_carrier_t *carrier = find_carrier_by_type(state, carrier_type);
        if (carrier == NULL || carrier->send == NULL) { continue; }

        entry->state = SDB_ENTRY_SENDING;
        entry->carrier_used = carrier_type;

        /* Get endpoint */
        const char *endpoint = get_endpoint_for_carrier(&entry->peer, carrier_type);

        /* Send */
        int rc = carrier->send(entry->frame, entry->frame_len, endpoint, carrier->ctx);

        if (rc == 0) {
            entry->state = SDB_ENTRY_SENT;
            entry->sent_at = (int64_t)time(NULL);
            state->sent++;
            sent_count++;
        } else {
            entry->retries++;
            if (entry->retries >= 3) {
                entry->state = SDB_ENTRY_FAILED;
                state->failed++;
            } else {
                entry->state = SDB_ENTRY_QUEUED; /* retry later */
            }
        }
    }

    return sent_count;
}

uint8_t sdb_select_carrier(sdb_state_t *state, const sdb_peer_t *peer)
{
    if (state == NULL || peer == NULL) { return SDB_CARRIER_NONE; }
    return select_best_carrier(state, peer);
}

size_t sdb_get_queue_depth(sdb_state_t *state)
{
    size_t i, count = 0;
    if (state == NULL) { return 0; }
    for (i = 0; i < SDB_MAX_QUEUE; i++) {
        if (state->queue[i].state == SDB_ENTRY_QUEUED) {
            count++;
        }
    }
    return count;
}

int sdb_get_entry_state(sdb_state_t *state, uint32_t id, uint8_t *out_state)
{
    sdb_entry_t *entry;

    if (state == NULL || out_state == NULL) { return SDB_ERR_NULL; }

    entry = find_by_id(state, id);
    if (entry == NULL) { return SDB_ERR_NOT_FOUND; }

    *out_state = entry->state;
    return SDB_OK;
}

int sdb_mark_acked(sdb_state_t *state, uint32_t id)
{
    sdb_entry_t *entry;

    if (state == NULL) { return SDB_ERR_NULL; }

    entry = find_by_id(state, id);
    if (entry == NULL) { return SDB_ERR_NOT_FOUND; }

    entry->state = SDB_ENTRY_ACKED;
    return SDB_OK;
}
