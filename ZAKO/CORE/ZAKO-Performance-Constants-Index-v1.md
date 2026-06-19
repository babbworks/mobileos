# ZAKO Performance Constants Index
## Version 1.0

*June 1, 2026*

---

> This document is a reference index of all numerical constants, thresholds, timing values, capacity limits, and tunable parameters defined across the ZAKO Standard and its sub-protocols. Each entry names the constant, gives its default value, states the document and section that defines it, and notes whether it is Sovereign-adjustable at runtime, distribution-configurable at build time, or fixed by protocol.

---

**Adjustability codes:**

| Code | Meaning |
|------|---------|
| **FIXED** | Defined by protocol; no implementation may change it |
| **DIST** | Distribution-configurable at build/image time; not adjustable at runtime |
| **DIST+SOV** | Distribution sets the range; Sovereign may override within that range at runtime |
| **SOV** | Sovereign-adjustable at runtime within a protocol-defined range |

---

## 1. Power Governance (Outstack Protocol)

### 1.1 Mode Entry Thresholds (battery %)

| Constant | Default | Sovereign Override Range | Adjustability |
|----------|---------|--------------------------|---------------|
| STANDARD_FLOOR | 50% | 30%–70% | DIST+SOV |
| CONSERVE_FLOOR | 20% | 10%–35% | DIST+SOV |
| LOW_FLOOR | 10% | 5%–20% | DIST+SOV |
| SHUTDOWN_FLOOR | 3% | 1%–8% | DIST+SOV |

Mode transitions occur when battery percentage crosses these thresholds downward. Transitions upward require exceeding the threshold by the HYSTERESIS_BAND.

### 1.2 Hysteresis

| Constant | Default | Sovereign Override Range | Adjustability |
|----------|---------|--------------------------|---------------|
| HYSTERESIS_BAND | 3% | 1%–10% | DIST+SOV |

Applied in both directions: entering Conservation at 20% requires returning to 23% (with default band) before returning to Standard.

### 1.3 Opportunistic Execution Floor

| Constant | Default | Sovereign Override Range | Adjustability |
|----------|---------|--------------------------|---------------|
| OPPORTUNISTIC_FLOOR | 70% | 50%–90% | DIST+SOV |

OPPORTUNISTIC processes only run when battery exceeds this floor OR the device is charging. Applies in Full Power mode only.

### 1.4 Sampling Intervals

| Constant | Default | Sovereign Override Range | Adjustability |
|----------|---------|--------------------------|---------------|
| SAMPLE_INTERVAL_FULL | 30 seconds | 15s–120s | DIST+SOV |
| SAMPLE_INTERVAL_STD | 60 seconds | 30s–180s | DIST+SOV |
| SAMPLE_INTERVAL_CONS | 120 seconds | 60s–300s | DIST+SOV |
| SAMPLE_INTERVAL_CRIT | 300 seconds | 120s–600s | DIST+SOV |
| SAMPLE_INTERVAL_EMRG | 30 seconds | 15s–60s | DIST+SOV |

Emergency mode uses a fast interval to track rapid discharge rate. In CONSTRAINED_POWER hardware profile, all intervals are doubled.

### 1.5 Battery Delta Trigger

| Constant | Default | Sovereign Override Range | Adjustability |
|----------|---------|--------------------------|---------------|
| SAMPLE_THRESHOLD | 5% | 1%–10% | DIST+SOV |
| SAMPLE_THRESHOLD_EMRG | 1% | fixed | FIXED |

A POWER_CHANGE record is written to the power ledger when battery percentage changes by more than SAMPLE_THRESHOLD since the last record. In Emergency mode the threshold is fixed at 1%.

### 1.6 Thermal Override

| Constant | Default | Sovereign Override Range | Adjustability |
|----------|---------|--------------------------|---------------|
| THERMAL_CRITICAL | 45°C | 40°C–55°C | DIST |
| THERMAL_SAFE | 40°C | 35°C–48°C | DIST |
| THERMAL_CRITICAL_SUSTAIN | 60 seconds | 30s–120s | DIST |
| THERMAL_SAFE_SUSTAIN | 120 seconds | 60s–300s | DIST |

Emergency mode is triggered when temperature exceeds THERMAL_CRITICAL sustained for THERMAL_CRITICAL_SUSTAIN seconds. Recovery requires temperature below THERMAL_SAFE sustained for THERMAL_SAFE_SUSTAIN seconds.

### 1.7 Charging Adjustments

| Constant | Default | Adjustability |
|----------|---------|---------------|
| FAST_CHARGE_THRESHOLD | 10 W | DIST |
| FAST_CHARGE_MODE_BONUS | +1 mode level | FIXED |
| FAST_CHARGE_BATTERY_FLOOR | 30% | DIST |

When charging rate exceeds FAST_CHARGE_THRESHOLD, Outstack operates one mode less restrictive than battery percentage alone would indicate. This bonus applies only when battery > FAST_CHARGE_BATTERY_FLOOR.

### 1.8 Mode Transition Anti-Flap

| Constant | Default | Adjustability |
|----------|---------|---------------|
| TRANSITION_CONFIRM_SAMPLES | 1 additional sample | FIXED |

Before any downward mode transition, outstack-powerd takes one additional sample to confirm the threshold crossing. This prevents a single anomalous reading from triggering a mode change.

### 1.9 Delegation Depth Limit

| Constant | Default | Sovereign Override Range | Adjustability |
|----------|---------|--------------------------|---------------|
| MAX_DELEGATION_DEPTH | 3 levels | — | DIST |

Maximum chain length for capability delegation (DELEGATE records). A DELEGATE record that would create a chain deeper than this limit is rejected by telux-identd.

---

## 2. Ledger and Signing Timing (Telux Protocol / telux-ledgerd)

### 2.1 Durability and Write Latency

| Constant | Value | Adjustability |
|----------|-------|---------------|
| LEDGER_FSYNC_POLICY | Synchronous before ACK | FIXED |
| LEDGER_ACK_REQUIRES_FSYNC | Always | FIXED |

Every record is fsynced to storage before LEDGER_ACK is emitted. There is no deferred-fsync option. This is a FIXED protocol guarantee — a conforming implementation cannot batch writes without fsyncing each before ACK.

### 2.2 Service Response Timing

| Constant | Default | Adjustability |
|----------|---------|---------------|
| CRITICAL_CAPABILITY_REISSUE_TIMEOUT | 500 ms | DIST |
| BACKGROUND_SUSPENSION_CONFIRM | 2 seconds | DIST |
| IDENTITY_LOCK_SIGN_REFUSAL | Immediate | FIXED |

CRITICAL services must respond to capability re-issuance (after a mode change) within CRITICAL_CAPABILITY_REISSUE_TIMEOUT. BACKGROUND services must confirm suspension within BACKGROUND_SUSPENSION_CONFIRM of receiving the suspension signal.

### 2.3 Staged Record Processing

| Constant | Value | Adjustability |
|----------|-------|---------------|
| STAGING_QUEUE_ORDER | FIFO | FIXED |
| STAGING_QUEUE_MAX_AGE | No limit | — |
| STAGING_QUEUE_FLUSH_ON_UNLOCK | Immediate | FIXED |

Staged unsigned records (queued while telux-identd is locked) are processed in FIFO order immediately on device unlock. There is no maximum age — staged records are never discarded by timeout.

### 2.4 Shutdown Sequence

| Constant | Value | Adjustability |
|----------|-------|---------------|
| SHUTDOWN_LEDGER_FLUSH_REQUIRED | Yes — always | FIXED |
| SHUTDOWN_FLUSH_TIMEOUT | No defined timeout | — |

outstack-powerd does not permit device shutdown until telux-ledgerd confirms flush. There is no timeout on the flush wait — if telux-ledgerd is unresponsive, outstack-powerd escalates to Emergency mode and retries rather than bypassing the flush requirement.

### 2.5 Chain Hash Algorithm

| Constant | Value | Adjustability |
|----------|-------|---------------|
| CHAIN_HASH_ALGORITHM | BLAKE3 | FIXED |
| FRAME_HASH_ALGORITHM | BLAKE3 | FIXED |
| SOVEREIGN_SIG_ALGORITHM | ed25519 | FIXED |

All chain and frame hashes use BLAKE3. All sovereign signatures use ed25519. These are FIXED by the protocol — implementations may not substitute alternatives.

---

## 3. Identity and Capability (Telux Protocol)

### 3.1 Key Management

| Constant | Default | Adjustability |
|----------|---------|---------------|
| KEY_TIER_PREFERENCE | Hardware (TrustZone / Keymaster 4.0) | DIST |
| SOFTWARE_KEY_ENCRYPTION | Required | FIXED |
| KEY_EXPORT_PERMITTED | Never without explicit sovereign action | FIXED |

Private key material must be encrypted at rest in software-backed deployments. Key export without a signed sovereign action is prohibited. Hardware-backed deployments are the distribution preference; software-backed is the fallback.

### 3.2 Capability Revocation

| Constant | Value | Adjustability |
|----------|-------|---------------|
| REVOCATION_GRACE_PERIOD | None — immediate | FIXED |
| REVOCATION_CASCADE_DELEGATIONS | Yes | FIXED |

Revocation takes effect before the next capability check — no grace period. All downstream delegations derived from a revoked grant are also revoked; telux-identd traverses the delegation chain on every top-level revocation.

---

## 4. Wire Protocol Capacity (ZAKO Wire Conventions)

### 4.1 Separator Field Ranges

| Field | Range | Special Values | Adjustability |
|-------|-------|----------------|---------------|
| file_sep | 0–7 | — | FIXED |
| record_sep | 0–31 | 31 = cross-session reference | FIXED |
| group_sep | 0–62 | 63 = Period Close / Tally | FIXED |
| sub_entity | 0–31 | 0–7 = Personal Island; 16–31 = Work Islands | FIXED |

### 4.2 Pictography Limits

| Constant | Value | Adjustability |
|----------|-------|---------------|
| MAX_PICTOGRAPHY_SEQUENCE | 8 symbols (4 bytes) | FIXED |
| CONTEXT_DECLARATION_WAVE_SIZE | 4 bytes | FIXED |
| CODEBOOK_SYMBOL_WIDTH | 4 bits | FIXED |
| CODEBOOK_SIZE | 16 symbols | FIXED |

A pictographic sequence exceeding 8 symbols is a protocol error. Bytes beyond the 8th symbol are treated as raw data until the next Meta byte.

### 4.3 Session and Batch Limits

| Constant | Value | Adjustability |
|----------|-------|---------------|
| MAX_COMPOUND_DEPTH | No defined limit per session | — |
| BELL_BEFORE_TALLY | Required | FIXED |
| TALLY_MUST_BE_LAST | Yes — no records after group_sep=63 | FIXED |
| COMPOUND_CANNOT_SPAN_BELL | True | FIXED |

---

## 5. PADS Service Limits

| Constant | Default | Adjustability |
|----------|---------|---------------|
| MAX_ACTIVE_WORK_ISLANDS | 16 (sub_entity 16–31) | FIXED |
| WORK_ISLAND_NAME_MAX_BYTES | 64 bytes UTF-8 | FIXED |
| PADS_PROCESS_CLASS_USER | INTERACTIVE | FIXED |
| PADS_PROCESS_CLASS_SYNC | BACKGROUND | FIXED |

---

## 6. Agreements Service

| Constant | Default | Adjustability |
|----------|---------|---------------|
| MAX_AGREEMENT_RECORD_SEP_SLOTS | 32 (record_sep 0–31) | FIXED |
| HOSTED_NODE_COUNTERSIGN_LATENCY | 2 seconds | DIST |
| EXPIRE_RECORD_GENERATED_BY | telux-identd (automatic) | FIXED |

---

## 7. Health Service

| Constant | Default | Adjustability |
|----------|---------|---------------|
| REPRODUCTIVE_HEALTH_DEFAULT_RESTRICT | RESTRICT_FORWARD=1 | DIST+SOV |
| MENTAL_HEALTH_DEFAULT_RESTRICT | RESTRICT_FORWARD=1 | DIST+SOV |
| HEALTH_ALERT_PRIORITY | Always INTERACTIVE | FIXED |
| HEALTH_TALLY_VERIFY_POLICY | Store with unverified flag if mismatch | FIXED |
| SENSOR_DAEMON_PROCESS_CLASS | BACKGROUND | FIXED |
| SENSOR_DAEMON_ALERT_CLASS | INTERACTIVE | FIXED |

Health Tally mismatches (computed aggregate vs declared Tally value) are stored with an `unverified_tally` flag rather than rejected — unlike financial Tallies, health aggregates involve averaging which may differ by rounding.

---

## 8. Academy Service

| Constant | Default | Adjustability |
|----------|---------|---------------|
| LEARNING_ENGINE_PROCESS_CLASS | OPPORTUNISTIC | FIXED |
| SPACED_REPETITION_PROCESS_CLASS | BACKGROUND | FIXED |
| ASSESSMENT_PROCESS_CLASS | INTERACTIVE | FIXED |
| CONTENT_PREFETCH_PROCESS_CLASS | BACKGROUND | FIXED |
| MAX_CONTENT_DELIVERY_LATENCY_CACHED | 5 seconds | DIST |
| MAX_CONTENT_DELIVERY_LATENCY_ORIGIN | 30 seconds | DIST |
| XP_STORAGE_MODEL | Computed from records — no mutable balance | FIXED |
| STREAK_MISS_POLICY | Absence = break (no grace period by default) | DIST |

---

## 9. Infrastructure Extension Profile

| Constant | Default | Adjustability |
|----------|---------|---------------|
| LEDGER_NODE_COUNTERSIGN_LATENCY | 2 seconds | DIST |
| CONTENT_NODE_PRIVACY_POLICY | No DID-content association logging | FIXED |
| MAX_DELEGATION_DEPTH_INFRASTRUCTURE | 1 level (node cannot re-delegate) | FIXED |
| ENROLLMENT_REQUIRES_AGREE_RECORD | Yes | FIXED |
| NODE_SYNC_AUDIT_RECORDS | Required (SEND + RECEIVE pairs) | FIXED |

---

## 10. BitPads Frame Size Reference (informational)

| Frame Type | Size | ZAKO Use Context |
|------------|------|-----------------|
| Pure Signal | 1 byte | Outstack heartbeat, daemon ACK, power alert |
| Anonymous Wave | 4 bytes | Peripheral sensor readings, pictography preamble |
| Minimal Full Record | 13 bytes | Process class assignment, minimal work records |
| Full Record (with all components) | 29 bytes | Work records, journal entries, health readings |
| Full BitLedger (minimal) | 22 bytes | Payments, exchanges, tallies |
| Full BitLedger (with note) | 22–44+ bytes | Agreements, capability grants with annotations |

These sizes are fixed by the BitPads v2.0 specification and are informational here. They are not tunable.

---

## 11. Cryptographic Reference (informational)

| Parameter | Value | Source |
|-----------|-------|--------|
| Key algorithm | ed25519 | Telux Protocol |
| Hash algorithm | BLAKE3 | Telux Protocol |
| DID method | did:key (ed25519) | Telux Protocol |
| DID prefix | z6Mk | W3C did:key + multicodec 0xed01 |
| Chain hash input | BLAKE3(frame_hash[n] \|\| chain_hash[n-1]) | Telux Protocol |
| Genesis chain anchor | BLAKE3(frame_hash[0] \|\| zeros_32) | Telux Protocol |
| Island ID derivation | BASE58URL(BLAKE3(public_key \|\| epoch)) | Telux Protocol |
| CRC used for BitLedger | CRC-15 | BitLedger v3.0 spec |

---

## 12. Change Protocol for This Document

This index is updated whenever a sub-protocol document is created or modified and contains numerical constants. An entry in this index is authoritative only in citing its source document; if there is a conflict between this index and the source document, the source document governs. The index is a navigation aid, not a normative document in its own right.

To propose a change to a constant:
1. Propose the change in the relevant sub-protocol document
2. Update this index entry to match
3. Note the version of the sub-protocol document that introduced the change in a bracketed annotation: e.g., `[from Outstack Protocol v1.1]`

---

*ZAKO Performance Constants Index v1.0 — June 1, 2026*  
*Reference companion to ZAKO Standard v1.x and all sub-protocols*
