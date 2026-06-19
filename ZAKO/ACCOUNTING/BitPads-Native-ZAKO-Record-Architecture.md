# BitPads Native: ZAKO Record Architecture Without the Workpads Layer

## What BitPads Itself Provides, and What a ZAKO-Native Record Schema Looks Like When Built Directly on It

*May 31, 2026*

---

## Preface

The previous document — *BitPads Protocol as the Accounting Core of ZAKO* — established that the pads-v1 codec and Workpads Standard are the correct integration path if ZAKO's records need to interoperate with the existing Workpads ecosystem. That is a real and valuable property. Zambian field workers using KaiOS will send invoices that Babb Cat should decode. This backward compatibility matters.

But a separate question deserves its own analysis: **what if ZAKO did not inherit the Workpads record schema at all?** What if, instead of adopting pads-v1 as the record format and extending it with ZAKO-specific templates, ZAKO built its record architecture directly on BitPads — using the protocol's native frame types, compound mode, Layer 2 separators, Full Record task and note components, and the C0 Enhancement Grammar?

This is not a marginal distinction. pads-v1 was designed for a specific application domain: field service records for contractors and small businesses, optimised for URL transmissibility over SMS and rendered through a template engine. ZAKO is an operating system. Its record needs — power events, capability delegation, Island transitions, AI agent actions, sovereign identity operations, resource accounting across hardware peripherals — do not map naturally onto "job, customer, date, location, story." Inheriting that schema means inheriting its assumptions.

BitPads itself — below the Workpads application layer, at the level of the protocol family specification — provides four native building blocks that together can express ZAKO's entire record space: the Full Record frame with its task and note components, compound mode with 1111 continuation markers, Layer 2 hierarchical separators, and the C0 Enhancement Grammar with its binary pictography codebook. This document examines each, constructs a ZAKO-native record architecture from them, and assesses what is gained and what is traded against the pads-v1 approach.

---

## Part One: What BitPads Natively Provides

### 1.1 The Transmission Spectrum as a Record Vocabulary

BitPads defines four frame types as a continuous spectrum. Each frame type is self-describing from the first byte. Each is optimal for a specific expression cost:

```
1 byte    Pure Signal     Priority + ACK + Continuation flags + C0 identity code
4 bytes   Anonymous Wave  Session-context value; no identity overhead
13 bytes  Minimal Record  Sender identity + value; new-session cost included
29 bytes  Full Record     Identity + value + time + task + note; all four components
22+ bytes Full BitLedger  Double-entry record; CRC-15 session integrity; conservation enforced
```

The critical observation: this spectrum is not five separate formats. It is one protocol with five expression points on a single cost axis. The same Meta byte architecture governs all five. A receiver that knows the protocol decodes any frame type from the same parsing path.

For ZAKO, this means the record vocabulary is already defined. ZAKO does not need to invent record types — it needs to map its semantic needs onto the appropriate point in the spectrum:

- **Outstack mode heartbeat:** 1 byte — Pure Signal with the current mode encoded as a C0 category (5 bits, 32 possible values; Outstack has 5 modes, fits in 3 bits leaving 2 for status)
- **Peripheral power reading:** 4 bytes — Anonymous Wave carrying milliwatts as a scaled value, session context establishes which peripheral
- **Process class assignment:** 13 bytes — Minimal Record with Sender ID (process PID encoded as sender) and value (class code as account pair)
- **Work record with description:** 29 bytes — Full Record with time (job start), task (job type code), note (description text)
- **Financial payment between entities:** 22+ bytes — Full BitLedger with CRC-15, conservation enforced against all flows in the batch

The per-frame cost is not overhead. It is the exact cost of what is being expressed.

### 1.2 The Full Record's Four Native Components

The Full Record (13–29 bytes) is BitPads' richest single frame type below the full BitLedger. It carries four optional components that attach only when present:

**Identity component** (always present in Full Record, included in the 13-byte minimum):
- Sender ID (32-bit): the origin node — a device, a process, an entity
- Sub-Entity ID (5-bit): subdivision of the sender — an Island, a service, a subprocess
- Three-level identity split available: 8-bit network / 8-bit system / 16-bit node (255 networks × 255 systems × 65,535 nodes)

**Value component** (always present in Full Record):
- Scaled integer using `N = A × 2^S + r` encoding
- No gaps from 0 to 33,554,431
- Decimal position and scaling factor inherited from Layer 2 batch context
- Four tiers of precision from 1-byte compact to 3-byte extended

**Time component** (optional; adds 1–2 bytes):
- T1S (Tier 1 Short): 1 byte — 8-bit time field encoding hour (0–23) and 15-minute blocks; sufficient for scheduling context in constrained records
- T2 (Tier 2): 2-byte extended time word (`--time-ext`, 0–65535) — full date and time expression

**Task component** (optional; adds 2–3 bytes):
- `task_code` (6 bits, 0–63): semantic category of the task — 64 distinct task types in a shared codebook
- `task_target` (variable): the entity or resource the task acts on
- `task_timing` (variable): timing constraint on the task (deadline, earliest start, duration)

**Note component** (optional; variable length):
- Free-text annotation block attached to the record
- The only variable-length component in the Full Record
- Carries the human-readable description that the structured fields cannot express

These four components — **identity, value, time, task, note** — are ZAKO's native record dimensions. Not "job, customer, date, location, story." Identity: who. Value: how much. Time: when. Task: what kind of action. Note: the human description of it. This is a more fundamental schema than Workpads' field service model, and it is already encoded in the protocol specification.

The maximum Full Record without BitLedger: 29 bytes. This is the correct size for ZAKO daemon inter-process communication, where URL encoding is irrelevant and binary efficiency is the constraint.

### 1.3 Compound Mode: Records Within Records

Compound mode is the BitLedger mechanism for grouping multiple records into one logical transaction. It operates through the 1111 account pair code, gated by a session-level permission flag in the Session Configuration Extension byte.

When compound mode is active:
- A record with `Completeness=1` (Partial) opens a compound group — the decoder holds it pending
- A record with `account_pair=1111` (Compound Continuation) continues the group
- A 1111 record with `Completeness=0` (Full) closes the group — all held records post atomically

The 1111 continuation has four sub-types (bits 37–38 in the continuation record):

| Sub-type | Meaning | ZAKO Use |
|----------|---------|----------|
| 00 Standard | Parallel leg of same event | Financial payment + capability grant for same exchange |
| 01 Correcting | This record corrects the preceding record's error | Amendment to a prior Island exchange record |
| 10 Reversal | This record fully reverses the preceding record | Revocation of a capability grant |
| 11 Cross-batch | Continuation spans a batch boundary | Multi-phase transaction crossing session periods |

**What "records within records" actually means in BitPads:**

A compound group is a set of records that must be applied together or not at all. The protocol guarantees this atomicity at the wire level — not as an application-layer promise, but as a structural property of the encoding. If the final 1111 Completeness=0 record does not arrive, the decoder holds all prior records pending indefinitely. Nothing posts.

For ZAKO, this resolves a significant design question. The previous document assumed that the chain protocol (from Workpads Standard) was needed to link related records. Compound mode in BitLedger provides a stronger guarantee: it links records that are part of one logical event, enforces their atomic posting, and distinguishes sub-types (standard/correcting/reversal/cross-batch) at the wire level.

**A ZAKO compound event example — capability grant with acknowledgement:**

```
Record A (opens compound group):
  account_pair = 0110 (Reservation/Escrow — capability reserved for grantee)
  Completeness = 1 (Partial — more follows)
  value = capability_scope_bitmask (encoded as 24-bit integer)
  task_code = 0x08 (GRANT in ZAKO task codebook)
  sender_id = island_sovereign_id
  sub_entity = island_id

Record B (1111 Compound Continuation, sub-type 00 Standard):
  account_pair = 1111
  sub_type = 00 (standard parallel leg)
  Completeness = 1 (Partial — more follows)
  value = expiry_timestamp
  task_code = 0x09 (EXPIRE_CONSTRAINT)

Record C (1111 Compound Continuation, sub-type 00, closes group):
  account_pair = 1111
  sub_type = 00
  Completeness = 0 (Full — compound group closed)
  value = delegation_depth (0=not delegatable; 1–7=delegatable N levels)
  note = "Capability: access Island storage; granted by Sovereign"

Decoder action:
  Receives A: enter compound watch, hold A
  Receives B: continue holding, hold B
  Receives C: Completeness=0, close group
  Post A + B + C atomically as one logical grant event
  Conservation check: capability scope issued = capability scope recorded (same value)
```

This is a richer structure than a pads-v1 chain reference. The chain reference links records that were created at different times by different parties across multiple sessions. The compound group links records that are parts of one logical event within one session, with wire-level atomicity guaranteed.

ZAKO needs both:
- **Compound groups** for atomic multi-leg events (grant + constraint + confirmation)
- **Session separators** for linking records across time (see section 1.4)

The distinction is important. pads-v1's chain protocol was trying to do both jobs with one mechanism. BitPads separates them cleanly.

### 1.4 Layer 2 Hierarchical Separators

Layer 2 (the batch header, 6 bytes) carries three separator fields that create a three-level organizational hierarchy within a session:

```
File Separator     (3 bits, 0–7):    highest level — logical context or project
Record Separator   (5 bits, 0–31):   intermediate — exchange relationship or chain
Group Separator    (6 bits, 0–63):   lowest level — batch within a relationship
```

These are counter values — monotonically incrementing identifiers within the session. The combination of (File Separator, Record Separator, Group Separator) uniquely identifies a position in the session's record space.

**As a chain mechanism:**

Instead of a separate chain reference appended to every URL (the `&c=<4-char>` tag in pads-v1), BitPads' separators identify exchange contexts at the batch level. All records within one batch that share the same (File, Record, Group) separator combination belong to one logical context.

For ZAKO:
- **File Separator** identifies the Island (up to 8 distinct Islands per session — sufficient for the personal Island plus 7 active working Islands; ZAKO can open a new session when more are needed)
- **Record Separator** identifies the exchange relationship (up to 31 distinct exchange relationships per Island per session)
- **Group Separator** identifies the position within a relationship (up to 63 batches in one relationship — sufficient for any realistic multi-step exchange)

The Layer 2 bells provide ACK/inquiry signalling without a separate record:
- **Enquiry Bell** (bit): sender requests acknowledgement before the next batch proceeds
- **Acknowledge Bell** (bit): receiver confirms receipt of the previous batch

These replace pads-v1's ACK record type for the high-frequency case. An ACK is 1 bit in Layer 2 rather than an 80-byte Full Record. For ZAKO daemon communication (power events, process class updates, Island state changes), this matters.

**Separator convention for ZAKO:**

```
File Separator values:
  0: Personal Island (always exists)
  1–7: Active working Islands (numbered by Island slot)

Record Separator values:
  0: System events (power, mode transitions, process lifecycle)
  1: Sovereign operations (identity, capability, delegation)
  2–30: Exchange relationships (one per active bilateral exchange)
  31: Cross-session reference (links to prior session's separator state)

Group Separator values:
  0: Session open (initialization records)
  1–62: Ordered positions within this exchange relationship
  63: Session close (state commit, conservation balance verification)
```

This convention replaces Workpads' chain_anchor + participant_slot + sequence (24-bit composite). It is less information-dense (BitPads' three separator fields total 14 bits versus pads-v1's 24-bit chain reference) but more structurally meaningful — the separator values carry organizational semantics that the chain reference does not.

### 1.5 The C0 Enhancement Grammar

The C0 Enhancement Grammar reclaims 3 structurally available bits in Unicode control characters (bytes 0–31). These 3 bits are: **Priority** (bit 7), **ACK Request** (bit 6), **Continuation** (bit 5). The lower 5 bits carry the original C0 identity (0–31 — the Baudot-heritage control character codes).

**Signal slot positions (13 total):**

```
P1:   Session boundary (Layer 1 / Layer 2 junction)
P2:   Batch open
P3:   Batch close
P4–P8: Component boundaries within a record (5 positions)
P9–P13: Stream boundaries (5 positions)
```

Enhanced C0 bytes occupy declared signal slot positions. Content bytes occupy non-signal positions. The decoder always knows which it is reading because position determines interpretation, not content.

For ZAKO's inter-daemon communication:

- **P1 (session boundary):** Mode transition signal — a C0 byte at the session boundary with Priority=1 signals an Outstack mode change before any records are processed. A receiving daemon immediately knows power mode has changed before decoding any of the batch's records.
- **P2 (batch open):** Island context signal — identifies which Island the following batch pertains to. A daemon managing multiple Islands sets this once per batch rather than including island_id in every record.
- **P4–P8 (component boundaries):** Task-specific signals — at the task component boundary, a C0 byte with Continuation=1 signals that the task has additional sub-records in compound mode.
- **P9–P13 (stream boundaries):** Priority escalation — at stream boundaries, Priority=1 + ACK Request=1 requires the receiver to halt its current queue and process the incoming record before proceeding.

**Binary pictography:**

When a stream has a declared Category Identity (established in Layer 2 or via a Context Declaration Wave), compact 4-bit nibble sequences decode through a shared codebook to full semantic events. The receiver does not parse text. It decodes concept-symbols from a shared lookup.

For ZAKO, a dedicated ZAKO Category Identity in the C0 Enhancement codebook unlocks:

```
ZAKO Pictography Codebook (ZAKO Category Identity = 0x0F):

0x0: FULL (Outstack mode FULL)
0x1: NORMAL (Outstack mode NORMAL)
0x2: CONSERVE (Outstack mode CONSERVE)
0x3: CRITICAL (Outstack mode CRITICAL)
0x4: EMERGENCY (Outstack mode EMERGENCY)
0x5: GRANT (capability granted)
0x6: REVOKE (capability revoked)
0x7: JOIN (entity joined Island)
0x8: LEAVE (entity left Island)
0x9: COMMIT (state commit, exchange closed)
0xA: AMEND (prior record amended)
0xB: DISPUTE (chain disputed)
0xC: ACK (acknowledgement)
0xD: SUSPEND (process suspended by Outstack)
0xE: RESUME (process resumed)
0xF: ALERT (system alert, human attention required)
```

At 4 bits per symbol, a sequence of four pictographs fits in 2 bytes. A mode transition record that would otherwise require a full BitLedger record with task and note components can be expressed as a 2-byte pictography sequence: `[mode_symbol][transition_reason_symbol]`. This is Sumerian accounting at the wire level — the mark is minimal, the shared codebook carries the meaning.

---

## Part Two: A ZAKO-Native Record Schema

### 2.1 The Schema Without Workpads

Building ZAKO's record schema from BitPads primitives — without inheriting pads-v1's field definitions — produces a fundamentally different architecture. Instead of "records are structured field sets encoded as a binary field-flag header + length-prefixed text blocks," the ZAKO-native schema is:

> Records are BitPads frames. Every frame carries exactly what it needs to carry: identity from Layer 1, batch context from Layer 2, conservation accounting from Layer 3, and annotation from the Full Record's task and note components. Nothing more.

The field schema is not a list of named text fields. It is a set of encoded dimensions:

| Dimension | Encoding | Source |
|-----------|----------|--------|
| Sender identity | Layer 1 Sender ID (32-bit) + Sub-Entity (5-bit) | BitPads Layer 1 |
| Domain | Layer 1 Domain (2-bit: financial/engineering/hybrid/custom) | BitPads Layer 1 |
| Batch context | Layer 2 Scaling Factor, Decimal Position, Currency/Qty Type | BitPads Layer 2 |
| Organizational position | Layer 2 File/Record/Group separators (3+5+6 bits) | BitPads Layer 2 |
| Value | Layer 3 scaled integer (0–33,554,431 × scaling factor) | BitLedger Layer 3 |
| Relationship type | Layer 3 Account Pair (4-bit, 14 pairs + correction + compound) | BitLedger Layer 3 |
| Direction + Status | Layer 3 bits 29–30 (mirrored in bits 37–38) | BitLedger Layer 3 |
| Time | Full Record T1S (1 byte) or T2 (2 bytes) | BitPads Record |
| Task type | Full Record task_code (6-bit, 64 categories) | BitPads Record |
| Task target | Full Record task_target (variable) | BitPads Record |
| Task timing | Full Record task_timing (variable) | BitPads Record |
| Annotation | Full Record note block (variable-length text) | BitPads Record |
| Atomicity links | 1111 Compound Continuation (sub-type 00/01/10/11) | BitLedger Compound |
| Signal flags | C0 Enhancement Grammar (Priority, ACK, Continuation at P1–P13) | BitPads Enhancement |
| Pictography | ZAKO Category codebook (4 bits/symbol, 16 concepts) | BitPads Enhancement |

What is absent from this list:
- Named text fields for "job," "customer," "location," "worker"
- URL encoding and deflate compression
- Workpads chain reference (replaced by Layer 2 separators)
- pads-v1 template rendering system
- FLAG1/FLAG2 field presence bitmaps

These are all application-layer constructs in pads-v1. BitPads does not need them because it encodes everything in structured bit positions.

For ZAKO's system-layer records (power events, process class transitions, capability operations, Island membership), named text fields are unnecessary. A power event has a value (milliwatts), a direction (in/out), a time, and a task code (CHARGE/CONSUME/GATE/RESTORE). It does not have a customer name.

For ZAKO's user-facing records (work records, payment records, job descriptions), the note block provides free text. The trade is: instead of "customer: Alice, job: Electrical inspection, location: Site 4," the note block contains "Alice — electrical inspection — Site 4." Less structured; equally expressive; more compressed (no field headers, no repeated empty fields).

### 2.2 ZAKO Task Codebook

The task_code field (6 bits, 64 values) is BitPads' semantic shorthand for the type of action a record represents. ZAKO defines a 64-entry task codebook that covers all ZAKO-specific operations plus enough general-purpose entries for user-facing work records:

```
System domain (0x00–0x0F): Outstack and OS operations
  0x00  POWER_CHARGE      Charging event (power entering system)
  0x01  POWER_CONSUME     Consumption event (power leaving system)
  0x02  POWER_GATE        Hardware power gating (peripheral disabled)
  0x03  POWER_RESTORE     Hardware power gate lifted
  0x04  MODE_TRANSITION   Outstack mode transition (value = new mode)
  0x05  PROCESS_ASSIGN    Process assigned to class (value = class code)
  0x06  PROCESS_SUSPEND   Process suspended by Outstack
  0x07  PROCESS_RESUME    Process resumed

Identity domain (0x08–0x0F):
  0x08  GRANT             Capability granted
  0x09  REVOKE            Capability revoked
  0x0A  DELEGATE          Capability delegated to sub-entity
  0x0B  EXPIRE            Capability expired (time-based)
  0x0C  DID_CREATE        DID identity created
  0x0D  DID_BIND          DID bound to device key
  0x0E  ATTEST            Attestation event (signed assertion)
  0x0F  VERIFY            Verification event (assertion checked)

Island domain (0x10–0x17):
  0x10  ISLAND_CREATE     Island created (value = island_id)
  0x11  ISLAND_DISSOLVE   Island dissolved
  0x12  MEMBER_JOIN       Entity joined Island
  0x13  MEMBER_LEAVE      Entity left Island
  0x14  MEMBER_REMOVE     Entity removed from Island (by Sovereign)
  0x15  LEDGER_COMMIT     Island ledger state commit
  0x16  LEDGER_AMEND      Prior Island record amended
  0x17  LEDGER_DISPUTE    Chain disputed; arbitration requested

Exchange domain (0x18–0x1F):
  0x18  SEND              Record sent to counterparty
  0x19  RECEIVE           Record received from counterparty
  0x1A  ACKNOWLEDGE       Receipt acknowledged
  0x1B  INVOICE           Payment requested
  0x1C  PAYMENT           Payment made
  0x1D  QUOTE             Quote or estimate
  0x1E  COMMIT            Both parties committed
  0x1F  COMPLETE          Exchange fully complete

Work domain (0x20–0x2F): User-facing work records
  0x20  JOB               Job or task assigned
  0x21  START             Work started
  0x22  PROGRESS          Progress update
  0x23  COMPLETE_WORK     Work completed
  0x24  INSPECT           Inspection performed
  0x25  REPORT            Report submitted
  0x26  EXPENSE           Expense recorded
  0x27  MATERIAL          Material or supply consumed
  0x28  TRAVEL            Travel event
  0x29  NOTE_ONLY         Annotation without transaction
  0x2A  PHOTO             Photo or attachment reference
  0x2B  SIGNATURE         Signature event
  0x2C–0x2F  (reserved for work extensions)

Resource domain (0x30–0x37): Non-financial resource flows
  0x30  DATA_SEND         Data transmitted
  0x31  DATA_RECEIVE      Data received
  0x32  STORAGE_WRITE     Storage written
  0x33  STORAGE_READ      Storage read
  0x34  BANDWIDTH_USE     Bandwidth consumed
  0x35  COMPUTE_USE       Compute cycles consumed
  0x36  SENSOR_READ       Sensor reading recorded
  0x37  CALIBRATE         Sensor or system calibrated

AI and automation domain (0x38–0x3F):
  0x38  AI_QUERY          AI agent queried
  0x39  AI_RESPONSE       AI agent responded
  0x3A  AI_ACT            AI agent took autonomous action
  0x3B  AI_DEFER          AI agent deferred (waiting for human)
  0x3C  SCHEDULE          Event scheduled
  0x3D  TRIGGER           Scheduled event triggered
  0x3E  AUTOMATION_ON     Automation rule activated
  0x3F  AUTOMATION_OFF    Automation rule deactivated
```

64 task codes, organized in 8 semantic domains of 8 each. Every ZAKO record type maps to one. The task_code replaces pads-v1's BASE_TEMPLATE (which distinguished invoice from job from state_commit) with a richer, more precisely named vocabulary that covers system operations pads-v1 was never designed for.

### 2.3 Full Record Composition: Three Examples

**Example 1: Power mode transition — 13 bytes**

```
Meta Byte 1:
  bit 7: SOH=1
  bits 6-4: Domain=00 (engineering)
  bits 3-2: Frame=10 (Full Record)
  bit 1: Enhancement=1 (C0 signal slots active)
  bit 0: Continuation=0

Meta Byte 2:
  bits 7-6: Time=01 (T1S, 1-byte time field present)
  bit 5: Task=1 (task component present)
  bit 4: Note=0 (no note — mode transition is fully structured)
  bit 3: Value=1
  bits 2-0: Tier=001 (1-byte compact value)

Layer 1 Session Header (8 bytes):
  SOH=1, Version=0, Domain=01 (engineering)
  Permissions: Read=1, Write=1, Correct=1, Represent=0
  Sender ID = outstack_powerd process identity
  Sub-Entity = SYSTEM_ISLAND (Island 0)
  CRC-15 computed

T1S Time field (1 byte):
  hours=14 (2PM), quarter=2 (14:30)

Task (2 bytes):
  task_code = 0x04 (MODE_TRANSITION)
  task_target = 0x02 (CONSERVE — the new mode)

Value (1 byte, compact):
  battery_pct = 32 (32% remaining, encoded as integer)

C0 Signal at P2 (batch open, 1 byte):
  C0 identity = FS (File Separator, 0x1C)
  Priority = 1 (mode transition is priority event)
  ACK = 0 (not requesting ACK)
  Continuation = 0

Total: 13 bytes
```

This 13-byte record completely describes a mode transition: when it happened, what the new mode is, what triggered it (battery level), and that it is a priority event. No URL encoding. No compression step. No field-presence bitmaps. No variable-length text fields. Every byte earns its position.

**Example 2: Work record with job description — 29 bytes**

```
Meta Byte 1: SOH=1, Domain=00 (financial/general), Frame=10 (Full Record), Enhancement=1
Meta Byte 2: Time=01 (T1S), Task=1, Note=1, Value=1, Tier=010 (2-byte value)

Layer 1 (8 bytes):
  Domain=00 (general/financial)
  Sender ID = worker entity identity (maps to DID in ZAKO identity registry)
  Sub-Entity = Personal Island (Island 0)
  Permissions = Read+Write (worker can read and write their own records)
  CRC-15

Layer 2 Batch Context (6 bytes):
  Scaling Factor = 0 (×1, value in whole currency units)
  Decimal Position = 2 (2 decimal places — currency)
  Currency = ZMW (Zambian Kwacha — 6-bit currency code)
  Record Separator = 5 (exchange relationship #5 — this employer)
  Group Separator = 12 (12th record in this relationship)
  Compound Prefix = 00 (no compound groups in this batch)

T1S Time field (1 byte): 08:15 start

Task (2 bytes):
  task_code = 0x21 (START)
  task_target = 0x23 (COMPLETE_WORK — target state of this task chain)

Value (2 bytes): 45000 → ZMW 450.00 (agreed rate)

Note (variable, up to 7 bytes to stay at 29 total):
  "Elec inspect Site 4"  — 19 bytes. Record expands to 36 bytes total.

Layer 2 Enquiry Bell = 1 (request ACK from supervisor)

Total: ~36 bytes
```

Compare to pads-v1 equivalent: the same record as a pads-v1 URL is approximately 280–320 characters (after deflate compression and base64url encoding). The BitPads Full Record is 36 bytes — 36 bytes raw binary versus 280–320 bytes URL. For daemon-to-daemon communication inside ZAKO, or for Bluetooth LE transmission between devices, the 36-byte binary is correct. For SMS transmission to a KaiOS counterparty, pads-v1 is correct.

ZAKO does not choose between them. It stores the 36-byte binary locally and generates the pads-v1 URL on demand for external sharing. The binary is the canonical form; the URL is the transmission form for specific channels.

**Example 3: Compound capability grant with reversal provision — 3 records**

```
Batch opens with compound mode = ON (Session Config Extension byte)
Layer 2 Compound Prefix = 01 (up to 3 compound groups)
File Separator = 1 (Island 1 — work Island)
Record Separator = 0 (Sovereign operations)

Record A (opens compound group):
  Frame: Full BitLedger (22+ bytes)
  Account Pair = 0110 (Reservation/Escrow — capability reserved)
  Completeness = 1 (Partial)
  Direction = 0 (grant flowing to grantee)
  Status = 1 (Pending — requires grantee acknowledgement)
  Value = 0x0038 (capability_scope bitmask: STORAGE_READ + ISLAND_EXCHANGE + JOB_CREATE)
  task_code = 0x08 (GRANT)
  time = current timestamp
  note = "Worker access: Site 4 Island"

Record B (1111 Continuation, Sub-type 00, Completeness=1):
  Account Pair = 1111
  Sub-type = 00 (standard parallel leg)
  Completeness = 1 (more follows)
  Value = expiry (Unix timestamp 30 days from now)
  task_code = 0x0B (EXPIRE)

Record C (1111 Continuation, Sub-type 10, Completeness=0, CLOSES GROUP):
  Account Pair = 1111
  Sub-type = 10 (Reversal — this defines the reversal path for Record A)
  Completeness = 0 (compound group closed — A+B+C post atomically)
  Value = 0x0009 (REVOKE + MEMBER_REMOVE — what happens on expiry or dispute)
  task_code = 0x09 (REVOKE)
  note = "Auto-revoke on expiry or dispute"

Decoder: receives A (hold), receives B (hold), receives C (Completeness=0, close group)
Posts A + B + C atomically. Conservation check: capability_scope issued = capability_scope in reversal path.
```

This three-record compound group defines a capability grant, its expiry constraint, and its reversal procedure — atomically. If Record C is never received (network loss, transmission error), none of the records post. The grant does not exist in a half-formed state. This is exactly the atomicity guarantee ZAKO's capability system requires.

---

## Part Three: What Is Gained and What Is Traded

### 3.1 Gains Over the pads-v1 Approach

**Conservation enforcement for ALL record types.**

In pads-v1, conservation is an application-layer concern. The Workpads Standard has no mechanism for verifying that the sum of all flows in a batch equals zero. Each pads-v1 record stands alone — there is no batch-level algebraic check. Workpads relies on the chain protocol (human-readable ACK, state_commit) to verify bilateral agreement.

BitLedger's conservation invariant applies to every batch. A batch that does not balance is rejected before it is stored. For ZAKO's power accounting, this is the difference between "we trust the battery readings" and "we know the readings are consistent because the protocol enforces it."

**Wire-level atomicity for compound events.**

pads-v1 has no equivalent of compound mode. Related records are linked by chain reference — a 4-character tag that both parties must agree on. If one record arrives and the other does not, the chain is broken but both records still exist independently. An application must notice the inconsistency.

BitLedger compound mode guarantees atomicity at the wire level. Either all records in a compound group post, or none do. This is the correct primitive for ZAKO's capability grants, Island membership operations, and multi-leg exchange events.

**Structured semantic precision without application-layer schemas.**

pads-v1 uses 4-bit BASE_TEMPLATE values (16 types) and named text fields to distinguish record types. BitPads uses 6-bit task_code values (64 types) and structured bit-field dimensions. The BitPads approach is more precise (64 > 16), fully structured (task dimensions are bit fields, not text strings), and requires no schema lookup (the decoder reads fixed bit positions).

For ZAKO's system records — power events, mode transitions, process lifecycle — this precision matters. "Invoice" and "state_commit" are Workpads categories. "POWER_GATE with value=WiFi_peripheral and direction=Out" is a ZAKO record that requires no Workpads concept to express.

**Efficient binary representation for daemon communication.**

pads-v1 records are designed for URL transmissibility — they are compressed text-compatible byte sequences that survive encoding to base64url. This makes them larger than necessary for contexts where raw binary is the transmission medium.

ZAKO daemon inter-process communication (Binder IPC, Unix domain sockets, kernel ring buffer) operates on raw binary. A 13-byte BitPads Full Record is the correct size for a power event communicated from `outstack-powerd` to `telux-ledgerd`. A 280-character pads-v1 URL is not.

**Layer 2 separators as structural context.**

pads-v1 chain references are optional and external — appended to the URL as `&c=AbCd`. They identify exchange relationships but carry no organizational hierarchy. BitPads Layer 2 separators are structural — they are in the batch header, inherited by every record in the batch, and provide a three-level hierarchy (File/Record/Group) that organizes records without any additional per-record overhead.

### 3.2 Trades Against the pads-v1 Approach

**SMS transmissibility.**

pads-v1 URL encoding (`workpads.me/p#1pa/<payload>`) allows any record to be transmitted as an SMS message and decoded by any Workpads client. This is a real and valuable property for Zambia.

BitPads Full Records are raw binary. Binary is not SMS-transmissible without an additional encoding layer. If ZAKO uses BitPads-native records as its canonical format, it must define a transmission encoding for SMS — most likely base64url, producing a URL similar to the pads-v1 format. The cost is that this encoding layer must be defined explicitly rather than inherited from the Workpads Standard.

**Workpads ecosystem interoperability.**

A pads-v1 record from Babb Cat is readable by Workpads KaiOS today. A BitPads-native ZAKO record is not — it would require Workpads clients to implement BitPads decoding, which they do not currently do.

This is a real constraint if interoperability with existing Workpads clients is a requirement at launch. It is not a permanent constraint — BitPads is more fundamental than pads-v1 (pads-v1 was derived from it), and Workpads clients could add BitPads support. But the current state is: pads-v1 interoperability exists, BitPads-native interoperability does not.

**Text field structure.**

pads-v1 provides named field slots for job, customer, location, date, worker, and other fields that are meaningful to human readers. The note block in BitPads Full Record provides free text but no named structure.

For records intended to be read by humans — work records, invoices, job reports — structured named fields are genuinely useful. "Customer: Alice" is more parseable than finding "Alice" in a free-text note block.

A ZAKO-native schema addresses this by convention rather than protocol. The task_code defines the record type; the note block follows a defined format per task_code. For `task_code=JOB`: the note block is `<customer> | <location> | <description>` pipe-delimited. This is a lightweight convention, not a protocol-level schema. It is sufficient for parsing and rendering; it is less safe than pads-v1's explicit field flags if the convention is not followed.

**The URL-as-document model.**

pads-v1's URL-as-document model is a complete record sovereignty solution: the content IS the URL, the template is cached locally, the document exists client-side. There is no equivalent in BitPads. BitPads defines wire formats for transmission; it does not define how records are stored, displayed, or shared as documents.

ZAKO would need to define its own document model for user-facing records if it abandons pads-v1. The simplest approach: for records with user-visible content, generate a pads-v1 URL on demand from the canonical BitPads binary (a one-way transformation, lossy in precision but sufficient for human display). The binary is the record; the pads-v1 URL is the human-facing view. This hybrid approach is discussed in Part Four.

---

## Part Four: The Hybrid Architecture

### 4.1 Two Canonical Forms for Two Contexts

The analysis points toward a specific architectural conclusion: ZAKO should maintain two canonical record forms for different contexts, not choose between them.

**Context A: System and daemon records**
- Format: BitPads Full Record or Full BitLedger binary
- Used for: power events, mode transitions, process lifecycle, capability operations, Island membership, Sovereign operations
- Why: raw binary efficiency, conservation enforcement, compound atomicity, Layer 2 separator organization
- Never needs URL encoding; transmitted over Binder IPC, Unix sockets, or Bluetooth LE

**Context B: Human exchange records**
- Format: pads-v1 URL (generated on demand from structured data)
- Used for: work records, invoices, job reports, payments, ACKs
- Why: SMS transmissibility, Workpads ecosystem interoperability, URL-as-document model, template rendering
- The BitPads task_code and value fields map to pads-v1 BASE_TEMPLATE and financial block fields

The unifying principle: both contexts are governed by the same BitLedger conservation invariant. A batch of system records and a batch of exchange records are both batch-level balanced. The conservation check does not know or care whether the records will be displayed as pads-v1 URLs or not.

### 4.2 The telux-ledgerd Storage Model

In the hybrid architecture, `telux-ledgerd` stores all records in a unified table. The format field distinguishes context:

```sql
CREATE TABLE records (
    seq          INTEGER PRIMARY KEY AUTOINCREMENT,
    format       TEXT NOT NULL,        -- 'bitpads_binary' | 'pads_v1'
    frame_bytes  BLOB NOT NULL,        -- canonical form (always binary for system; binary for exchange)
    pads_v1_url  TEXT,                 -- NULL for system records; generated on demand for exchange records
    
    -- Fields extracted from frame for indexing:
    file_sep     INTEGER NOT NULL,     -- Layer 2 File Separator (Island ID)
    record_sep   INTEGER NOT NULL,     -- Layer 2 Record Separator (relationship ID)
    group_sep    INTEGER NOT NULL,     -- Layer 2 Group Separator (position in relationship)
    task_code    INTEGER NOT NULL,     -- 6-bit task code (ZAKO codebook)
    sender_id    INTEGER NOT NULL,     -- Layer 1 Sender ID
    sub_entity   INTEGER NOT NULL,     -- Layer 1 Sub-Entity ID
    direction    INTEGER,              -- Layer 3 direction bit (0=in, 1=out)
    status       INTEGER,              -- Layer 3 status bit (0=settled, 1=pending)
    value_raw    INTEGER,              -- Layer 3 raw integer value
    wall_ts      INTEGER NOT NULL,     -- Unix epoch ms
    
    -- ZAKO identity extension:
    source_did   TEXT,                 -- DID resolved from sender_id via identity registry
    
    -- ZAKO integrity:
    frame_hash   BLOB NOT NULL UNIQUE, -- SHA-256(frame_bytes)
    chain_hash   BLOB,                 -- SHA-256(previous record's frame_hash) in this Island
    sovereign_sig BLOB NOT NULL        -- ed25519 over frame_hash (Keymaster 4.0)
);
```

The indices on (file_sep, record_sep, group_sep) replace pads-v1's (chain_anchor, chain_slot, chain_seq). The separator triple is a structural address, not an application-level identifier. Looking up "all records in Island 1, relationship 5, positions 0–12" is a direct range query on three integer columns.

### 4.3 The pads-v1 Generation Path

For exchange records that need to be shared externally, `telux-ledgerd` exposes a `generate_pads_v1_url(seq)` method that constructs a pads-v1 URL from the stored BitPads binary:

```
Input:  BitPads Full Record (task_code, value, time, note)
Output: pads-v1 URL (workpads.me/p#1pa/<payload>&c=<sep_encoded>)

Mapping:
  task_code → BASE_TEMPLATE (JOB→0x03, INVOICE→0x04, PAYMENT→0x05, etc.)
  value_raw → customer_amount (uint24)
  T1S time → date/time fields
  note block → job field (first pipe-segment) + story field (remainder)
  (file_sep, record_sep, group_sep) → chain_ref (3-char base64url of combined 14-bit separator)
  sender_id → participants block (IS_SENDER=1)
```

This is a lossy transformation — pads-v1 cannot represent all ZAKO BitPads fields (task_target, compound continuation records, C0 signal slots have no pads-v1 equivalents). But for the subset of record types that need external sharing (work records, invoices, payments), the mapping is sufficient and produces a valid pads-v1 URL that any Workpads client can decode.

The reverse transformation (pads-v1 URL → BitPads binary) is also defined, for importing records received from Workpads KaiOS counterparties:

```
Input:  pads-v1 URL received by SMS or QR
Output: BitPads Full Record stored in telux-ledgerd

Mapping:
  BASE_TEMPLATE → task_code (0x03→JOB, 0x04→INVOICE, etc.)
  customer_amount → value_raw
  date/time fields → T1S time
  job + story fields → note block (concatenated, pipe-separated)
  chain_ref → (file_sep=0, record_sep=derived, group_sep=seq_from_chain)
  participants IS_SENDER → sender_id (looked up in identity registry by name/DID)
```

Imported pads-v1 records are stored in the personal Island (file_sep=0) under the appropriate exchange relationship. They participate in conservation checks against ZAKO's own records for the same relationship. The system can detect if a received invoice does not match the expected value for a job that ZAKO has its own records for.

---

## Part Five: Implications for ZAKO Architecture Documents

### 5.1 What This Changes in the Implementation Plan

Phase 9 of the BabbCat Implementation Plan specifies `telux-ledgerd` and the ZAKO exchange ledger. The hybrid architecture described in this document modifies that phase in one significant way:

The storage schema uses BitPads Layer 2 separator triple (file_sep, record_sep, group_sep) as the primary organizational key rather than (chain_anchor, chain_slot, chain_seq) from pads-v1's chain protocol. This is a smaller change than it sounds — the separator triple is a superset of what the chain reference encodes, and the SQLite indices are equivalent. But it means `telux-ledgerd` is natively a BitPads ledger, not a pads-v1 ledger that happens to store binary. The distinction matters for how the daemon speaks to system records (no pads-v1 involved) versus exchange records (pads-v1 generated on demand).

### 5.2 The Codec Question

The pads-v1 codec (from `workpads-codec` npm / `js/lib/codec.js`) remains relevant for:
- Generating pads-v1 URLs for external sharing (the generation path above)
- Decoding incoming pads-v1 URLs from Workpads counterparties (the import path)
- The Workpads Android app's native rendering

It is no longer the canonical storage format. The canonical storage format is BitPads binary, assembled by a Kotlin or native-C BitPads encoder that ZAKO owns and controls.

The two codecs coexist. They speak different things to different audiences:
- BitPads binary: daemons, kernel interfaces, Bluetooth LE, Binder IPC — low-overhead, structured, conservation-enforced
- pads-v1 URL: SMS, QR codes, email, browser — URL-transmissible, template-renderable, Workpads-compatible

### 5.3 The Binary Pictography Opportunity

The C0 Enhancement Grammar's binary pictography is an underexplored capability that the pads-v1 approach cannot use at all (pads-v1 has no concept of C0 signal slots or codebook-decoded nibbles). The ZAKO task pictography codebook defined in section 1.5 — 16 concepts at 4 bits each — enables a class of ultra-compact ZAKO records:

A cover display notification (1.44" screen, extremely limited space) showing the current Outstack mode and last exchange event:

```
2 bytes of pictography (4 nibble-symbols):
  0x1: NORMAL
  0xC: ACK (last exchange acknowledged)
  0x5: GRANT (last capability operation)
  0x0: FULL (or MODE indicator)
```

The cover display renders these four symbols as four pictographic marks from the ZAKO visual codebook — no text parsing, no URL decoding, no field extraction. Four bits decode to one concept through the shared codebook. This is BitPads at its most compact: four semantic events in two bytes, readable by the cover display controller with zero parsing overhead.

---

## Conclusion

BitPads without the Workpads layer is not a degraded version of the system. It is the more fundamental version — the one that works at the level of conservation laws rather than field service record schemas.

What the Full Record's four components (identity, value, time, task, note) provide natively is sufficient to express every ZAKO record type: power events, mode transitions, capability grants, Island operations, work records, payments. The 64-entry task codebook gives these records precise semantic type information at 6 bits, more efficient than pads-v1's 4-bit BASE_TEMPLATE and more relevant to ZAKO's actual record space.

What compound mode's 1111 continuation marker provides — wire-level atomicity for multi-leg transactions — is a stronger guarantee than pads-v1's chain protocol for the cases that need it most: capability grants that must include both the grant and the revocation path, Island membership events that must include both the join and the initial capability scope.

What Layer 2's hierarchical separators provide — structural organizational context at the batch level — is a cleaner replacement for pads-v1's application-layer chain reference for the daemon communication cases where URLs are irrelevant.

The pads-v1 layer remains the correct interface for external communication — SMS, QR codes, Workpads ecosystem interoperability. ZAKO generates pads-v1 URLs from its BitPads canonical records on demand.

The correct architecture is therefore: **BitPads binary as the canonical ZAKO record format, with pads-v1 URL generation as a transmission adapter for external channels**. Not "pads-v1 as the record format with ZAKO-specific template extensions." The distinction is about where the centre of gravity is. In the pads-v1-centred approach, ZAKO extends someone else's schema. In the BitPads-centred approach, ZAKO owns its own schema and generates compatible representations for external consumption.

This is the difference between a distribution that adopts Workpads and one that is built on the same mathematical foundation Workpads was built on.

---

*Cross-reference:*
- `BitPads-as-ZAKO-Accounting-Core.md` — the pads-v1 integration approach (contrasting document)
- `ZAKO-Architecture-and-Vision.md` — foundational ZAKO OS narrative
- `BabbCat-Implementation-Plan.md` — Phase 9 implementation (update file_sep/record_sep/group_sep schema)
- `bitpads/readme.md` — BitPads protocol family overview
- `bitledger/markdown/BitLedger_CompoundMode_DesignNote.md` — compound mode specification
- `bitledger/markdown/BitLedger_Universal_Domain.md` — Universal Domain specification
