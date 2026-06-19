# ZAKO Standard
## Version 1.0

*June 1, 2026*

---

> ZAKO is a sovereign operating system. Every interaction it mediates, every resource it governs, every relationship it records — these are conservation events. They have two sides, they must balance, and they must be provable. The protocol that enforces this at the wire level is BitPads and BitLedger. The sovereignty architecture that makes it yours is Telux. The power governance that makes it honest is Outstack. Together they are ZAKO.

---

## 1. What This Document Is

This is the ZAKO Standard. It defines:

- The canonical record format for all ZAKO data
- The service architecture and the contracts between services
- The two-engine model that organizes all activity
- The sovereignty model governing identity, Islands, and access
- The power governance model governing process execution
- The codebooks, domain extensions, and pictography conventions that express every ZAKO record type in the BitPads wire format
- The behavioral contracts that ZAKO distributions must honour

A ZAKO distribution — such as Babb Cat — is a specific expression of this Standard on specific hardware. The Standard does not change when the hardware changes. What changes is the implementation profile: which hardware capabilities are available, which features are P0 versus deferred, which carrier integrations are present.

This document is the referent. Distributions are its expressions.

---

## 2. Foundational Principles

### 2.1 Every Interaction Is a Record

ZAKO treats every meaningful event as a record in a ledger. A payment made, a task assigned, a blood pressure reading, a capability granted, a power mode transition, a journal entry written — each is a BitPads frame written to the appropriate Island's ledger, signed by the Island Sovereign, and chain-hashed against the record that preceded it. Records are never deleted. They are amended by new records that reference and supersede them.

This is not a design choice made for auditability. It is a consequence of the conservation principle: if something happened, it changed the state of the system, and that change has two sides. Both sides must be recorded. The ledger is the proof.

### 2.2 Conservation at the Wire Level

Every record in ZAKO that involves a quantity — money, energy, data, time, health measurements — is a BitLedger record. The BitLedger protocol enforces the conservation invariant at the wire level: for every batch of records, the sum of all signed flows must equal zero. This is not a rule the application must enforce. It is a structural property of the encoding. A batch that does not balance cannot be correctly formed.

Three independent error detection mechanisms apply to every BitLedger record: CRC-15 over the session header, cross-layer direction and status bit mirroring, and the conservation invariant itself. Corruption that survives any one mechanism is caught by the others.

### 2.3 Sovereignty Is the Design Constraint

A ZAKO user is the Sovereign of their device. Their records are not stored on external servers. Their identity is not issued by a third party. Their data does not leave their device without an explicit sovereign action — an action the Sovereign takes knowingly, producing a record of the sharing itself.

Sovereignty is implemented through the Telux architecture: ed25519 keys backed by hardware (Keymaster 4.0 in TrustZone where available), W3C Decentralized Identifiers (did:key method), Island containers with their own security namespace and ledger, and capability grants as the mechanism for any cross-entity access.

### 2.4 Offline-First Is Non-Negotiable

ZAKO functions fully without network connectivity. Records are created locally, stored locally, and transmitted when and how the Sovereign chooses. Transmission channels include: SMS (no data connection required), QR code (no network at all), Bluetooth LE (short range, very low power), and IP when available. No ZAKO service requires a network connection to create or read records.

### 2.5 Power Is a Sovereignty Property

A device that cannot control its own power consumption cannot guarantee its own availability. ZAKO treats power governance as a first-class architectural concern through the Outstack system. Power events are records. Power mode transitions are records. The power budget is a ledger. A ZAKO device with 10% battery remaining is a device exercising its sovereignty over which processes may continue.

---

## 3. The Protocol Foundation

### 3.1 BitPads and BitLedger

ZAKO's record format is the BitPads protocol family. Every ZAKO record is a BitPads frame. The protocol family has four layers:

**BitLedger v3.0** — the 40-bit double-entry accounting record. Five bytes encode any conserved scalar exchange between any two entities. Conservation is enforced at encoding time.

**BitLedger Universal Domain v1.0** — the same 40-bit record extended to any conserved scalar: energy, mass, data, time, service obligations. The wire format is identical. The semantic interpretation changes via a domain declaration in the session header.

**BitPads v2.0** — the meta-layer wrapping every transmission. A single Meta byte declares the frame type before the receiver processes any payload. Four frame types span a cost spectrum from 1 byte to 44 bytes.

**BitPads Enhancement Sub-Protocol v2.0** — the C0 Enhancement Grammar, reclaiming structural bits in control characters for priority, acknowledgement, and continuation signalling at 13 declared positions in every transmission. Binary pictography via shared codebooks at 4 bits per symbol.

### 3.2 The Four Frame Types

| Size | Frame | ZAKO Use |
|------|-------|----------|
| 1 byte | Pure Signal | Outstack mode heartbeat, daemon ACK, power alert |
| 4 bytes | Anonymous Wave | Peripheral sensor readings, bulk telemetry |
| 13–29 bytes | Full Record | Work records, journal entries, health readings, task records |