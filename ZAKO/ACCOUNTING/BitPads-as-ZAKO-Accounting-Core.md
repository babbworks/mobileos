# BitPads Protocol as the Accounting Core of ZAKO

## Conservation, Sovereignty, and the Record of Everything

*May 31, 2026*

---

> *"The same intellectual act that balances a ledger column has always balanced the universe."*
> — The BitPads Protocol Rationale, v2.0

---

## Preface

ZAKO is a sovereign operating system. It manages hardware, identity, power, money, work, and trust on behalf of a single human being — its owner. Every one of these domains involves flows: power flowing out of a battery, money flowing between accounts, data flowing across a radio link, work flowing from a contractor to a client. Every meaningful flow between two things is governed by a conservation law.

BitPads is an accounting protocol built on the observation that conservation laws are universal — that the same 40-bit structure that records a payment between two people also records a watt-hour of energy leaving a battery, a kilobyte of data traversing a radio link, or a unit of work flowing from contractor to client. The protocol does not merely accommodate this universality; it enforces it at the wire level. Records that violate conservation cannot be formed correctly. Records that are corrupted in transit fail three independent checks before they are accepted.

This document establishes that BitPads — including its full genealogy from BitLedger v3.0 through the Workpads Standard's pads-v1 codec — is the correct and complete accounting core for ZAKO. Not because it is convenient, and not because it happens to be available. Because the mathematical properties of the protocol are isomorphic with the architectural properties of ZAKO itself: conservation of value, self-sovereign record keeping, offline-first operation, and chain-linked immutable history.

---

## Part One: The Protocol Family

### 1.1 Genealogy: Four Layers, One Principle

The BitPads ecosystem is a four-layer stack, each layer extending the one below without breaking it.

**BitLedger v3.0** is the foundation. It defines a 40-bit double-entry accounting record — the minimum number of bits required to encode any conserved scalar exchange between two parties with sufficient precision for practical use. At its default scaling, a single 5-byte record can represent any transaction from one cent to $335 trillion. This is not a financial record system that happens to use binary encoding. It is a binary conservation-law enforcement mechanism that happens to be useful for finance.

**BitLedger Universal Domain v1.0** extends the Layer 1 header with a 2-bit domain selector. Domain 00 is financial (the default, backward compatible). Domain 01 is engineering — physical flows of mass, energy, data, pressure, charge, bandwidth. Domain 10 is hybrid (simultaneous financial and engineering interpretation of the same record). Domain 11 is custom (extension byte declares the domain). The wire format is identical across all domains. Only the semantic interpretation of the account-pair code and the quantity-type field changes. This means one protocol handles every flow that ZAKO OS must account for.

**BitPads v2.0** is the meta-layer. A single meta-byte precedes every transmission and declares the frame type: Pure Signal (1 byte heartbeat), Anonymous Wave (4 bytes, no identity overhead), Full Record (13–29 bytes, identity and value with optional context), or Full BitLedger Frame (22+ bytes with CRC-15 session integrity). The meta-byte makes every transmission self-framing — a receiver never needs out-of-band information to begin parsing. The C0 Enhancement subprotocol reclaims structural bits in Unicode control characters (bytes 0–31) to carry priority flags, acknowledgement requests, and continuation markers at sub-byte cost — a technique rooted in Sumerian accounting marks.

**Workpads Standard / pads-v1** is the application layer. It defines a complete binary-text hybrid codec for field service records, financial transactions, and chain-linked exchange histories. pads-v1 inherits BitPads' Role-C field presence header and extends it to 16 field-flag bits covering job description, customer identity, date, location, time windows, worker identity, narrative sections (details and story), financial amounts, and chain references. Records compress to 160–420 characters as URL strings — transmissible by SMS, QR code, Bluetooth LE, email, or any other medium.

These four layers are not independent products assembled by coincidence. They are one protocol expressed at four levels of abstraction.

### 1.2 The Wire Format: 40 Bits of Conservation

The core of BitLedger Layer 3 — the per-transaction record — deserves close examination because its design decisions illuminate every downstream choice.

```
Layer 3 / Set A (32 bits):
  Bits 1–17:   Multiplicand (upper 17 bits of value, 0–131,071)
  Bits 18–25:  Multiplier (lower 8 bits of value, 0–255)
  Bits 26–27:  Rounding signal (exact=00, ceiling=11, floor=10, invalid=01)
  Bit 28:      Split Order override
  Bits 29–30:  Direction + Status (mirrored in Set B for cross-layer validation)
  Bit 31:      Debit/Credit designation
  Bit 32:      Quantity Present flag

Layer 3 / Set B (8 bits):
  Bits 33–36:  Account Pair (4-bit code; 14 valid pairs + 1 correction + 1 compound)
  Bit 37:      Direction (must equal bit 29; mismatch = error)
  Bit 38:      Status (must equal bit 30; mismatch = error)
  Bit 39:      Completeness (full vs. partial)
  Bit 40:      Extension flag
```

**No-Gap Value Coverage:** Every integer from 0 to 33,554,431 is exactly reachable without gaps. This is enforced by the encoding formula `N = A × 2^S + r` where `A` is the Multiplicand field, `S` is the Layer 2 Optimal Split (default 8), and `r` is the Multiplier field. At S=8: `N = A × 256 + r`. Since `r` ranges 0–255 and `A` ranges 0–131,071, every value in the range is covered exactly once.

**Three Independent Error Mechanisms:**
1. CRC-15 over the Layer 1 session header (polynomial x^15 + x + 1 = 0x8003), catching 100% of burst errors ≤15 bits
2. Cross-layer Direction and Status bit mirroring: Set A bits 29–30 must equal Set B bits 37–38; any single-bit flip in either copy is detected
3. The invalid rounding state 01 (bit 26=0, bit 27=1) is structurally impossible to set intentionally, so its presence signals corruption
4. The Conservation Invariant: the sum of all signed flows in a batch must equal zero; any phantom, duplicated, or missing record that survives byte-level CRC is caught by this algebraic check

No application-level checksum logic is needed. The protocol structure is the error detection.

### 1.3 The Universal Relationship Matrix

In financial mode, the 4-bit account-pair code (bits 33–36) encodes one of 14 canonical debit/credit relationships (cash/revenue, accounts receivable/revenue, etc.). In engineering mode, the same 4 bits encode one of 16 canonical flow relationships between any two nodes in any conserved system:

| Code | Archetype | Engineering Meaning |
|------|-----------|---------------------|
| 0000 | Source → Sink | Battery → CPU |
| 0001 | Parent → Child | Power bus → subsystem |
| 0010 | Debtor → Creditor | Obligation incurred (power borrowed) |
| 0011 | Mutual Exchange | Bandwidth-for-compute barter |
| 0100 | Loss/Dissipation | Heat dissipated to environment |
| 0101 | Generation/Input | Solar panel generating power |
| 0110 | Reservation/Escrow | Power reserved for emergency |
| 0111 | Repayment/Return | Returning borrowed power |
| 1000 | Transformation | Chemical → kinetic energy |
| 1001 | Distribution | Power bus → multiple peripherals |
| 1010 | Aggregation | Multiple sensors → processor |
| 1011 | Internal Transfer | RAM buffer → cache |
| 1100 | Obligation Transfer | Power debt reassigned |
| 1101 | State Commit | End-of-period resource snapshot |
| 1110 | Correction/Void | Sensor recalibration event |
| 1111 | Compound Continuation | Multi-stage event sequence |

These 16 archetypes are exhaustive. Every flow of every conserved quantity between any two nodes in any system maps to one of them. This is not an assertion — it follows from the mathematical structure of conservation laws, which BitLedger's original design explicitly derives.

### 1.4 pads-v1: The Application-Level Record

Where BitLedger defines the transaction primitive, pads-v1 defines the complete annotated record for human-readable exchange contexts. A pads-v1 frame begins with a one-byte header declaring frame type, domain, and presence of additional header bytes. Field presence is encoded in two mandatory flag bytes (16 bits, one per standard field). Present fields follow in ascending slot order as length-prefixed UTF-8 blocks (text) or fixed-width integers (amounts, dates).

The full pads-v1 frame structure:

```
[meta1]          1 byte — BASE_TEMPLATE, ACK_REQUEST, CHAIN, RECIPIENT_TYPE
[meta2]          1 byte — SELF_DESCRIBING, COMPACT_TIME, HAS_TRIG_BLOCK,
                          PARTICIPANTS, DOMAIN, DRAFT, RESTRICT_FORWARD
[setup_byte]     1 byte — DECIMAL_POS, CURRENCY, TAX_CODE, SF_PRESENT
[sf_byte]        1 byte — SCALING_FACTOR, COMPOUND_VALUE, QTY_COMPACT, SPLIT_POINT
[transaction]    1 byte — DIRECTION, TIME, EFFECT, SUBTYPE, QTY_SPLIT, ROUNDING
[field_flags]    2 bytes — 16-bit field presence vector
[field_flags3]   1 byte — extended fields (if bit 15 of field_flags set)
[data blocks]    Variable — UTF-8 text and binary integer fields in slot order
[participants]   Variable — structured sender + worker identity block
[TRIG_block]     Variable — stack-machine bytecode for conditional commitment
```

The URL encoding: every pads-v1 frame is deflate-compressed (fflate level 9) and base64url-encoded, producing a URL fragment: `workpads.me/p#1pa/<payload>`. The scheme tag `1pa` signals version 1, package a, deflate compression. Optional chain reference appended as `&c=<4-char>`.

Benchmark: a typical 5-field job record with 2 actions encodes to ~156 bytes raw, ~216 URL characters. A full invoice with financial block, participants, and chain reference: ~300 bytes raw, ~420 URL characters. An ACK confirmation: ~80 bytes raw, ~120 URL characters.

---

## Part Two: The Alignment with ZAKO's Architecture

### 2.1 Conservation Is ZAKO's Native Language

ZAKO OS is not primarily an accounting system. It is a sovereign operating environment. But sovereignty over hardware and network resources is impossible without accounting — without knowing, at any moment, what has come in, what has gone out, and what the current balance is.

Outstack's five power modes exist because power is conserved. You cannot have CONSERVE mode without knowing how much power has been spent and how much remains. You cannot enforce CRITICAL mode (suspend all non-essential processes) without a continuous accounting of power flows. The Outstack power daemon (`outstack-powerd`) already implements an informal power ledger — polling battery state, computing consumption rates, making mode transition decisions. This informal ledger has no wire format, no error detection, no cross-device synchronization, and no historical record.

BitLedger's Universal Domain gives `outstack-powerd` a proper accounting foundation. Every power event becomes a 40-bit record. The conservation invariant validates every batch. CRC-15 catches transmission errors if power data is exchanged over Bluetooth or mesh. The engineering-domain archetypes map directly onto Outstack's power concepts:

- Battery charging: 0101 (Generation/Input) — energy enters system from charger
- Device consumption: 0000 (Source → Sink) — battery to application processor
- Process suspension: 0110 (Reservation/Escrow) — power reserved for CRITICAL processes
- Power mode transition: 1101 (State Commit) — snapshot of system power state at mode change
- Anomaly correction: 1110 (Correction/Void) — sensor recalibration after measurement error

Outstack's five process classes (CRITICAL, INTERACTIVE, BACKGROUND, DEFERRED, OPPORTUNISTIC) have direct financial analogues in the account-pair codes — different classes of power debt with different priority status bits. This is not metaphor. The mathematical structure is the same.

### 2.2 The Island as Accounting Domain

Telux's Island concept — a sovereign container with its own power domain, security namespace, and exchange ledger — maps naturally onto BitLedger's Layer 1 session context. Each Island is a distinct accounting domain. Records within an Island share a session header that establishes the Island's identity, permissions, and currency/unit defaults.

In BitPads terms:
- **Layer 1 Sender ID (32 bits):** The Island Sovereign's device-assigned identity index
- **Sub-Entity ID (5 bits):** The specific Island within a Sovereign's device (up to 31 Islands)
- **Layer 2 Group Separator (4 bits):** Logical subdivision within an Island (work type, time period)
- **Layer 2 File Separator (3 bits):** Context within a group (project, client relationship)

An Island's exchange ledger is simply a collection of BitLedger batches, each batch containing the records for one exchange session or time period. The conservation invariant applies per batch. The Island Sovereign's ed25519 signature (via Keymaster 4.0) signs the batch header after validation passes.

This is not merely compatible with the ZAKO ledger architecture — it resolves a design question that the ZAKO Architecture document left open. The architecture specified that each Island has a ledger, and that the ledger records must be chain-hashed and Sovereign-signed. It did not specify the internal structure of the records. BitLedger provides that internal structure, and the structure carries its own mathematical guarantees that are independent of the cryptographic signing layer.

### 2.3 The URL-as-Document Model as Sovereignty Infrastructure

The URL-as-document model is one of the most consequential design decisions in the Workpads Standard, and it is more radical than it appears.

Traditional record systems store data in databases. To access a record, you authenticate to a server, the server queries its database, and the server returns the record. This creates three dependencies: the server must exist, you must authenticate to it, and the server must have maintained its database correctly. None of these can be guaranteed. In Zambia, or in a rural area with intermittent connectivity, or after a company's servers go down, the records are inaccessible.

The URL-as-document model inverts this. The record IS the URL. The URL is base64url(deflate(pads-v1 frame)). The frame contains the complete record. No server is involved in record creation, storage, or retrieval. The record can be transmitted by any medium — SMS, QR code, Bluetooth, email, printed paper — and decoded identically by any Workpads-compatible client. If the only surviving copy is a printed QR code on a receipt, the record is still fully recoverable.

For ZAKO, this property is foundational. A sovereign device's records must be sovereign. A ZAKO user's transaction history must not be held hostage by any server, any company, or any connectivity requirement. pads-v1 URLs satisfy this requirement architecturally, not as a policy choice that could be reversed.

The economic implication: 1,000 transactions per year produce ~1.5 MB of URL strings. At 10,000 transactions: ~15 MB. A mobile device with 16 GB of storage can hold the complete transaction history of a professional lifetime in less than 1% of its storage capacity.

### 2.4 The Chain Protocol as Exchange Ledger Backbone

The Workpads chain protocol provides the linking mechanism for multi-record exchanges — the backbone of the ZAKO Telux exchange ledger.

A chain is anchored by a 24-bit value divided into three parts:
- **ANCHOR (17 bits):** Device-specific, cryptographically random, shared across all chains from one device
- **PARTICIPANT_SLOT (4 bits):** Identifies which party holds this copy (slot 0 = sender's authoritative copy, slots 1–15 = distributed to counterparties)
- **SEQUENCE (3 bits):** Position in the chain (0 = anchor record, 1–6 = subsequent, 7 = State Commit closure)

Encoded as 4 base64url characters (`&c=AbCd`), the chain reference is appended to every URL in the chain. A 24-bit value provides 131,072 distinct anchors per device — sufficient to uniquely identify every exchange relationship a person is likely to have in their lifetime.

The multi-worker pre-seeding mechanism deserves specific attention: a supervisor creates a job record for three workers. The frame is compressed exactly once. The same compressed payload is URL-encoded three times, with different `&c=` values for each worker's participant slot. The compression and encoding cost is O(1), not O(N). This is the correct solution to the "share one record with N recipients" problem that naive record systems solve by creating N copies.

State Commit records (BASE_TEMPLATE=0xD) formally close a chain. They carry a subtype encoding: job close (00), payment confirmed (01), terms agreed (10), or period summary (11). A State Commit with ACK_REQUEST=1 requires a counterparty ACK before the chain is considered complete by both parties. The ACK is a minimal frame (~80 bytes) that references the chain anchor and confirms receipt.

This chain structure IS the Telux exchange ledger's bilateral record. The ZAKO ledger adds Sovereign signatures and DID fields on top; the chain protocol provides the structure that makes the records linkable, queryable, and auditable.

---

## Part Three: ZAKO-Specific Integration

### 3.1 DID-Anchored Identity Extension

BitPads Layer 1 encodes a 32-bit Sender ID — sufficient for session-scoped identity but not for cryptographic sovereignty. ZAKO's identity layer (Telux `did:key` DIDs, backed by Keymaster 4.0 TrustZone) provides cryptographic identity that extends BitPads' sender model.

The integration point is the pads-v1 participants block. ZAKO extends participants with a new flag: `HAS_DID`. When set, a 51-character `did:key:z6Mk...` string follows the participant name block. This allows ZAKO records to carry full verifiable identity without modifying the core frame structure — it is a standard extension to the participants block, which the protocol is already designed to accommodate.

For the Sovereign's own records (IS_SENDER=1, PARTICIPANT_SLOT=0), the DID is always present. For counterparty records (PARTICIPANT_SLOT=1–15), the DID is present when known and omitted when transacting with parties who have not yet established a DID.

Layer 1 Sender ID retains its role as a session-scoped identity index for efficiency. On ZAKO devices, the Sender ID is a small integer indexing into the device's `telux-identd` known-peer table, which maps to the full DID for verification purposes.

**Signature placement:** The pads-v1 frame is signed by the Sovereign's ed25519 key (via Keymaster 4.0) after encoding. The signature is NOT embedded in the frame — this would add 64 bytes of overhead to every record and violate the URL-transmissible size constraint. Instead, ZAKO maintains a parallel signature chain: each record's hash (SHA-256 of the frame bytes) is signed and stored in the Island's SQLite ledger alongside the frame. The URL-transmissible record is self-contained without the signature; the signature exists for local auditing and dispute resolution where the parties need to prove authenticity.

This separation preserves the core URL-as-document property while adding the cryptographic layer ZAKO requires.

### 3.2 ZAKO Template Extensions (0x07–0x0B)

The pads-v1 BASE_TEMPLATE field (bits 6–3 of meta1) has 16 possible values. The Workpads Standard uses:
- `0x00` – `0x0C`: Standard service and commercial record types
- `0x0D`: State Commit
- `0x0E`: Amendment

`0x0F` is reserved. ZAKO extends with five new templates in the extension range:

**Template 0x07 — `island_exchange`**

Generic resource transfer between Island members. Used when the exchange type is not a standard commercial transaction — skill sharing, compute time, sensor data, physical goods between sovereign entities.

Additional required fields (via FLAGS3 extension bits):
- `island_id` (text): The Island in which this exchange occurred
- `source_did` (text): DID of the transferring entity
- `destination_did` (text): DID of the receiving entity

Optional:
- `authorization_token` (binary, 32 bytes): Capability token signed by Island Sovereign authorizing this specific exchange type
- `domain` byte: Specific exchange category (compute, storage, bandwidth, data, physical, service)

**Template 0x08 — `capability_grant`**

Sovereignty delegation record. When an Island Sovereign grants a capability to another entity — permission to act on behalf of the Island, to access specific resources, or to execute specific operations — this record establishes the delegation and its constraints.

Additional required fields:
- `grantor_did`: DID of the granting Sovereign
- `grantee_did`: DID of the entity receiving the capability
- `capability_uri`: Namespaced identifier for the granted capability
- `expiry` (uint32): Unix timestamp when the grant expires (0 = no expiry)
- `delegation_depth` (uint8): How many times this capability can be re-delegated (0 = not delegatable)

**Template 0x09 — `power_event`**

Outstack power anomaly or mode transition record. Records the system's power state at significant transitions for historical auditing, debugging, and pattern analysis.

Additional required fields:
- `previous_mode` (uint8): Outstack mode before transition (0=FULL, 1=NORMAL, 2=CONSERVE, 3=CRITICAL, 4=EMERGENCY)
- `new_mode` (uint8): Outstack mode after transition
- `trigger` (uint8): What caused the transition (battery_threshold=0, thermal=1, user_override=2, scheduled=3, emergency=4)
- `battery_pct` (uint8): Battery percentage at time of transition
- `thermal_pct` (uint8): Thermal margin at time of transition (0 = overheating, 100 = cool)

Optional:
- `suspended_processes` (uint16): Count of processes suspended by this transition
- `power_draw_mw` (uint32): Instantaneous power draw in milliwatts at transition time

**Template 0x0A — `entity_join`**

Entity joins an Island. Records the DID of the joining entity, the Island they joined, the Sovereign who admitted them, and the capability scope they were granted on joining.

Additional required fields:
- `island_id`: The Island joined
- `entity_did`: DID of the joining entity
- `entity_type` (uint8): human=0, ai=1, service=2, device=3
- `admitted_by`: DID of Island Sovereign or delegated admitter
- `initial_capability_scope` (uint16 bitmask): Which capabilities the entity is granted on admission

**Template 0x0B — `entity_leave`**

Entity departs or is removed from an Island. Paired with `entity_join` records; together these form the complete membership history of any Island.

Additional required fields:
- `island_id`: The Island departed
- `entity_did`: DID of the departing entity
- `leave_type` (uint8): voluntary=0, expired=1, revoked=2, migrated=3
- `final_state_commit`: Hash of the final State Commit record in all active chains with this entity (preserves audit trail even after departure)

These five templates extend the pads-v1 template space without modifying any existing template. Implementations that do not understand templates 0x07–0x0B can still decode the standard fields (job, customer, date, etc.) — the extension fields are carried in FLAGS3 extension bits that older implementations ignore.

### 3.3 BitLedger as Outstack's Power Accounting Layer

The most technically elegant integration is using BitLedger's Universal Domain directly inside `outstack-powerd` as its internal accounting format.

Currently (in the design), `outstack-powerd` maintains power state by polling `sysfs` battery nodes and computing instantaneous power draw. This is stateless — it answers "what is the power state right now" but cannot answer "what has the power state been over the last 48 hours" or "which process class is responsible for the anomalous draw this morning."

BitLedger Universal Domain gives `outstack-powerd` a complete ledger:

```
Layer 1 Session Header (established at daemon start):
  Protocol Version: 0
  Domain: 01 (Engineering)
  Permissions: Read=1, Write=1, Correct=1, Represent=0
  Sender ID: device_id (lower 32 bits of ANDROID_ID)
  CRC-15: computed over all above fields

Layer 2 Batch Header (opened per Outstack mode period):
  Transmission Type: 10 (Copy from sender — recording own state)
  Scaling Factor: 0000110 (×1,000 — amounts in milliwatt-hours)
  Optimal Split: 1000 (S=8, standard)
  Decimal Position: 010 (2 decimal places)
  Quantity Type: 7 (Electrical Charge — milliampere-hours)
  Group Separator: current mode (0=FULL, 1=NORMAL, 2=CONSERVE, 3=CRITICAL, 4=EMERGENCY)

Layer 3 Records (emitted on each significant power event):
  Battery charging:
    Account Pair: 0101 (Generation/Input)
    Direction: 0 (In)
    Quantity: charger_current_mah (measured via battery HAL)
    
  CPU cluster active:
    Account Pair: 0000 (Source → Sink: Battery → CPU)
    Direction: 1 (Out)
    Quantity: cpu_power_draw_mah (computed from PMIC current sensor)
    
  WiFi active:
    Account Pair: 0000 (Source → Sink: Battery → WiFi)
    Direction: 1 (Out)
    Quantity: wifi_power_draw_mah
    
  Mode transition:
    Account Pair: 1101 (State Commit)
    new_mode in extension byte
    
  Process suspension:
    Account Pair: 0110 (Reservation/Escrow)
    Quantity: projected_savings_mah_per_hour
```

Conservation validation per batch: the sum of all signed flows (charging in, all peripheral consumption out) must balance to the net battery change over the batch period. Any persistent imbalance greater than measurement tolerance (say, 2% of capacity) signals a drain path that is not being accounted for — a wakelock leak, an unregistered peripheral, or a measurement error.

This is not academic. Unaccounted drain is one of the most common battery complaints on Android devices. BitLedger's conservation invariant turns "my battery is draining mysteriously" from a debugging mystery into a ledger imbalance that is detectable and attributable.

### 3.4 The Exchange Ledger: Complete Architecture

With BitPads as the foundation, the ZAKO exchange ledger architecture resolves cleanly:

```
Physical Storage: SQLite database per Island (FBE-encrypted, dm-verity protected)

Schema:
CREATE TABLE records (
    seq            INTEGER PRIMARY KEY AUTOINCREMENT,
    chain_anchor   TEXT NOT NULL,          -- 17-bit anchor (base64url)
    chain_slot     INTEGER NOT NULL,        -- 0 = authoritative; 1-15 = counterparty copies
    chain_seq      INTEGER NOT NULL,        -- position in chain (0-7)
    base_template  INTEGER NOT NULL,        -- pads-v1 BASE_TEMPLATE value
    source_did     TEXT NOT NULL,           -- ZAKO extension: sending entity DID
    dest_did       TEXT,                    -- ZAKO extension: receiving entity DID
    island_id      TEXT NOT NULL,           -- ZAKO extension: Island context
    frame_bytes    BLOB NOT NULL,           -- raw pads-v1 frame (before base64url)
    frame_hash     BLOB NOT NULL UNIQUE,    -- SHA-256(frame_bytes) — integrity anchor
    chain_hash     BLOB,                    -- SHA-256(previous record's frame_hash) 
    sovereign_sig  BLOB NOT NULL,           -- ed25519 sig over frame_hash (Keymaster 4.0)
    lamport_ts     INTEGER NOT NULL,        -- monotonic counter (no clock skew attacks)
    wall_ts        INTEGER NOT NULL,        -- Unix epoch milliseconds
    INDEX idx_anchor (chain_anchor),
    INDEX idx_island (island_id),
    INDEX idx_source (source_did),
    INDEX idx_dest (dest_did),
    INDEX idx_template (base_template)
);

CREATE TABLE chain_index (
    chain_anchor   TEXT PRIMARY KEY,
    island_id      TEXT NOT NULL,
    participants   TEXT,                    -- JSON array of {did, name, slot}
    opened_ts      INTEGER NOT NULL,
    closed_ts      INTEGER,                 -- NULL if chain open
    state          TEXT NOT NULL           -- 'open' | 'committed' | 'disputed'
);
```

**Write Path:**
1. Caller (Workpads app, exchange daemon, or power daemon) provides pads-v1 frame bytes to `telux-ledgerd` via AIDL
2. Daemon validates: Island membership (source_did has membership record), capability token for template type
3. Daemon computes `frame_hash` = SHA-256(frame_bytes)
4. Daemon fetches `chain_hash` = `frame_hash` of most recent record in this Island
5. Daemon requests ed25519 signature over `frame_hash` from `telux-identd` (Keymaster 4.0 in TrustZone)
6. Daemon writes to SQLite with all fields populated
7. Daemon returns `frame_hash` to caller as write confirmation

**Read Path:**
1. Caller queries by `chain_anchor`, `island_id`, `source_did`, or `base_template`
2. Daemon returns `frame_bytes` — caller decodes with pads-v1 codec
3. Daemon optionally returns `sovereign_sig` for external verification scenarios

**Export Path:**
- Single record: `frame_bytes` → deflate → base64url → `workpads.me/p#1pa/<payload>&c=<chain_ref>` URL
- Chain export: ordered records → JSON array → exportable to any Workpads client
- Power ledger export: BitLedger batch → compact binary → transmissible over Bluetooth for debugging

### 3.5 Natural Language Query Layer

The exchange ledger must be queryable in natural language. For the target user population — Zambian field service workers who may have low literacy in the Western sense but high numeracy and practical intelligence — voice or text queries in natural language are the primary interface.

pads-v1's structured fields make natural language query deterministic for the most common cases:

```
Query: "what did Alice pay me last month"
Parse:
  entity = "Alice" → look up in identity registry → dest_did="did:key:z6Mk..."
  direction = "pay" → DIRECTION=1 (outgoing from Alice, incoming to me)
  time = "last month" → wall_ts range [first of last month, last of last month]
  template filter = [0x01 (payment), 0x02 (state_commit with payment subtype)]
SQL:
  SELECT frame_bytes FROM records
  WHERE source_did = 'did:key:z6Mk...' 
    AND base_template IN (1, 13)
    AND wall_ts BETWEEN ? AND ?
  ORDER BY wall_ts DESC

Query: "show all open jobs"
Parse:
  status = "open" → chains where state = 'open'
  template = "jobs" → base_template IN (0x03, 0x07)
SQL:
  SELECT c.chain_anchor, r.frame_bytes
  FROM chain_index c JOIN records r ON c.chain_anchor = r.chain_anchor
  WHERE c.state = 'open' AND r.chain_seq = 0

Query: "total income this week"
Parse:
  aggregate = SUM
  direction = "income" → DIRECTION=0 (incoming)
  time = "this week" → wall_ts range [Monday 00:00, now]
SQL:
  SELECT SUM(decoded_amount) FROM records
  WHERE DIRECTION=0 AND wall_ts >= ? AND base_template IN (financial templates)
```

The rule-based parser covers approximately 80% of practical queries. For the remaining 20% — ambiguous phrasing, entity disambiguation, complex time expressions — a small on-device language model (quantized to 50–200MB, running in a DEFERRED process class) handles parse disambiguation without any network dependency.

---

## Part Four: The Protocol Ecosystem's Role in ZAKO

### 4.1 Workpads Interoperability as First-Class Requirement

ZAKO is being built for a specific population in a specific country, but it is not being built for an isolated system. Field service workers in Zambia who use Babb Cat will have counterparties who use Workpads KaiOS on feature phones, Workpads web on desktops, or the Workpads CLI. These counterparties must be able to receive and read records generated by Babb Cat without any software update, any ZAKO account, or any prior coordination.

This interoperability is guaranteed by the pads-v1 codec. A Babb Cat invoice is a pads-v1 URL. A Workpads KaiOS client can decode it. The ZAKO extension fields (island_id, source_did, dest_did) are carried in FLAGS3 extension bits that the KaiOS client ignores — gracefully. The financial amount, job description, date, customer name, and chain reference are all visible and usable by any pads-v1 client.

The design constraint this imposes on ZAKO: ZAKO's ledger records must remain valid pads-v1 records. The five ZAKO extension templates (0x07–0x0B) must be registered with `workpads-standard/` as ratified extensions before Babb Cat is released. This ensures that any future Workpads client can decode them, even if it renders the ZAKO-specific fields as opaque blobs.

### 4.2 The Template System as ZAKO's Application Layer

The Workpads template system — where a template URI like `urn:workpads:tpl:note:invoice:v2` maps to a cached HTML/CSS rendering template — is exactly what ZAKO needs for its exchange record display layer.

ZAKO extends the template system with:
- `urn:home:tpl:island_exchange:v1` — renders island_exchange records with Island membership context
- `urn:home:tpl:capability_grant:v1` — renders capability delegation in a form the user can understand
- `urn:home:tpl:power_event:v1` — renders Outstack power transition records as a timeline
- `urn:home:tpl:entity_join:v1` / `urn:home:tpl:entity_leave:v1` — renders Island membership history

These templates are cached locally on the device, fetched from the ZAKO distribution server on first use, and work offline thereafter. The fallback chain (template not found → fetch canonical URI → fallback to built-in default) means the user always sees their record rendered, even if the specific template was not cached.

The template system cleanly separates record format (pads-v1 frame bytes, stable) from record presentation (HTML/CSS template, updatable independently). When ZAKO releases a new visual design for invoice records, the underlying pads-v1 frames are unchanged — only the template is updated. Records from five years ago render with the latest visual design because the format is stable and the presentation is templated.

### 4.3 Offline-First as Sovereignty, Not Feature

The Workpads Standard was designed offline-first not as a user experience feature but as a sovereignty requirement. A record system that requires connectivity to function is a record system that is controlled, at some level, by whoever controls the connectivity. In rural Zambia, that means a mobile network operator. In a conflict zone, it means whoever controls the towers. In any context, it means an external party.

pads-v1's URL-as-document model is offline-first architecturally. There is no request to a server. There is no authentication check. The record is encoded locally, in the device's memory, using only the codec. It can be shared via SMS (no data required — SMS works on all network conditions including 2G), via QR code (no network at all — one party scans the other's screen), or via Bluetooth LE (no network, short range, very low power). It can be decoded identically.

For ZAKO, this property is non-negotiable. Babb Cat targets Zambia, where mobile data is expensive and unreliable outside urban centers. A ZAKO user traveling to a rural job site must be able to create records, share records, and receive records without any data connectivity. pads-v1 enables this at the protocol level.

The chain protocol adds one connectivity requirement: if the user wants to share a chain reference with a counterparty in real time, both devices must be able to communicate (but not necessarily via the internet — Bluetooth is sufficient). State Commit records can be transmitted offline and processed when connectivity is restored. The chain remains valid in either case.

### 4.4 From BitPads to ZAKO: The Unbroken Line

The intellectual lineage from BitPads to ZAKO is not a retrofit. It is a natural extension of the same design philosophy expressed at increasing levels of abstraction.

**BitLedger** says: Every exchange between two entities must conserve value. The wire format enforces this. Error detection is built into the structure. No external validation is needed.

**BitPads** says: Different exchanges have different character — a heartbeat needs 1 byte, a full annotated record needs 29 bytes, a formal double-entry transaction needs 40 bits. Frame type is declared in the first byte. Every transmission is self-framing.

**Workpads Standard / pads-v1** says: Field service workers need job descriptions, customer names, and financial amounts in a form that fits in a URL and travels by SMS. The codec compresses these into the minimum binary footprint that preserves all the information.

**ZAKO** says: Sovereign entities — human beings, AI agents, devices, services — need to exchange records of all kinds (work, power, capability, membership, payment) without depending on any central authority, using their own cryptographic identity, in a format that survives the worst network conditions. The record must be auditable forever, identifiable to a specific Island, and provable to any counterparty.

Each layer adds exactly what the layer below did not provide, without changing what was already correct. ZAKO is not replacing or extending BitPads. ZAKO is BitPads expressed at the level of an operating system.

---

## Part Five: Implementation Implications for Babb Cat

### 5.1 The Codec Library

Babb Cat requires a native Android codec for pads-v1 encoding and decoding. The existing JavaScript implementation (`workpads-codec` npm package, `js/lib/codec.js`) is the reference. For Android, the implementation choices in order of preference:

1. **JNI wrapper around the C implementation of fflate:** The deflate compression is the performance-sensitive component. A JNI wrapper lets the Kotlin/Java exchange layer call native deflate without the overhead of Kotlin's compression libraries.

2. **Kotlin reimplementation:** The pads-v1 codec logic is specified fully in `workpads-standard/codec.md`. Reimplementing in Kotlin (using `java.util.zip.Deflater` for compression) is approximately 800–1,200 lines including the full frame structure, all field types, financial block, participants block, and chain reference handling. The `workpads-standard/` codec test vectors in `test/fixtures/1pv-vectors.json` provide ground truth for validation.

3. **QuickJS embedded JavaScript:** If the JavaScript reference implementation must be used directly, QuickJS (a minimal JS engine, ~200KB) can be embedded as an Android native library. This is the heaviest option but provides exact behavioral parity with the reference.

Recommended: option 2 (Kotlin reimplementation). The codec is well-specified, the test vectors exist, and a native Kotlin implementation integrates cleanly with the Android service architecture.

### 5.2 Integration with telux-ledgerd

`telux-ledgerd` is the ZAKO daemon that manages Island ledgers. Its interface with the pads-v1 codec follows a clean boundary:

- **Above the boundary:** Application code (Workpads app, system UI, exchange service) deals in structured record objects (`WorkpadRecord`, `IslandExchange`, `PowerEvent`) and URLs
- **At the boundary:** `telux-ledgerd` accepts either raw pads-v1 frame bytes (for records received from external sources) or structured record objects (for records generated locally). For local records, it calls the codec to encode before writing.
- **Below the boundary:** SQLite storage, Keymaster-backed signing, chain hash computation

This separation allows the codec to evolve (new fields, new templates) without modifying the storage layer, and allows the storage layer to evolve (new index strategies, migration) without modifying application code.

### 5.3 The Power Accounting Path

`outstack-powerd` generates power events continuously. The integration with `telux-ledgerd`:

- Power events use BASE_TEMPLATE=0x09 (`power_event`)
- They are written to the device's Personal Island ledger (the Island that always exists and cannot be deleted)
- The write rate is low: mode transitions happen rarely (single digits per day under normal use)
- Between transitions, `outstack-powerd` maintains its own in-memory BitLedger batch and writes the batch summary to the ledger at each mode transition
- Detailed per-peripheral power records (WiFi draw, CPU draw) remain in `outstack-powerd`'s in-memory ring buffer and are written to the ledger only on anomaly detection (conservation imbalance > 2% of capacity)

This keeps the ledger compact (a few power records per day) while preserving the ability to retrieve detailed power history when needed.

### 5.4 Workpads App Integration

The Workpads Android app for Babb Cat is a standard Android application that uses `telux-ledgerd` as its storage backend via AIDL, replacing the app's internal localStorage. From the app's perspective, the change is minimal: instead of calling `window.localStorage.setItem('wp_record_...')`, it calls `teluxLedger.writeRecord(frame_bytes, island_id)`. The pads-v1 codec, the template system, and the UI are unchanged.

The DID integration adds one new UI element: the app can now display the source_did of any received record and resolve it to a name (from the device's identity registry) or show the raw DID if the sender is unknown. This is the Stone/DID identity functionality that was explicitly left open in the Workpads KaiOS implementation. ZAKO completes it.

### 5.5 Chain Protocol Sync

The chain protocol in Workpads uses localStorage for chain state (`wp_chain_<anchor_hex>`). In Babb Cat, this state lives in `telux-ledgerd`'s SQLite `chain_index` table. The app retrieves chain state via `teluxLedger.getChain(anchor)` and `teluxLedger.getChainRecords(anchor)`. The UI logic for chain display (connector glyphs `◉`, `─`, `◎`, State Commit option, ACK generation) is unchanged.

The key behavioral change: in the KaiOS app, the chain state is device-local and not synchronized across devices. In Babb Cat, chains are per-Island, and Islands can be synchronized across devices belonging to the same Sovereign. A job chain started on the phone is visible on the tablet. This is an ZAKO-layer capability that the pads-v1 protocol enables but does not mandate.

---

## Part Six: Beyond Babb Cat

### 6.1 BitLedger for AI Entity Accounting

ZAKO's architecture positions AI agents as first-class entities with DIDs, Island membership, and ledger records. An AI agent that provides a service — translation, data analysis, route planning — must be able to receive capability grants, generate work records, and request payment. All three of these are pads-v1 record types.

The AI entity's exchange records are identical in format to human exchange records. The entity_type field in `entity_join` records distinguishes them (entity_type=1 for AI). The DID is issued by the same Keymaster-backed `telux-identd` that issues human DIDs. The ledger does not know or care whether the Sovereign of a record is a human or an AI — it enforces conservation, chain integrity, and signature validity identically.

This is the correct design. Differential treatment of AI versus human records in the ledger would create either security vulnerabilities (if AI records have lower integrity requirements) or arbitrary restrictions (if AI records have different accounting rules). The protocol is neutral on the nature of the entities. The sovereignty layer is not.

### 6.2 BitLedger Universal Domain for Autonomous Systems

The Universal Domain specification was written with spacecraft in mind — recording fuel flow, power distribution, and data telemetry in the same 40-bit format as financial transactions. This is exactly the requirement for ZAKO running on an autonomous excavator or a delivery robot.

An excavator's ZAKO instance needs to account for:
- Fuel consumption per hour of operation (mass flow, archetype 0000)
- Hydraulic pressure supply from pump to cylinder (fluid flow, archetype 0001)
- Battery discharge during electric-assist modes (energy flow, archetype 0000)
- Service hours owed to maintenance contractors (service obligation, quantity type 14)
- Payment to fuel supplier (financial domain, same session header with domain 00)

One protocol. One wire format. One ledger. Conservation enforced at the bit level for every domain simultaneously.

### 6.3 The Open Standard Path

Phase 14 of the Babb Cat implementation plan targets submission of the ZAKO extension to pads-v1 (templates 0x07–0x0B, the DID extension to the participants block, the Island-scoped ledger architecture) as a formal standard. The submission target is either IETF (Informational RFC for the encoding, Standards Track for the chain protocol) or W3C Community Group Note (for the DID integration and sovereign identity aspects).

The submission package would include:
1. The pads-v1 wire format specification (currently in `workpads-standard/codec.md` and `pads-v2-encoding-spec.md`) as an IETF-formatted document
2. The ZAKO extension templates as an appendix to the pads-v1 spec
3. The BitLedger Universal Domain specification as a companion document
4. The chain protocol specification (currently in `workpads-standard/chain-protocol.md`) as a separate submission
5. Security analysis covering the DID integration, Keymaster-backed signing, and offline-first record creation

This submission, if accepted, makes pads-v1 an open standard that any device, platform, or service can implement for interoperability with ZAKO and Workpads.

---

## Conclusion: The Record of Everything

There is a statement in the BitLedger protocol rationale that is easy to overlook: the same intellectual act that balances a ledger column has always balanced the universe. This is not poetry. It is a structural claim about conservation laws — that the double-entry bookkeeping invented by Venetian merchants in the 13th century and the conservation laws derived by physicists in the 19th century are two expressions of the same underlying invariant.

BitPads takes this claim seriously at the wire level. The 40-bit record does not assume a specific domain. It assumes a conservation law. The same frame that records a payment records a power flow records a mass transfer. The same CRC-15 that protects a financial batch protects a telemetry batch. The same chain protocol that links an invoice to a payment links a job assignment to a completion certificate links a power event to a mode transition.

ZAKO needs all of these. It needs a protocol that is sovereign (no server dependency), offline-capable (no connectivity requirement), minimal (works on a 2G SMS link), auditable (immutable chain of Sovereign-signed records), universal (finance and engineering in the same format), and already implemented in the production Workpads ecosystem that its users already understand.

BitPads is that protocol. pads-v1 is its current production codec. The chain protocol is its exchange ledger backbone. The URL-as-document model is its portability guarantee.

ZAKO's accounting core is not something to be designed. It has already been designed, implemented, and validated. It lives in `workpads-standard/codec.md` and `bitledger/markdown/BitLedger_Protocol_v3.md` and `workpads-standard/chain-protocol.md`. Babb Cat's task is to integrate it, extend it with the five ZAKO-specific templates, anchor it to Keymaster-backed DID identity, and write `telux-ledgerd` as the service that makes it available to every app and daemon on the device.

The Sumerian clay accountants who invented the first accounting marks — one mark for one unit of grain, pressed into soft clay that hardened to stone — understood something that has been repeatedly rediscovered: the record of exchange is the foundation of trust between strangers. BitPads is that clay tablet, expressed in bits. ZAKO is the civilization that uses it.

---

*Cross-reference:*
- `ZAKO-Architecture-and-Vision.md` — foundational ZAKO OS narrative
- `BabbCat-Implementation-Plan.md` — Phase 9 (exchange ledger) for integration specifics
- `workpads-standard/codec.md` — normative pads-v1 wire format
- `bitledger/markdown/BitLedger_Protocol_v3.md` — BitLedger core specification
- `bitledger/markdown/BitLedger_Universal_Domain.md` — Universal Domain extension specification
- `workpads-standard/chain-protocol.md` — chain protocol specification
- `TeluxOS-Bedrock-Purity-Assessment.md` — CAT S22 hardware constraints
