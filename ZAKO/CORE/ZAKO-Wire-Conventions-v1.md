# ZAKO Wire Conventions
## Version 1.0

*June 1, 2026*

---

> The ZAKO Standard defines what records mean. This document defines how they travel — how the BitPads frame fields are populated, how sessions are opened, how Islands and sub-entities are addressed, how compound groups are assembled, and how daemon signals move across the C0 signal grid. A record that carries correct semantics but incorrect wire conventions is malformed under this Standard.

---

## 1. Purpose and Scope

This document is a sub-protocol of the ZAKO Standard. It defines the wire-level conventions that ZAKO imposes on the BitPads protocol family beyond what the BitPads and BitLedger specifications themselves require. These conventions are normative: all ZAKO distributions must follow them, and all ZAKO implementations must enforce them.

The BitPads and BitLedger specifications define the wire format. They permit degrees of freedom — field values whose interpretation is left to the application layer. ZAKO exercises those degrees of freedom in specific, defined ways. This document records those choices so that any two ZAKO distributions interoperate correctly at the record level.

The following are out of scope and addressed elsewhere:
- Semantic meaning of task_code verbs, domain values, and account pairs: ZAKO Codebook Standard
- pads-v1 URL generation and transmission encoding: ZAKO Transmission Adapters (forthcoming)
- Service-level behavioural contracts: individual Service Specification documents

---

## 2. Reference Architecture

### 2.1 The Address Space

Every ZAKO record is addressed by a four-level coordinate:

```
Island   →   Sub-Entity   →   Record Position
  |               |                  |
file_sep    record_sep + sub_entity  group_sep
(0–7)          (0–31)               (0–63)
```

These three Layer 2 separator values, taken together, serve as the organisational key for `telux-ledgerd`. They are not application metadata added to the record after the fact. They are present in the Layer 2 batch header that governs the transmission in which the record was carried, and they are extracted and stored as first-class indexed columns.

| Separator | Field | Range | ZAKO Assignment |
|-----------|-------|-------|-----------------|
| File Separator | `file_sep` | 0–7 | Island slot in the Personal Island |
| Record Separator | `record_sep` | 0–31 | Exchange relationship type within an Island |
| Group Separator | `group_sep` | 0–63 | Position within a relationship; 63 = period close |

### 2.2 The Two Address Spaces

ZAKO records exist in two address spaces:

**Personal Island address space** — the on-device sovereign ledger. Eight Island slots (file_sep 0–7), each a distinct sub-entity with its own namespace. The content of these slots is defined in §3.

**Work Island address space** — sub-entity numbers 16–31 in the PADS service. Work Islands are Islands defined and operated by the user for work contexts: clients, projects, service areas, active contracts. They share the PADS service container but have distinct ledger namespaces.

Work Island numbering (sub_entity 16–31) is assigned dynamically within the PADS service and is not defined by this Standard. The PADS Service Specification governs Work Island lifecycle.

---

## 3. Personal Island Sub-Entity Numbering

The Personal Island is the foundational ZAKO Island. Every ZAKO device has exactly one. It is the Sovereign's home container. Its sub-entity slots are statically assigned:

| sub_entity | Name | Content |
|------------|------|---------|
| 0 | System | Outstack power events, mode transitions, gate records, process class assignments |
| 1 | Identity | DID operations, capability grants, revocations, attestations, bind events |
| 2 | Exchange | Financial flows, payments, invoices, incoming receipts, Exchange Engine records |
| 3 | Journal | Private personal entries; RESTRICT_FORWARD always set; SELinux enforced |
| 4 | Health | Health domain records: vitals, medications, measurements, health tallies |
| 5 | Academy | Learning domain records: progress, credentials, study events, completions |
| 6 | People | Social entity records: contact definitions, relationship events, social notes |
| 7 | Places | Location domain records: place definitions, visit events, geo-anchored records |

Sub-entity slots 8–15 are reserved for future Personal Island expansion by the ZAKO Standard. Sub-entity slots 16–31 are the Work Island namespace managed by the PADS service.

### 3.1 Sub-Entity and Island Addressing

The `file_sep` value maps directly to the Personal Island sub-entity:

| file_sep | Sub-entity | Service |
|----------|------------|---------|
| 0 | System (0) | Outstack |
| 1 | Identity (1) | telux-identd |
| 2 | Exchange (2) | Exchange Engine |
| 3 | Journal (3) | Journal (private) |
| 4 | Health (4) | Health Island |
| 5 | Academy (5) | Academy service |
| 6 | People (6) | Social graph service |
| 7 | Places (7) | Location service |

For Work Islands (sub_entity 16–31), the `file_sep` value is 0 (the Personal Island container), and the sub_entity field in the record identity component carries the Work Island number. The PADS service interprets sub_entity 16–31 as Work Island namespace.

---

## 4. Session Header Conventions

### 4.1 The BitPads Session Header

Every transmission in the BitPads protocol opens with a session header that declares the Layer 1 context for all frames that follow. ZAKO uses this header in the following prescribed manner.

### 4.2 Domain Field (2 bits, Layer 1)

The domain field declares the primary domain of the session. It does not prevent individual records from carrying custom domain extensions — those are declared in the record's own extension byte. The session domain field declares the dominant domain so that receivers can route without parsing every record:

| Value | Session Domain | Typical Use |
|-------|---------------|-------------|
| 00 | Financial | Exchange Engine sessions, payment batches, tally records |
| 01 | Engineering | Outstack power sessions, sensor batches, resource records |
| 10 | General | Work records, health records, journal entries, mixed sessions |
| 11 | Custom | Session contains custom domain records (extension byte follows) |

If domain=11, the custom domain byte immediately follows the session header. ZAKO uses the same custom domain byte values defined in the ZAKO Codebook Standard §4.

### 4.3 Compound Mode Permission (Session Configuration Extension Byte)

When a ZAKO session will include compound records — multi-leg transactions grouped atomically — the session configuration extension byte must be present and the compound mode permission bit must be set. ZAKO policy:

- Compound mode is **always permitted** in Exchange Engine sessions (domain=00)
- Compound mode is **always permitted** in Identity sessions (file_sep=1)
- Compound mode is **conditionally permitted** in PADS sessions (set when the work record batch includes linked payment legs)
- Compound mode is **never permitted** in Journal sessions (file_sep=3); a malformed Journal session with compound mode set must be rejected by telux-ledgerd

### 4.4 CRC-15 Scope

When a session includes Full BitLedger records, the CRC-15 is computed over the session header bytes. ZAKO enforces this:
- CRC-15 validation is mandatory for all Full BitLedger frames
- A frame presenting as Full BitLedger with a failing CRC-15 is rejected without partial posting
- Full Record frames (not Full BitLedger) do not carry a CRC-15; ZAKO does not require or simulate one for these frames

---

## 5. Batch Header Conventions

### 5.1 Layer 2 Batch Separator Conventions

Every Layer 2 batch opens with a batch header that sets the three separator values governing all records in the batch. ZAKO populates these as follows.

**File Separator (file_sep, 0–7):**  
Set to the Island slot of the dominant records in the batch. Mixed-Island batches are permitted but unusual. When a batch carries records from multiple Island slots, the file_sep is set to the slot of the first record; subsequent records carry their own sub_entity field to indicate slot.

**Record Separator (record_sep, 0–31):**  
Encodes the exchange relationship type governing the batch. ZAKO assigns record_sep values within each service domain. The full record_sep assignment table is maintained by each Service Specification. Core assignments reserved across all services:

| record_sep | Meaning |
|------------|---------|
| 0 | Self-record (no counterparty; unilateral event) |
| 1 | Peer exchange (bilateral; both parties present) |
| 2 | Broadcast (one-to-many; no individual counterparty) |
| 3 | Request (initiating leg only; response pending) |
| 4 | Response (responding leg; references request) |
| 5 | Delegation (capability flowing from superior to subordinate) |
| 6 | Revocation (capability withdrawal) |
| 7 | Correction (correcting record; references target) |
| 8–15 | Service-reserved (defined by individual Service Specification) |
| 16–31 | Work Island relationship types (defined by PADS Service Specification) |

**Group Separator (group_sep, 0–62):**  
Encodes the position of this record within the batch relationship context. ZAKO uses this as a sequence counter within the record_sep group: first record = 0, second = 1, and so on.

**Group Separator 63 — Period Close Convention:**  
`group_sep=63` is the ZAKO convention for a State Commit record that closes a period. A record with `group_sep=63` is always a Tally: it carries `task_code=COMMIT` (0x14) and `account_pair=1101`. It locks the balance of the period defined by the preceding records in the batch. This convention applies across all domains — financial, work, health, power, academy.

When a batch contains a Period Close record, it must be the last record in the batch. No records may follow a `group_sep=63` record in the same batch.

### 5.2 Bell Character Batch Boundary Convention

The BitPads Layer 2 Bell character marks a batch boundary — the end of one batch and the potential beginning of another within the same session. ZAKO uses Bell boundaries for the following purposes:

- **Period close:** Before writing a Tally record (`group_sep=63`), a Bell boundary is emitted. The Tally stands alone in its own batch.
- **Domain transition:** When a session transitions from one primary domain to another (e.g., a work batch followed by a financial batch), a Bell boundary separates them.
- **Compound group boundary:** The Bell is never emitted in the middle of an open compound group. If a Bell would occur while a compound group is pending, it is deferred until the compound group closes.

---

## 6. RESTRICT_FORWARD Flag Contract

### 6.1 Definition

The RESTRICT_FORWARD flag is a 1-bit field in the Full Record and Full BitLedger frames. When set, it declares that the record must not be forwarded, exported, relayed, or shared beyond the receiving party. It is not an advisory. It is a wire-level instruction that ZAKO services must enforce.

### 6.2 The JOURNAL Invariant

**Protocol requirement:** Any record with `task_code=0x2A` (JOURNAL) MUST have `RESTRICT_FORWARD=1`.

This is an invariant, not a policy. A JOURNAL record with `RESTRICT_FORWARD=0` is malformed under this Standard. ZAKO implementations must enforce this at two levels:

**Application level:** `telux-ledgerd` rejects any JOURNAL record presented for storage without `RESTRICT_FORWARD=1`. The write fails with an error. It is not corrected silently.

**System level:** The SELinux policy for the Journal sub-entity (sub_entity=3, file_sep=3) prohibits the system sharing daemon (`telux-sharedb`) from accessing records in the Journal namespace. The capability to share from file_sep=3 does not exist in the permission model.

Any ZAKO distribution that silently stores a malformed JOURNAL record (RESTRICT_FORWARD absent or zero) fails conformance.

### 6.3 RESTRICT_FORWARD in Other Contexts

RESTRICT_FORWARD may be set on any record type — not only JOURNAL records. Its meaning is consistent:

- The record was created for the receiver and is not to leave the receiver's device
- The receiver's sovereign signature is sufficient; no further distribution
- The record does not appear in any shared feed, export, or relay channel

Common non-Journal uses of RESTRICT_FORWARD:
- Health records the user marks as device-only
- Notes that annotate an incoming exchange but are not part of the exchange's shared record
- Internal daemon signals that are meaningful only to the device generating them

RESTRICT_FORWARD does not affect how the record is stored — only whether it may leave. Internally, a RESTRICT_FORWARD record is indexed, chain-hashed, and treated identically to any other record. Its privacy is a property of its transmission, not its storage.

---

## 7. Compound Record Conventions

### 7.1 Opening and Closing a Compound Group

When a ZAKO transmission includes a compound group, the following wire sequence applies:

```
[Session Header with compound mode enabled]
[Batch Header: file_sep, record_sep, group_sep for the compound group]
Record A:  Completeness=1 (Partial)   ← opens the compound group
Record B:  account_pair=1111, sub_type, Completeness=1   ← continuation
...
Record N:  account_pair=1111, sub_type, Completeness=0   ← closes the group
```

The entire compound group is atomic. telux-ledgerd holds all records pending until the closing record arrives. If the session terminates before the closing record, all held records are discarded without posting.

### 7.2 ZAKO Standard Compound Patterns

The following compound patterns are defined by this Standard. Implementations must be prepared to receive and produce these patterns correctly.

---

#### Pattern A: Capability Grant with Expiry Constraint

**Purpose:** A capability grant that carries both the grant scope and its expiry condition as a single atomic unit.

```
Record 1 — Grant Leg (Completeness=1, Partial):
  file_sep    = 1          ← Identity sub-entity
  record_sep  = 5          ← Delegation relationship
  group_sep   = 0
  task_code   = 0x08       ← GRANT
  account_pair = 0110      ← Reservation/Escrow
  value        = capability_scope_bitmask
  sender_id    = island_sovereign_id

Record 2 — Expiry Leg (1111 continuation, sub_type=00, Completeness=1):
  account_pair = 1111
  sub_type     = 00        ← Standard parallel leg
  task_code    = 0x0B      ← EXPIRE
  value        = expiry_epoch_seconds

Record 3 — Recipient Acknowledgement (1111 continuation, sub_type=00, Completeness=0):
  account_pair = 1111
  sub_type     = 00
  task_code    = 0x0F      ← VERIFY
  sender_id    = grantee_sovereign_id
```

---

#### Pattern B: Work Record with Payment Leg

**Purpose:** A PADS work record and its associated financial leg posted atomically.

```
Record 1 — Work Leg (Completeness=1, Partial):
  file_sep     = 0          ← PADS / Personal Island
  record_sep   = 1          ← Peer exchange
  group_sep    = 0
  domain       = 10         ← General (Work domain)
  task_code    = 0x20       ← START or DELIVER
  account_pair = work relationship archetype
  value        = work units completed
  sender_id    = work_island_sovereign_id

Record 2 — Payment Leg (1111 continuation, sub_type=00, Completeness=0):
  account_pair = 1111
  sub_type     = 00         ← Standard parallel leg
  domain       = 00         ← Financial (switches domain within compound group)
  task_code    = 0x18       ← PAY
  account_pair = 0001       ← Outflow / Credit
  value        = payment_amount
```

Note: Switching domain within a compound group is permitted. The compound group may span a domain boundary. The session header domain field declares the primary domain; individual records carry domain override bytes as needed.

---

#### Pattern C: Multi-Axis Vital Reading (e.g., Blood Pressure)

**Purpose:** A health measurement that requires two simultaneously-captured values (systolic and diastolic) as a single logical record.

```
Record 1 — Primary Measurement (Completeness=1, Partial):
  file_sep     = 4          ← Health sub-entity
  record_sep   = 0          ← Self-record (no counterparty)
  group_sep    = <sequence>
  domain       = 11         ← Custom; extension byte follows
  domain_ext   = 0x03       ← Health domain
  task_code    = 0x28       ← MEASURE
  account_pair = 0000       ← Raw measurement (no direction)
  value        = systolic_mmhg
  quantity_type = 0x02      ← Systolic Blood Pressure

Record 2 — Secondary Measurement (1111 continuation, sub_type=00, Completeness=0):
  account_pair = 1111
  sub_type     = 00         ← Standard parallel leg
  value        = diastolic_mmhg
  quantity_type = 0x03      ← Diastolic Blood Pressure
```

The two records share the same wall_ts from the first record's time component. The receiver treats both values as captured at the same instant.

---

#### Pattern D: Place Definition

**Purpose:** A location entity record that carries both a geographic anchor and a human-readable name.

```
Record 1 — Geo Anchor (Completeness=1, Partial):
  file_sep     = 7          ← Places sub-entity
  record_sep   = 0          ← Self-record
  group_sep    = <sequence>
  domain       = 11         ← Custom
  domain_ext   = 0x06       ← Location domain
  task_code    = 0x0C       ← CREATE
  account_pair = 0000       ← Entity instantiation
  value        = encoded_lat_long   ← Scaled integer pair

Record 2 — Name Annotation (1111 continuation, sub_type=00, Completeness=0):
  account_pair = 1111
  sub_type     = 00
  task_code    = 0x29       ← NOTE
  note_block   = place_name_utf8
```

---

#### Pattern E: Compound Correction

**Purpose:** Amending a previously stored record in the same session without deletion.

```
Record 1 — Original Record (already stored; referenced by frame_hash)

Record 2 — Correction Open (Completeness=1, Partial):
  Same file_sep, record_sep
  task_code   = same as original record
  value       = corrected_value

Record 3 — Correction Reference (1111 continuation, sub_type=01, Completeness=0):
  account_pair = 1111
  sub_type     = 01        ← Correcting sub-type
  value        = original_record_frame_hash_truncated
```

The sub_type=01 (Correcting) continuation signals that Record 2 is an amendment. telux-ledgerd stores Record 2 as the current value and marks the original record as superseded. The original record is never deleted; its seq remains in the ledger.

---

#### Pattern F: Reversal

**Purpose:** Full reversal of a prior record (e.g., revoking a capability grant, reversing a payment leg).

```
Record 1 — Reversal Record (standalone or compound open):
  task_code    = 0x09       ← REVOKE (for a grant) or appropriate reversal verb
  account_pair = 1111
  sub_type     = 10         ← Reversal sub-type
  Completeness = 0          ← If standalone; 1 if more legs follow
  value        = target_frame_hash_truncated
```

A Reversal record with sub_type=10 creates a new ledger entry that cancels the accounting effect of the targeted record. The targeted record is marked reversed. Neither record is deleted.

---

## 8. C0 Enhancement Signal Slot Assignments

### 8.1 The C0 Enhancement Grid

The BitPads Enhancement Sub-Protocol v2.0 defines 13 signal slot positions in the C0 control character range. Each slot is a 1-byte Pure Signal with a declared identity position. ZAKO assigns specific meanings to these slots for inter-daemon communication.

The 13 slots occupy Unicode control character positions: 0x01–0x06 (SOH through ACK), 0x0B–0x0C (VT, FF), 0x0E–0x0F (SO, SI), 0x10–0x13 (DLE through DC3). Each position carries:
- Priority bits (2): CRITICAL / INTERACTIVE / BACKGROUND / OPPORTUNISTIC
- ACK bit (1): whether a response is expected
- Continuation bit (1): whether more signals follow
- Category bits (4): signal identity within the slot

ZAKO uses the following slot assignments:

### 8.2 Outstack Power Signals (Slots 0x01–0x04)

These signals are emitted by `outstack-powerd`. All listeners on the system bus receive and must process them.

| Slot | Signal | Meaning | Priority |
|------|--------|---------|----------|
| 0x01 | POWER_CRITICAL | Battery < configured critical threshold (default 5%) | CRITICAL |
| 0x02 | POWER_LOW | Battery < configured low threshold (default 20%) | INTERACTIVE |
| 0x03 | MODE_ENTER | Outstack mode transition — category bits encode new mode | INTERACTIVE |
| 0x04 | POWER_STABLE | Battery charging; previously constrained resources may resume | BACKGROUND |

The `MODE_ENTER` signal category bits (4 bits, 16 values) encode the Outstack mode being entered:

| Category | Outstack Mode |
|----------|--------------|
| 0x0 | Full Power |
| 0x1 | Standard |
| 0x2 | Conservation |
| 0x3 | Low Power |
| 0x4 | Critical Reserve |
| 0x5–0xE | Reserved |
| 0xF | Emergency (custom signal; not standard mode transition) |

### 8.3 telux-ledgerd Signals (Slots 0x05–0x06)

| Slot | Signal | Meaning | Priority |
|------|--------|---------|----------|
| 0x05 | LEDGER_ACK | Record successfully stored; category bits encode Island slot (0–7) | BACKGROUND |
| 0x06 | LEDGER_REJECT | Record rejected; category bits encode rejection reason | INTERACTIVE |

The `LEDGER_REJECT` category bits encode:

| Category | Rejection Reason |
|----------|-----------------|
| 0x0 | JOURNAL invariant violation (RESTRICT_FORWARD absent) |
| 0x1 | Conservation invariant failure (BitLedger batch does not balance) |
| 0x2 | CRC-15 failure |
| 0x3 | Compound group incomplete at session end |
| 0x4 | Chain hash mismatch |
| 0x5 | Signature verification failure |
| 0x6 | Reserved domain value |
| 0x7 | Compound mode not permitted for this session |
| 0x8–0xE | Reserved |
| 0xF | Unknown / implementation-defined rejection |

### 8.4 telux-identd Signals (Slots 0x0B–0x0C)

| Slot | Signal | Meaning | Priority |
|------|--------|---------|----------|
| 0x0B | IDENTITY_READY | DID resolved and key material available for signing | BACKGROUND |
| 0x0C | IDENTITY_LOCKED | Key material locked (device locked or TrustZone unavailable) | INTERACTIVE |

When `IDENTITY_LOCKED` is active, telux-ledgerd queues signed records in an unsigned staging area. Records are signed and posted once `IDENTITY_READY` is received. Staged unsigned records do not appear in any Service View.

### 8.5 Exchange Engine Signals (Slots 0x0E–0x0F)

| Slot | Signal | Meaning | Priority |
|------|--------|---------|----------|
| 0x0E | EXCHANGE_PENDING | An incoming exchange record awaits counterparty acceptance | INTERACTIVE |
| 0x0F | EXCHANGE_SETTLED | Exchange accepted and both legs posted to ledger | INTERACTIVE |

### 8.6 PADS Service Signals (Slots 0x10–0x11)

| Slot | Signal | Meaning | Priority |
|------|--------|---------|----------|
| 0x10 | WORK_ASSIGNED | A task record with outbound assignment has been ledger-confirmed | BACKGROUND |
| 0x11 | WORK_DELIVERED | A DELIVER record from a work island has been received and stored | BACKGROUND |

### 8.7 Learning Engine Signal (Slot 0x12)

| Slot | Signal | Meaning | Priority |
|------|--------|---------|----------|
| 0x12 | LEARNING_READY | A deferred learning batch is ready to process (battery and conditions permitting) | OPPORTUNISTIC |

Learning Engine signals are always OPPORTUNISTIC priority. Outstack gates their execution based on the current power mode. In Critical Reserve mode, LEARNING_READY signals are ignored by all learning daemons.

### 8.8 System Heartbeat (Slot 0x13)

| Slot | Signal | Meaning | Priority |
|------|--------|---------|----------|
| 0x13 | SYSTEM_HEARTBEAT | Periodic alive signal from ZAKO supervisor; category bits encode system health index | BACKGROUND |

The system health index (4 bits, 0–15):

| Value | Meaning |
|-------|---------|
| 0 | All services nominal |
| 1–3 | One or more non-critical services degraded |
| 4–7 | PADS or Exchange Engine degraded |
| 8–11 | telux-ledgerd degraded; records queuing |
| 12–14 | Multiple critical services degraded |
| 15 | System entering emergency state; non-critical services halting |

---

## 9. Meta Byte Conventions

### 9.1 The Meta Byte

Every BitPads transmission opens with a Meta byte. ZAKO uses the Meta byte fields as follows:

**Frame Type (bits 7–6):**
ZAKO does not redefine frame type codes. The BitPads specification defines:
- `00` — Pure Signal
- `01` — Anonymous Wave
- `10` — Full Record
- `11` — Full BitLedger

ZAKO uses all four. Frame type determines the parser path; ZAKO adds no ZAKO-specific frame types.

**Priority (bits 5–4):**
ZAKO maps its process class model onto BitPads priority bits:

| Priority Bits | ZAKO Process Class | Outstack Context |
|---------------|-------------------|-----------------|
| `11` | CRITICAL | Exchange Engine writes, telux-ledgerd writes, Outstack POWER_CRITICAL events |
| `10` | INTERACTIVE | User-initiated records, Exchange pending signals, mode transition records |
| `01` | BACKGROUND | PADS work records, health logs, Academy completions, ledger ACK signals |
| `00` | OPPORTUNISTIC | Learning Engine batches, deferred AI processing, Academy background sync |

When Outstack constrains process execution by power mode, in-flight records' priority bits inform telux-ledgerd's posting queue discipline: CRITICAL records post immediately regardless of power mode; OPPORTUNISTIC records are queued until Outstack permits execution.

**ACK Bit (bit 3):**
Set on records that require a ledger confirmation signal. Exchange Engine records always set ACK=1. PADS work records set ACK=1 when assignment is outbound (the work is being sent to another party). Journal records set ACK=0 (no external party to acknowledge).

**Continuation Bit (bit 2):**
Set when the transmission will contain additional frames after this one. Cleared on the last frame of a transmission. ZAKO uses this to allow streaming implementations to begin parsing before the full transmission has arrived.

**Category/Context (bits 1–0):**
For Pure Signal frames: encodes the first 2 bits of the signal category, with the remaining 2 bits in the signal body. For other frame types: ZAKO leaves bits 1–0 at `00` unless a specific protocol requires otherwise (no ZAKO convention assigns meaning to these bits in non-signal frames).

### 9.2 Transmission Opening Sequence

Every ZAKO transmission follows this opening sequence:

```
[Meta byte]
[Session Header — if this is a new session]
[Session Configuration Extension — if compound mode or custom domain]
[Layer 1 Custom Domain byte — if domain=11]
[Layer 2 Batch Header — file_sep, record_sep, group_sep]
[Record(s)]
[Bell boundary — if batch terminates before session]
[Next batch header + records — if session continues]
[Session close — end of transmission]
```

A Pure Signal transmission omits everything after the Meta byte. The signal is self-contained in 1 byte.

---

## 10. telux-ledgerd Storage Conventions

### 10.1 Column Mapping from Wire Fields

When telux-ledgerd stores a record, it extracts the following wire fields into indexed columns:

| Column | Source | Notes |
|--------|--------|-------|
| `island_id` | Device identity + Island slot | Derived from sender_id + file_sep |
| `sub_entity` | Record identity component sub-entity field | 0–31 |
| `file_sep` | Layer 2 batch header | 0–7 |
| `record_sep` | Layer 2 batch header | 0–31 |
| `group_sep` | Layer 2 batch header | 0–63; 63 = Period Close |
| `task_code` | Full Record task component | 0x00–0x3F |
| `domain` | Layer 1 session header domain + extension | 0–3 base; extension byte if domain=11 |
| `account_pair` | Full BitLedger Layer 3 | 0x0–0xF; 0xF = Compound Continuation |
| `direction` | BitLedger direction bit | 0=Outflow, 1=Inflow |
| `status` | BitLedger status bit | 0=Pending, 1=Settled |
| `value_raw` | Scaled value integer | Stored as raw integer; scale factor in batch context |
| `restrict_fwd` | RESTRICT_FORWARD flag | 0 or 1; enforced by SELinux for file_sep=3 |
| `wall_ts` | Record time component | Unix epoch seconds |
| `lamport_ts` | Monotonic sequence counter | Incremented by telux-ledgerd at each write |
| `frame_bytes` | Complete frame | Binary blob; canonical record |
| `frame_hash` | BLAKE3 over frame_bytes | UNIQUE; serves as record identifier |
| `chain_hash` | BLAKE3 over frame_hash + prev chain_hash | Chain integrity; NULL for first record |
| `source_did` | Sender DID from identity component | W3C DID; NULL if anonymous |
| `dest_did` | Destination DID if present | W3C DID; NULL if broadcast/self |
| `sovereign_sig` | ed25519 signature | Over frame_hash; NOT NULL |

### 10.2 Chain Hash Computation

The chain hash provides tamper-evidence across consecutive records:

```
chain_hash[n] = BLAKE3(frame_hash[n] || chain_hash[n-1])
```

For the first record in an Island's ledger, `chain_hash[0] = BLAKE3(frame_hash[0] || island_genesis_hash)`, where `island_genesis_hash` is the BLAKE3 hash of the Island creation record.

The chain hash is computed by telux-ledgerd at write time, not by the sender. The sender provides the frame; telux-ledgerd computes the chain position. This ensures chain continuity even when records arrive from external sources.

### 10.3 Period Close and Tally Records

A Tally record (`task_code=COMMIT`, `account_pair=1101`, `group_sep=63`) has the following additional storage behaviour:

1. telux-ledgerd computes the period balance before storing the Tally: the sum of all signed flows in the Island slot (file_sep) from the previous Tally record to this one.
2. The Tally record's `value_raw` field must equal this computed balance. If it does not match, the Tally record is rejected with `LEDGER_REJECT` (category 0x1 — Conservation invariant failure).
3. After a Tally record is stored, subsequent records in the same Island slot begin a new period. The Tally `frame_hash` serves as the period anchor for the next period's chain hash.

---

## 11. Conformance Requirements

A ZAKO distribution conforms to this document when:

1. **Sub-entity numbering is respected.** Personal Island sub-entity slots 0–7 are used only for their defined purposes. No service writes to a sub-entity slot other than its own without an explicit capability grant.

2. **JOURNAL invariant is enforced.** Records with `task_code=0x2A` (JOURNAL) are rejected by telux-ledgerd if `RESTRICT_FORWARD=0`. The SELinux policy prohibits the sharing daemon from accessing file_sep=3.

3. **Group Separator 63 is reserved.** Only Tally records (`task_code=COMMIT`, `account_pair=1101`) may carry `group_sep=63`. A non-Tally record with `group_sep=63` is rejected.

4. **Compound mode is correctly gated.** Compound groups are not opened in sessions where compound mode was not declared in the Session Configuration Extension byte. A compound record arriving in a non-compound session is rejected.

5. **Bell boundaries respect compound groups.** No Bell boundary is emitted while a compound group is open.

6. **Priority bits map to Outstack process classes.** Incoming records with priority bits `11` (CRITICAL) are not gated by Outstack regardless of power mode. Incoming records with priority bits `00` (OPPORTUNISTIC) may be deferred in Conservation mode and queued in Low Power and Critical Reserve modes.

7. **C0 signal slots are not reassigned.** Slots 0x01–0x13 as defined in §8 are not reused for distribution-specific signals. Distributions may define additional signal slots using C0 positions not reserved by this document.

8. **Chain hash is unbroken.** The chain hash sequence for each Island ledger must be contiguous. A gap in the chain — a record with a chain_hash that does not match the expected continuation — must be flagged and must not be silently accepted.

---

## Appendix A: Wire Field Quick Reference

| Field | Bits | Location | ZAKO Constraint |
|-------|------|----------|-----------------|
| Frame type | 2 | Meta byte [7:6] | Standard BitPads codes; no ZAKO addition |
| Priority | 2 | Meta byte [5:4] | Maps to Outstack process class (§9.1) |
| ACK | 1 | Meta byte [3] | Required for Exchange Engine records |
| Continuation | 1 | Meta byte [2] | Standard; no ZAKO addition |
| Domain | 2 | Session header Layer 1 | 00–10 standard; 11 = custom (extension byte follows) |
| file_sep | 3 | Layer 2 batch header | 0–7 = Personal Island slot (§3) |
| record_sep | 5 | Layer 2 batch header | 0–7 = ZAKO core (§5.1); 8–15 service; 16–31 Work Islands |
| group_sep | 6 | Layer 2 batch header | 0–62 = position; 63 = Period Close / Tally |
| sub_entity | 5 | Record identity component | 0–7 = Personal Island (§3); 16–31 = Work Islands |
| task_code | 6 | Full Record task component | See ZAKO Codebook Standard |
| account_pair | 4 | BitLedger Layer 3 | 1101 = Tally (with group_sep=63); 1111 = Compound Continuation |
| RESTRICT_FORWARD | 1 | Full Record / Full BitLedger | Mandatory=1 when task_code=0x2A (JOURNAL) |
| group_sep=63 | — | Batch header | Tally record only; must be last record in batch |

---

## Appendix B: Compound Sub-Type Reference

| Sub-type Bits | Name | Ledger Effect |
|---------------|------|--------------|
| 00 | Standard | Parallel leg; posts with opening record atomically |
| 01 | Correcting | Amends preceding record; original marked superseded |
| 10 | Reversal | Reverses preceding record; both entries remain in ledger |
| 11 | Cross-batch | Compound group spans a batch boundary; deferred until close |

---

## Appendix C: Rejection Code Summary

| Code | Signal | Reason |
|------|--------|--------|
| 0x0 | LEDGER_REJECT | JOURNAL invariant: RESTRICT_FORWARD absent on task_code=0x2A |
| 0x1 | LEDGER_REJECT | Conservation: BitLedger batch does not balance |
| 0x2 | LEDGER_REJECT | CRC-15 failure on Full BitLedger frame |
| 0x3 | LEDGER_REJECT | Compound group incomplete at session end |
| 0x4 | LEDGER_REJECT | Chain hash mismatch |
| 0x5 | LEDGER_REJECT | Sovereign signature verification failure |
| 0x6 | LEDGER_REJECT | Reserved domain value in record |
| 0x7 | LEDGER_REJECT | Compound mode not declared for this session |
| 0xF | LEDGER_REJECT | Implementation-defined rejection |
