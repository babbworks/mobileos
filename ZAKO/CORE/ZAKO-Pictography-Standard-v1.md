# ZAKO Pictography Standard
## Version 1.0

*June 1, 2026*

---

> Binary pictography is not decoration. It is a transmission discipline: fewer bits, more meaning, no schema negotiation. A 4-bit symbol drawn from a shared codebook carries a concept that would otherwise require a full field name, a type identifier, and a value. When the channel is constrained — 160 bytes of SMS, a Bluetooth LE advertisement, a QR code someone is reading across a room — the difference between symbolic and textual encoding is the difference between a message that fits and one that doesn't.

---

## 1. Purpose and Scope

This document is a sub-protocol of the ZAKO Standard. It defines:

- The ZAKO Pictography system: category identities, codebook structure, and symbol encoding
- The core ZAKO codebook (Category Identity 0x0F, 16 symbols)
- Extended domain codebooks for ZAKO specialist services
- Transmission conventions: how pictography sequences are declared, framed, and decoded
- Composition rules: how symbols from multiple codebooks are sequenced in one transmission
- The codebook extension mechanism for distribution-defined symbols

Binary pictography in ZAKO operates through the C0 Enhancement Grammar defined in the BitPads Enhancement Sub-Protocol v2.0. This document does not redefine that grammar. It defines the codebooks that make ZAKO's use of it determinate.

---

## 2. The Binary Pictography Model

### 2.1 The Problem It Solves

A BitPads Full Record is 13–29 bytes. For a daemon reporting a power mode transition, 29 bytes is an order of magnitude more than necessary. For a QR code transmitting a status update from a constrained device, every byte matters. For a BLE advertisement where the payload ceiling is 27 bytes, a full record that takes 22 of them leaves little room for anything else.

The BitPads C0 Enhancement Grammar provides a mechanism for expressing structured semantic events in 4 bits: the binary pictograph. Four bits reference one of 16 symbols in a declared codebook. One byte carries two symbols. Two bytes carry a four-symbol sequence — enough to express mode, reason, sub-entity, and status together.

This is not a new idea. Clay tablet accountants in 4th-millennium Uruk used shared symbol sets to record grain quantities without prose. The receiver knew the codebook. The mark was the message. ZAKO uses the same principle over a different medium.

### 2.2 Codebook Identity

A pictography codebook is identified by a 4-bit Category Identity. Category Identity is established by a Context Declaration Wave — a 4-byte Anonymous Wave frame that sets the active codebook for the transmission that follows.

```
Context Declaration Wave (4 bytes):
  [Meta byte: frame_type=01 Anonymous Wave]
  [Category Identity: 4 bits]
  [Version: 4 bits]
  [Reserved: 8 bits — must be 0x00]
  [Checksum: 8 bits — XOR of bytes 1–3]
```

Once a Context Declaration Wave is received, all subsequent pictography sequences in the session use that codebook until another Context Declaration Wave changes it.

ZAKO defines codebooks for Category Identities 0x0–0xF. The core ZAKO codebook occupies 0x0F. Domain codebooks occupy 0x01–0x08. Identities 0x09–0x0E are reserved. Identity 0x00 is the null codebook (no pictography active; pictography sequences are treated as raw data).

### 2.3 Symbol Encoding

A symbol is 4 bits. Symbols are packed two per byte, most significant nibble first:

```
Byte:   [SYM_A: bits 7–4] [SYM_B: bits 3–0]
```

A symbol sequence is one or more bytes carrying packed 4-bit symbols drawn from the active codebook. The sequence length is established by context (known by both sender and receiver from the transmission type). There is no length prefix within the symbol sequence itself.

### 2.4 Transmission Cost Comparison

For a power mode transition event — a common ZAKO event type:

| Encoding | Bytes | Notes |
|----------|-------|-------|
| Full BitLedger record | 22+ bytes | Full double-entry with CRC-15 |
| Full Record (minimal) | 13 bytes | No note; minimal task component |
| Pure Signal (C0) | 1 byte | Mode signal only; no reason |
| Pictography (2 symbols) | 1 byte | Mode + reason; no frame overhead |
| Pictography (4 symbols) | 2 bytes | Mode + reason + sub-entity + status |

For bulk sensor telemetry over BLE, pictographic encoding can express what would otherwise require multiple Anonymous Wave records in a fraction of the byte cost.

---

## 3. Core ZAKO Codebook (Category Identity 0x0F)

The core ZAKO codebook covers the most frequent cross-service events: power state, capability lifecycle, exchange lifecycle, and system attention signals. Every ZAKO implementation must maintain this codebook and must correctly decode all 16 symbols.

| Symbol | Hex | Name | Meaning |
|--------|-----|------|---------|
| ◼ | 0x0 | FULL | Outstack mode: Full Power — no restrictions |
| ◧ | 0x1 | NORMAL | Outstack mode: Standard — background processes permitted |
| ◑ | 0x2 | CONSERVE | Outstack mode: Conservation — background deferred |
| ◔ | 0x3 | CRITICAL | Outstack mode: Critical Reserve — only CRITICAL class runs |
| ○ | 0x4 | EMERGENCY | Outstack mode: Emergency — system shutdown procedures active |
| ↑ | 0x5 | GRANT | Capability granted to an entity |
| ↓ | 0x6 | REVOKE | Capability revoked from an entity |
| → | 0x7 | JOIN | Entity joined an Island or newgroup |
| ← | 0x8 | LEAVE | Entity left or was removed from an Island |
| ✓ | 0x9 | COMMIT | State committed; period closed; exchange settled |
| ≈ | 0xA | AMEND | Prior record amended; superseding record posted |
| ✗ | 0xB | DISPUTE | Record or chain formally disputed |
| ● | 0xC | ACK | Acknowledgement; receipt confirmed |
| ‖ | 0xD | SUSPEND | Process or service suspended by Outstack |
| ▶ | 0xE | RESUME | Process or service resumed |
| ⚠ | 0xF | ALERT | System alert; human attention required |

### 3.1 Symbol Semantics

These 16 symbols map to the most common ZAKO events across all services. When used in daemon communication, they replace Full Record overhead for high-frequency signals.

**Power mode symbols (0x0–0x4):** The five Outstack power modes. These are the most frequently transmitted pictographs in a ZAKO session — emitted by `outstack-powerd` on every mode transition, received and acted upon by all daemons. They correspond to the MODE_ENTER signal slot (slot 0x03 in the Wire Conventions §8.2), but where the C0 signal carries only the new mode, a pictographic sequence can carry mode + reason in one byte.

**Lifecycle symbols (0x5–0x8):** Four fundamental lifecycle events: grant, revoke, join, leave. These appear in Identity records, Island membership events, and capability management.

**State symbols (0x9–0xB):** COMMIT closes periods and exchanges. AMEND signals a superseding record follows. DISPUTE flags a contested state. These drive higher-order state machines in the Exchange Engine and PADS service.

**Operational symbols (0xC–0xF):** ACK, SUSPEND, RESUME, and ALERT operate as primitives across all services. ALERT is always treated with INTERACTIVE priority regardless of current Outstack mode.

### 3.2 Symbol Composition

Symbols from the core codebook can be composed in sequences to express complex state transitions without additional record overhead:

**Two-symbol composition (1 byte):**
```
[mode][reason]        e.g., 0x24 → CONSERVE + EMERGENCY_SIGNAL
[event][ack]          e.g., 0x9C → COMMIT + ACK (committed and acknowledged)
[grant][alert]        e.g., 0x5F → GRANT + ALERT (capability granted but needs attention)
```

**Four-symbol composition (2 bytes):**
```
[mode][reason][sub_entity][status]
  e.g., 0x24 0x3C → CONSERVE + EMERGENCY_SIGNAL + CRITICAL + ACK
        → "Entering Conservation mode due to emergency signal; sub-entity 3 critical; acknowledged"
```

Sub-entity symbols in four-symbol sequences reference the Personal Island sub-entity table. When the fourth symbol position is used for a sub-entity reference, it is interpreted as a sub-entity index (0–7 mapping to Personal Island slots 0–7, remainder as custom extension) rather than a codebook symbol. This interpretation is governed by the transmission pattern declared by the sender — specifically the record_sep field in the preceding batch header.

---

## 4. Domain Codebooks

Each ZAKO specialist domain has a dedicated pictography codebook. Domain codebooks compress the most frequent events within that domain into 16 symbols. They coexist with the core codebook; a transmission may switch codebooks between sequences using a new Context Declaration Wave.

### 4.1 Exchange Domain Codebook (Category Identity 0x01)

Exchange Engine events — the most time-sensitive ZAKO records. These are always CRITICAL or INTERACTIVE priority.

| Symbol | Hex | Name | Meaning |
|--------|-----|------|---------|
| $ | 0x0 | SEND | Record or value sent to counterparty |
| ⬇ | 0x1 | RECEIVE | Record or value received from counterparty |
| ✉ | 0x2 | INVOICE | Payment requested |
| ⬆ | 0x3 | PAY | Payment made |
| ? | 0x4 | QUOTE | Estimate issued |
| ✔✔ | 0x5 | AGREE | Both parties committed |
| ✅ | 0x6 | COMPLETE | Exchange fully resolved |
| ↩ | 0x7 | RETURN | Value or record returned to sender |
| ⏸ | 0x8 | HOLD | Exchange paused; counterparty response pending |
| ⏩ | 0x9 | RESUME | Exchange resumed after hold |
| ✗ | 0xA | CANCEL | Exchange cancelled; legs reversed |
| ⚖ | 0xB | DISPUTE | Exchange contested |
| ⟳ | 0xC | RETRY | Exchange retry after failure |
| ⬜ | 0xD | PENDING | Outbound leg posted; inbound leg pending |
| 🔒 | 0xE | LOCKED | Exchange locked in compound group; awaiting close |
| ⚠ | 0xF | ALERT | Exchange requires human attention |

### 4.2 Work Domain Codebook (Category Identity 0x02)

PADS work record lifecycle events.

| Symbol | Hex | Name | Meaning |
|--------|-----|------|---------|
| ↗ | 0x0 | ASSIGN | Work assigned |
| ▶ | 0x1 | START | Work begun |
| ≫ | 0x2 | PROGRESS | Progress update |
| ■ | 0x3 | FINISH | Work item completed |
| 🔍 | 0x4 | INSPECT | Inspection event |
| 📋 | 0x5 | REPORT | Report submitted |
| 💸 | 0x6 | EXPENSE | Expense recorded |
| 🚗 | 0x7 | TRAVEL | Travel event |
| ⏰ | 0x8 | OVERDUE | Deadline exceeded |
| ⇉ | 0x9 | DELEGATE | Work reassigned to another party |
| ⬛ | 0xA | BLOCK | Work blocked; dependency unresolved |
| ✓ | 0xB | DELIVER | Deliverable submitted to counterparty |
| 📎 | 0xC | ATTACH | Attachment or reference added |
| ↩ | 0xD | REJECT | Deliverable rejected by counterparty |
| ⟳ | 0xE | REVISE | Revision requested |
| ⚠ | 0xF | ALERT | Work item requires attention |

### 4.3 Health Domain Codebook (Category Identity 0x03)

Health Island event signals. These are always BACKGROUND priority unless ALERT is set.

| Symbol | Hex | Name | Meaning |
|--------|-----|------|---------|
| ❤ | 0x0 | VITALS | Vital signs recorded |
| 💊 | 0x1 | MEDICATE | Medication administered or taken |
| ⚖ | 0x2 | WEIGHT | Body weight recorded |
| 🩸 | 0x3 | GLUCOSE | Blood glucose recorded |
| 💨 | 0x4 | SPO2 | Blood oxygen recorded |
| 🌡 | 0x5 | TEMP | Temperature recorded |
| 👣 | 0x6 | STEPS | Step count updated |
| 💤 | 0x7 | SLEEP | Sleep duration recorded |
| 🍎 | 0x8 | INTAKE | Caloric intake recorded |
| 😐 | 0x9 | MOOD | Mood or pain score recorded |
| 🔔 | 0xA | REMIND | Medication or health reminder fired |
| ↑ | 0xB | HIGH | Measurement above threshold |
| ↓ | 0xC | LOW | Measurement below threshold |
| ≈ | 0xD | NORMAL | Measurement within normal range |
| ✓ | 0xE | TALLY | Health period snapshot committed |
| ⚠ | 0xF | ALERT | Health measurement requires attention |

### 4.4 Academy Domain Codebook (Category Identity 0x04)

Learning Engine and Academy service events.

| Symbol | Hex | Name | Meaning |
|--------|-----|------|---------|
| 📖 | 0x0 | STUDY | Learning session started |
| ✓ | 0x1 | COMPLETE | Lesson or module completed |
| 📝 | 0x2 | ASSESS | Assessment taken |
| ⭐ | 0x3 | MASTER | Mastery level achieved |
| 🔄 | 0x4 | REVISIT | Prior content reviewed |
| 📌 | 0x5 | ASSIGN | Content assigned by system |
| 🏆 | 0x6 | CERTIFY | Certificate or badge earned |
| 💡 | 0x7 | RECOMMEND | Recommendation generated |
| 👥 | 0x8 | COLLABORATE | Collaborative session event |
| 🔥 | 0x9 | STREAK | Streak milestone |
| ⏱ | 0xA | TIME | Time-engaged update |
| 🎯 | 0xB | GOAL | Goal set or achieved |
| ⬛ | 0xC | BLOCK | Prerequisite not met |
| ↩ | 0xD | RESET | Progress reset |
| ≫ | 0xE | PROGRESS | Progress update |
| ⚠ | 0xF | ALERT | Learning event requires attention |

### 4.5 Location Domain Codebook (Category Identity 0x05)

Places Island event signals.

| Symbol | Hex | Name | Meaning |
|--------|-----|------|---------|
| 📍 | 0x0 | FIX | Current position updated |
| ↗ | 0x1 | ARRIVE | Sovereign or entity arrived at named place |
| ↙ | 0x2 | DEPART | Sovereign or entity departed from named place |
| 🏠 | 0x3 | DEFINE | Place created and named |
| 🗺 | 0x4 | ROUTE | Route between two places recorded |
| ⬡ | 0x5 | FENCE | Geofence boundary triggered |
| ◉ | 0x6 | NEAR | Proximity event |
| 🔗 | 0x7 | LINK | Record geo-anchored to this place |
| ↩ | 0x8 | REVISIT | Historical visit recorded |
| 📡 | 0x9 | SIGNAL | Location source or accuracy note |
| ∑ | 0xA | AGGREGATE | Visit or duration aggregate |
| ✓ | 0xB | TALLY | Location period snapshot |
| ↑ | 0xC | NORTH | Cardinal direction indicator (used in compound) |
| ↓ | 0xD | SOUTH | Cardinal direction indicator |
| ← | 0xE | WEST | Cardinal direction indicator |
| ⚠ | 0xF | ALERT | Location event requires attention |

### 4.6 Social Domain Codebook (Category Identity 0x06)

People Island and social graph events.

| Symbol | Hex | Name | Meaning |
|--------|-----|------|---------|
| 👤 | 0x0 | CREATE | New contact record |
| ✏ | 0x1 | UPDATE | Contact information updated |
| 🤝 | 0x2 | MUTUAL | Bilateral relationship confirmed |
| 👋 | 0x3 | INTRODUCE | Entity A introduced to Entity B |
| 🔑 | 0x4 | TRUST | Entity marked as trusted |
| 🔗 | 0x5 | DID | DID bound or verified |
| 🚫 | 0x6 | BLOCK | Contact blocked |
| 👁 | 0x7 | FOLLOW | One-way relationship declared |
| 🏢 | 0x8 | ORG | Contact linked to organization Island |
| 💤 | 0x9 | INACTIVE | Contact marked dormant |
| ⟳ | 0xA | MERGE | Two contacts unified |
| 📩 | 0xB | SHARE | Record shared with contact |
| ↩ | 0xC | REFER | Referral from external source |
| ✓ | 0xD | TALLY | Relationship snapshot |
| 🔓 | 0xE | GRANT | Capability or access granted to contact |
| ⚠ | 0xF | ALERT | Social event requires attention |

### 4.7 Agreements Domain Codebook (Category Identity 0x07)

Agreements Island lifecycle events.

| Symbol | Hex | Name | Meaning |
|--------|-----|------|---------|
| 📄 | 0x0 | PROPOSE | Draft terms transmitted |
| ✔ | 0x1 | ACCEPT | Party accepted terms |
| ✗ | 0x2 | REJECT | Party declined terms |
| ↩ | 0x3 | COUNTER | Revised terms returned |
| ⚡ | 0x4 | ACTIVE | Agreement in force |
| ⚖ | 0x5 | BREACH | Obligation missed |
| ✏ | 0x6 | AMEND | Terms changed by mutual consent |
| ✅ | 0x7 | COMPLETE | All obligations fulfilled |
| 🏛 | 0x8 | ARBITRATE | Dispute referred to arbitrator |
| ✓✓ | 0x9 | RESOLVE | Dispute settled |
| ⏸ | 0xA | SUSPEND | Agreement paused by consent |
| ⏱ | 0xB | EXPIRE | Agreement lapsed |
| ⚠ | 0xC | ENFORCE | Term formally enforced |
| ✓ | 0xD | TALLY | Agreement period snapshot |
| 🔒 | 0xE | LOCK | Agreement locked; no further amendment |
| ⚠ | 0xF | ALERT | Agreement event requires attention |

### 4.8 Schedule Domain Codebook (Category Identity 0x08)

Calendar and scheduling events.

| Symbol | Hex | Name | Meaning |
|--------|-----|------|---------|
| 📅 | 0x0 | SCHEDULE | Event created |
| ✔ | 0x1 | CONFIRM | Attendance confirmed |
| ✗ | 0x2 | DECLINE | Attendance declined |
| 🔄 | 0x3 | RESCHEDULE | Time or location changed |
| ✗✗ | 0x4 | CANCEL | Event cancelled |
| ▶ | 0x5 | START | Event in progress |
| ■ | 0x6 | END | Event concluded |
| ✗● | 0x7 | MISS | No-show recorded |
| 🔔 | 0x8 | REMIND | Reminder delivered |
| ⟳ | 0x9 | RECUR | Recurring instance |
| ⇉ | 0xA | DELEGATE | Responsibility transferred |
| + | 0xB | JOIN | Entity added to event |
| ⬛ | 0xC | BLOCK | Time blocked; conflict noted |
| ✓ | 0xD | TALLY | Calendar period snapshot |
| ⏰ | 0xE | OVERDUE | Scheduled time has passed |
| ⚠ | 0xF | ALERT | Schedule event requires attention |

---

## 5. Transmission Conventions

### 5.1 Declaring a Codebook Context

Before emitting any pictography sequence, the sender must declare the active codebook with a Context Declaration Wave:

```
Byte 1: Meta byte
  [7:6] = 01   Frame type: Anonymous Wave
  [5:4] = 01   Priority: BACKGROUND (typical for pictography preamble)
  [3]   = 0    ACK not required
  [2]   = 1    Continuation: more follows
  [1:0] = 00   Standard

Byte 2: Category Identity (upper nibble) + Version (lower nibble)
  e.g., 0xF0 → ZAKO core codebook (0x0F), version 0

Byte 3: Reserved — must be 0x00

Byte 4: Checksum — XOR of bytes 2–3
```

The Context Declaration Wave is implicit in sessions where the codebook was established in the Session Configuration Extension byte and has not changed. In those sessions, senders may omit the Context Declaration Wave for the first pictography sequence. A change of codebook mid-session always requires a new Context Declaration Wave.

### 5.2 Pictography in Pure Signal Transmissions

The most compact ZAKO pictographic transmission is a Pure Signal byte with a trailing symbol byte:

```
[Meta byte: frame_type=00, priority bits, ACK, Continuation]
[Symbol byte: SYM_A (bits 7–4) + SYM_B (bits 3–0)]
```

Two bytes total. This is the minimum-cost state signal. It requires that the active codebook is known from prior session context — no Context Declaration Wave needed when the session has already established one.

**Example — Outstack mode change to Conservation, triggered by low battery:**
```
Meta:    0b00_10_0_0_00   (Pure Signal, INTERACTIVE, no ACK, no Continuation)
Symbols: 0x2C             (CONSERVE [0x2] + ACK [0xC])
```

Total: 2 bytes. The same information in a Full Record would cost 13–16 bytes.

### 5.3 Pictography in Extended Sequences

For richer context, a pictographic sequence may span multiple bytes. No more than 8 symbols (4 bytes) are permitted in a single pictographic sequence without an intervening structural record. This limit prevents pictographic sequences from replacing the semantic information that structured records provide — pictography is a signalling supplement, not a record substitute.

**Four-symbol sequence conventions:**

| Position | Typical Content |
|----------|-----------------|
| Symbol 1 | Primary event (what happened) |
| Symbol 2 | Cause or context (why) |
| Symbol 3 | Sub-entity or domain reference (where, to whom) |
| Symbol 4 | Status or outcome (how it resolved) |

### 5.4 Multi-Codebook Composition

A transmission may reference symbols from multiple codebooks by interleaving Context Declaration Waves with symbol sequences. This is used when a single event is meaningful to multiple domain services:

**Example — Work item completed with payment made:**
```
[Context Declaration Wave: Category 0x02, Work codebook]
[Symbol byte: 0x3B] → FINISH (0x3) + DELIVER (0xB)

[Context Declaration Wave: Category 0x01, Exchange codebook]
[Symbol byte: 0x36] → PAY (0x3) + COMPLETE (0x6)
```

Total: 4 bytes of Context Declaration Waves + 2 bytes of symbols = 6 bytes for an event that would otherwise cost two Full BitLedger records (44+ bytes). The structured records remain the canonical ledger entries; the pictographic sequence is a companion signal for high-speed system routing.

### 5.5 Relationship to Structural Records

Pictographic sequences do not replace ZAKO records. They accompany them or precede them as routing signals. The canonical record is always a BitPads frame written to `telux-ledgerd`. A pictographic sequence:

- May precede a record to signal its arrival before full decoding (reduces routing latency)
- May accompany a session that carries structured records (daemon-to-daemon status indication)
- May be the only transmission in a constrained channel where a full record cannot fit (in this case, the receiving party is responsible for generating the corresponding local ledger record if required)

When a pictographic sequence arrives without an accompanying structured record, the receiver must determine from context whether a local ledger record is required. For Outstack power signals: no ledger record is generated from the signal alone — it triggers the receiving daemon's power state change, and the daemon's own next power event record captures the state transition. For Exchange and Work signals: a corresponding structured record is expected within the same session or the next session. If the structured record does not arrive within the session, the receiver flags the pending state.

---

## 6. Codebook Extension

### 6.1 Distribution-Defined Codebooks

Category Identity 0x0 is the null codebook. Category Identities 0x1–0x8 are defined in this Standard (§4). Category Identities 0x9–0xE are reserved. Category Identity 0x0F is the core ZAKO codebook (§3).

Distributions may define additional codebooks for distribution-specific pictographic events. Distribution-defined codebooks must use a Context Declaration Wave with a Category Identity byte where the upper nibble is 0xF and the lower nibble is a distribution-defined discriminator (0–15). This gives distributions 16 additional codebook slots without conflicting with the ZAKO Standard allocation.

A distribution-defined codebook must:
1. Document all 16 symbol meanings
2. Ensure that symbols it uses do not collide with core ZAKO symbols when sequences are composed across codebooks
3. Treat receipt of unrecognised Category Identity values as no-op: do not decode, do not error; treat following bytes as raw data until the next Context Declaration Wave

### 6.2 Proposing Codebook Additions

Additions to standard domain codebooks (§4) are proposed as minor version updates to this document. A proposal must specify:
- The domain codebook to be modified (Category Identity)
- The symbol position (hex value 0x0–0xF) to be defined or redefined
- The symbol name and meaning
- The ZAKO event type this symbol represents
- At least one example transmission demonstrating use

Positions 0xE and 0xF in each domain codebook are soft-reserved for TALLY and ALERT respectively. These may be overridden by a major version only.

---

## 7. Receiver Behaviour

### 7.1 Codebook-Unknown Reception

If a receiver receives a Context Declaration Wave with an unrecognised Category Identity, it must:
1. Not error or terminate the session
2. Not attempt to decode following bytes as symbols
3. Resume normal frame parsing at the next Meta byte
4. Optionally log the unrecognised identity for diagnostic purposes

### 7.2 Symbol-Unknown Reception

Within a recognised codebook, if a symbol value is received that is not defined in the receiver's version of this document, the receiver must:
1. Not error
2. Treat the symbol as ALERT (0xF in core codebook; 0xF in all domain codebooks) — the safe fallback that flags human attention without making an incorrect semantic assumption
3. Log the unrecognised symbol value

### 7.3 Priority Inheritance

Pictographic sequences inherit the priority declared in the Meta byte of the Context Declaration Wave or Pure Signal that contains them. Receivers must honour this priority:
- Priority `11` (CRITICAL) sequences interrupt current queue
- Priority `10` (INTERACTIVE) sequences are processed before pending BACKGROUND work
- Priority `01` (BACKGROUND) sequences are processed in queue order
- Priority `00` (OPPORTUNISTIC) sequences are deferred in Conservation mode and dropped in Critical Reserve mode

The exception: any sequence containing ALERT (0xF) from either the core codebook or a domain codebook is always promoted to INTERACTIVE priority, regardless of the declared Meta byte priority. An alert that is dropped due to power mode is a protocol violation.

---

## 8. Conformance Requirements

A ZAKO distribution conforms to this document when:

1. **Core codebook is complete.** The implementation maintains all 16 symbols of the core ZAKO codebook (Category Identity 0x0F) and correctly decodes all of them.

2. **Context Declaration Wave is recognised.** The implementation correctly parses the 4-byte Context Declaration Wave and switches the active codebook accordingly. Checksum failure on the Context Declaration Wave causes the implementation to ignore the following symbol sequence and continue at the next Meta byte.

3. **ALERT is never dropped.** Any pictographic sequence containing symbol 0xF (ALERT) from any codebook is promoted to INTERACTIVE priority and is not deferred by power mode gating.

4. **Symbol sequences do not exceed 8 symbols.** The implementation rejects pictographic sequences longer than 8 symbols (4 bytes) by treating the 9th symbol onward as raw data.

5. **Pictography does not replace ledger records.** The implementation does not use pictographic signals as the canonical ledger entry. Structured BitPads frames remain the ledger record. Pictographic sequences are routing and signalling companions only — unless the structured record arrives separately, in which case the pictographic sequence is a preview of it, not a substitute for it.

6. **Distribution-defined codebooks use correct identity range.** Any distribution-defined codebook uses Category Identity bytes with upper nibble 0xF only.

---

## Appendix A: Category Identity Quick Reference

| Category Identity | Codebook | Defined In |
|-------------------|----------|------------|
| 0x00 | Null (no pictography) | This document |
| 0x01 | Exchange Domain | §4.1 |
| 0x02 | Work / PADS Domain | §4.2 |
| 0x03 | Health Domain | §4.3 |
| 0x04 | Academy Domain | §4.4 |
| 0x05 | Location Domain | §4.5 |
| 0x06 | Social Domain | §4.6 |
| 0x07 | Agreements Domain | §4.7 |
| 0x08 | Schedule Domain | §4.8 |
| 0x09–0x0E | Reserved | — |
| 0x0F | ZAKO Core | §3 |
| 0xF0–0xFF upper nibble | Distribution-defined | §6.1 |

---

## Appendix B: Core Codebook Quick Reference (0x0F)

```
0x0  FULL       Outstack Full Power mode
0x1  NORMAL     Outstack Standard mode
0x2  CONSERVE   Outstack Conservation mode
0x3  CRITICAL   Outstack Critical Reserve mode
0x4  EMERGENCY  Outstack Emergency mode
0x5  GRANT      Capability granted
0x6  REVOKE     Capability revoked
0x7  JOIN       Entity joined Island
0x8  LEAVE      Entity left Island
0x9  COMMIT     State committed / period closed
0xA  AMEND      Prior record amended
0xB  DISPUTE    Record or chain disputed
0xC  ACK        Acknowledgement
0xD  SUSPEND    Process or service suspended
0xE  RESUME     Process or service resumed
0xF  ALERT      Human attention required [always INTERACTIVE priority]
```

---

## Appendix C: Two-Byte Signal Quick Reference

Common two-byte pictographic signals used in ZAKO daemon communication. Codebook 0x0F assumed unless noted.

| Bytes | Symbols | Meaning |
|-------|---------|---------|
| `0x24` | CONSERVE + EMERGENCY | Entering conservation due to emergency signal |
| `0x9C` | COMMIT + ACK | Period committed and acknowledged |
| `0x5C` | GRANT + ACK | Capability granted and acknowledged |
| `0x6F` | REVOKE + ALERT | Capability revoked; requires attention |
| `0x7C` | JOIN + ACK | Island join acknowledged |
| `0x8C` | LEAVE + ACK | Island leave acknowledged |
| `0xAF` | AMEND + ALERT | Amendment applied; check required |
| `0xBF` | DISPUTE + ALERT | Chain disputed; immediate attention |
| `0xDF` | SUSPEND + ALERT | Critical process suspended; attention required |
| `0xEF` | RESUME + ALERT | Process resumed after critical suspend |
| `0x02` | FULL + CONSERVE | Mode oscillation (was FULL, now CONSERVE) |
| `0x40` | EMERGENCY + FULL | Recovering from emergency to full power |

---

*ZAKO Pictography Standard v1.0 — June 1, 2026*  
*Sub-protocol of ZAKO Standard v1.x*  
*Cross-reference: ZAKO Wire Conventions v1.0 §8, ZAKO Codebook Standard v1.0*
