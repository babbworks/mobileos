/*
 * exchange_engine.c — Bilateral Exchange Engine Implementation
 *
 * Pure logic layer: manages exchange state, conservation checks,
 * atomic posting via callbacks.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "exchange_engine.h"
#include <string.h>
#include <time.h>

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

static exe_exchange_t *find_empty_slot(exe_engine_t *engine)
{
    size_t i;
    for (i = 0; i < EXE_MAX_EXCHANGES; i++) {
        if (engine->exchanges[i].state == EXE_STATE_EMPTY) {
            return &engine->exchanges[i];
        }
    }
    return NULL;
}

static exe_exchange_t *find_by_id(exe_engine_t *engine, uint32_t id)
{
    size_t i;
    for (i = 0; i < EXE_MAX_EXCHANGES; i++) {
        if (engine->exchanges[i].id == id &&
            engine->exchanges[i].state != EXE_STATE_EMPTY) {
            return &engine->exchanges[i];
        }
    }
    return NULL;
}

/*
 * Find a PENDING_OUTBOUND exchange with matching counterparty DID.
 * Used to match an incoming ACK to an outbound SEND.
 */
static exe_exchange_t *find_pending_outbound_for(exe_engine_t *engine,
                                                  const char *counterparty_did)
{
    size_t i;
    for (i = 0; i < EXE_MAX_EXCHANGES; i++) {
        if (engine->exchanges[i].state == EXE_STATE_PENDING_OUTBOUND &&
            strcmp(engine->exchanges[i].counterparty_did, counterparty_did) == 0) {
            return &engine->exchanges[i];
        }
    }
    return NULL;
}

static void populate_leg(exe_leg_t *leg, const uint8_t *frame, size_t frame_len,
                         const char *did, const uint8_t *sig,
                         uint32_t value_n, uint8_t direction)
{
    size_t copy_len = (frame_len > EXE_FRAME_MAX) ? EXE_FRAME_MAX : frame_len;
    memcpy(leg->frame, frame, copy_len);
    leg->frame_len = copy_len;
    strncpy(leg->did, did, EXE_DID_MAX - 1);
    leg->did[EXE_DID_MAX - 1] = '\0';
    memcpy(leg->sig, sig, 64);
    leg->value_n = value_n;
    leg->direction = direction;
    leg->timestamp = (int64_t)time(NULL);
}

/*
 * Conservation check: outflow + inflow must sum to zero.
 * From our perspective: our leg + their leg must balance.
 */
static int check_conservation(const exe_leg_t *leg_a, const exe_leg_t *leg_b)
{
    int64_t balance = 0;

    /* Direction 0 = in (adds value), direction 1 = out (subtracts) */
    if (leg_a->direction == 0) {
        balance += (int64_t)leg_a->value_n;
    } else {
        balance -= (int64_t)leg_a->value_n;
    }

    if (leg_b->direction == 0) {
        balance += (int64_t)leg_b->value_n;
    } else {
        balance -= (int64_t)leg_b->value_n;
    }

    return (balance == 0) ? EXE_OK : EXE_ERR_CONSERVATION;
}

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

int exe_init(exe_engine_t *engine, const exe_callbacks_t *callbacks)
{
    if (engine == NULL || callbacks == NULL) { return EXE_ERR_NULL; }

    memset(engine, 0, sizeof(*engine));
    engine->callbacks = *callbacks;
    engine->next_id = 1;

    return EXE_OK;
}

int exe_send(exe_engine_t *engine,
             const uint8_t *frame, size_t frame_len,
             const char *our_did, const uint8_t *our_sig,
             const char *counterparty,
             uint32_t value_n, uint32_t *out_id)
{
    exe_exchange_t *ex;

    if (engine == NULL || frame == NULL || our_did == NULL ||
        our_sig == NULL || counterparty == NULL || out_id == NULL) {
        return EXE_ERR_NULL;
    }

    ex = find_empty_slot(engine);
    if (ex == NULL) { return EXE_ERR_FULL; }

    memset(ex, 0, sizeof(*ex));
    ex->id = engine->next_id++;
    ex->state = EXE_STATE_PENDING_OUTBOUND;
    ex->created = (int64_t)time(NULL);
    strncpy(ex->counterparty_did, counterparty, EXE_DID_MAX - 1);

    /* Our SEND is an outflow (direction=1 from our perspective) */
    populate_leg(&ex->local_leg, frame, frame_len, our_did, our_sig, value_n, 1);
    ex->local_valid = 1;

    *out_id = ex->id;
    return EXE_OK;
}

int exe_receive(exe_engine_t *engine,
                const uint8_t *frame, size_t frame_len,
                const char *sender_did, const uint8_t *sig,
                uint32_t value_n, uint8_t direction,
                uint32_t *out_id)
{
    exe_exchange_t *ex;
    int rc;

    if (engine == NULL || frame == NULL || sender_did == NULL ||
        sig == NULL || out_id == NULL) {
        return EXE_ERR_NULL;
    }

    /* Verify signature if callback available */
    if (engine->callbacks.verify_sig != NULL) {
        rc = engine->callbacks.verify_sig(sender_did, frame, frame_len,
                                          sig, engine->callbacks.ctx);
        if (rc != 0) { return EXE_ERR_VERIFY; }
    }

    /* Check if this matches a pending outbound exchange (ACK to our SEND) */
    ex = find_pending_outbound_for(engine, sender_did);
    if (ex != NULL) {
        /* This is the ACK/response to our outbound SEND */
        populate_leg(&ex->remote_leg, frame, frame_len,
                     sender_did, sig, value_n, direction);
        ex->remote_valid = 1;
        *out_id = ex->id;
        /* Don't auto-complete here — caller should call exe_complete_outbound */
        return EXE_OK;
    }

    /* New inbound exchange — create PENDING_INBOUND */
    ex = find_empty_slot(engine);
    if (ex == NULL) { return EXE_ERR_FULL; }

    memset(ex, 0, sizeof(*ex));
    ex->id = engine->next_id++;
    ex->state = EXE_STATE_PENDING_INBOUND;
    ex->created = (int64_t)time(NULL);
    strncpy(ex->counterparty_did, sender_did, EXE_DID_MAX - 1);

    populate_leg(&ex->remote_leg, frame, frame_len,
                 sender_did, sig, value_n, direction);
    ex->remote_valid = 1;

    *out_id = ex->id;
    return EXE_OK;
}

int exe_acknowledge(exe_engine_t *engine, uint32_t exchange_id,
                    const uint8_t *ack_frame, size_t ack_len,
                    const char *our_did, const uint8_t *our_sig,
                    uint32_t our_value, uint8_t our_direction)
{
    exe_exchange_t *ex;
    int rc;

    if (engine == NULL || ack_frame == NULL || our_did == NULL || our_sig == NULL) {
        return EXE_ERR_NULL;
    }

    ex = find_by_id(engine, exchange_id);
    if (ex == NULL) { return EXE_ERR_NOT_FOUND; }
    if (ex->state != EXE_STATE_PENDING_INBOUND) { return EXE_ERR_STATE; }
    if (!ex->remote_valid) { return EXE_ERR_STATE; }

    /* Populate our leg */
    populate_leg(&ex->local_leg, ack_frame, ack_len,
                 our_did, our_sig, our_value, our_direction);
    ex->local_valid = 1;

    /* Conservation check */
    rc = check_conservation(&ex->local_leg, &ex->remote_leg);
    if (rc != EXE_OK) {
        ex->state = EXE_STATE_FAILED;
        engine->failed_count++;
        return EXE_ERR_CONSERVATION;
    }

    /* Post atomically */
    if (engine->callbacks.post_atomic != NULL) {
        rc = engine->callbacks.post_atomic(
            ex->remote_leg.frame, ex->remote_leg.frame_len,
            ex->local_leg.frame, ex->local_leg.frame_len,
            0, engine->callbacks.ctx);
        if (rc != 0) {
            ex->state = EXE_STATE_FAILED;
            engine->failed_count++;
            return EXE_ERR_POST;
        }
    }

    ex->state = EXE_STATE_COMPLETED;
    engine->completed_count++;
    return EXE_OK;
}

int exe_complete_outbound(exe_engine_t *engine, uint32_t exchange_id,
                          const uint8_t *ack_frame, size_t ack_len,
                          const char *ack_did, const uint8_t *ack_sig,
                          uint32_t ack_value, uint8_t ack_dir)
{
    exe_exchange_t *ex;
    int rc;

    if (engine == NULL || ack_frame == NULL || ack_did == NULL || ack_sig == NULL) {
        return EXE_ERR_NULL;
    }

    ex = find_by_id(engine, exchange_id);
    if (ex == NULL) { return EXE_ERR_NOT_FOUND; }
    if (ex->state != EXE_STATE_PENDING_OUTBOUND) { return EXE_ERR_STATE; }
    if (!ex->local_valid) { return EXE_ERR_STATE; }

    /* Verify counterparty signature */
    if (engine->callbacks.verify_sig != NULL) {
        rc = engine->callbacks.verify_sig(ack_did, ack_frame, ack_len,
                                          ack_sig, engine->callbacks.ctx);
        if (rc != 0) { return EXE_ERR_VERIFY; }
    }

    /* Populate remote leg */
    populate_leg(&ex->remote_leg, ack_frame, ack_len,
                 ack_did, ack_sig, ack_value, ack_dir);
    ex->remote_valid = 1;

    /* Conservation check */
    rc = check_conservation(&ex->local_leg, &ex->remote_leg);
    if (rc != EXE_OK) {
        ex->state = EXE_STATE_FAILED;
        engine->failed_count++;
        return EXE_ERR_CONSERVATION;
    }

    /* Post atomically */
    if (engine->callbacks.post_atomic != NULL) {
        rc = engine->callbacks.post_atomic(
            ex->local_leg.frame, ex->local_leg.frame_len,
            ex->remote_leg.frame, ex->remote_leg.frame_len,
            0, engine->callbacks.ctx);
        if (rc != 0) {
            ex->state = EXE_STATE_FAILED;
            engine->failed_count++;
            return EXE_ERR_POST;
        }
    }

    ex->state = EXE_STATE_COMPLETED;
    engine->completed_count++;
    return EXE_OK;
}

int exe_cancel(exe_engine_t *engine, uint32_t exchange_id)
{
    exe_exchange_t *ex;

    if (engine == NULL) { return EXE_ERR_NULL; }

    ex = find_by_id(engine, exchange_id);
    if (ex == NULL) { return EXE_ERR_NOT_FOUND; }

    if (ex->state == EXE_STATE_COMPLETED) { return EXE_ERR_STATE; }

    ex->state = EXE_STATE_CANCELLED;
    engine->cancelled_count++;
    return EXE_OK;
}

int exe_get_state(exe_engine_t *engine, uint32_t exchange_id, uint8_t *out_state)
{
    exe_exchange_t *ex;

    if (engine == NULL || out_state == NULL) { return EXE_ERR_NULL; }

    ex = find_by_id(engine, exchange_id);
    if (ex == NULL) { return EXE_ERR_NOT_FOUND; }

    *out_state = ex->state;
    return EXE_OK;
}

size_t exe_get_pending_count(exe_engine_t *engine)
{
    size_t i, count = 0;
    if (engine == NULL) { return 0; }
    for (i = 0; i < EXE_MAX_EXCHANGES; i++) {
        if (engine->exchanges[i].state == EXE_STATE_PENDING_OUTBOUND ||
            engine->exchanges[i].state == EXE_STATE_PENDING_INBOUND) {
            count++;
        }
    }
    return count;
}
