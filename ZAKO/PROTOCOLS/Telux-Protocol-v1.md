# Telux Protocol
## Version 1.0

*June 1, 2026*

---

> "Tel" — to communicate. Exchange is the essence of communication. Telux is the full-stack system that makes authenticated, conserved, sovereign exchange possible: within a device's own network of processes and Islands, between two devices, within newgroups of many, and across any carrier that can carry a BitPads frame. Every component of Telux exists in service of that single purpose. Identity exists so exchange can be trusted. The ledger exists so exchange can be proven. The capability system exists so exchange can be controlled. Outstack exists so exchange can survive.

---

## 1. Purpose and Scope

This document defines the Telux protocol — the full-stack exchange system at the core of every ZAKO device. Telux governs how exchange happens at every scale: internally between the device's own processes and Islands, peer-to-peer between two Sovereign devices, within newgroups of multiple participants, and outward across any carrier network that can carry a BitPads frame.

Telux is not a messaging layer added on top of a sovereign OS. It is the reason the sovereign OS exists. ZAKO is the operating environment. Telux is what it does.

In scope:
- The exchange model and why identity is its prerequisite
- The sovereign network model — a single device as a full network
- The identity foundation: DIDs, keys, Islands, capability grants
- The exchange layer: bilateral exchange, conservation enforcement, the Exchange Engine
- Newgroup exchange: multi-party shared contexts
- Cross-network exchange: how Telux networks reach each other
- The carrier layer: SMS, BLE, QR, IP as transport surfaces
- The Telux daemon triad: `telux-ledgerd`, `telux-identd`, `telux-sharedb`
- Outstack as Telux's operational companion
- Conformance requirements

---

## 2. Exchange as the Primary Goal

### 2.1 Why Exchange Is the Foundation

Every meaningful thing a device does involves exchange. A payment is an exchange of value. A work record is an exchange of obligation. A health reading is an exchange between a sensor and a ledger. A capability grant is an exchange of access. A message is an exchange of meaning. Even a power mode transition is an exchange of state between Outstack and every listening process.

Exchange requires three things to be trustworthy:

1. **Identity** — you must know who you are exchanging with
2. **Record** — the exchange must be provable after the fact
3. **Conservation** — what is given and what is received must balance

These three requirements define Telux's architecture entirely. Identity is the foundation. The ledger is the proof. Conservation is the law. Everything else is implementation.

### 2.2 Identity Exists Because of Exchange

Identity in Telux is not a feature of the system. It is the prerequisite for exchange. A Sovereign establishes their identity not as an end in itself but so that counterparties can trust the records they receive, so that capability grants can be addressed to specific entities, so that exchange can occur between parties who may have never met in person and may be separated by distance, time, or carrier incompatibilities.

This ordering matters for how the system is built. Identity serves exchange. When a design choice must be made between making identity easier and making exchange more trustworthy, exchange wins. Identity is the means. Exchange is the end.

### 2.3 The Ledger Exists Because of Exchange

The ledger is not a database of user data. It is the proof of exchange. Every record in `telux-ledgerd` is a record of something that happened between two parties — even when one of those "parties" is the device itself, or a daemon, or a sensor. The ledger is what makes exchange irrefutable. A payment that has no ledger record did not happen in Telux's world. A capability grant with no ledger record carries no authority.

### 2.4 Conservation Is the Exchange Law

The conservation invariant — that for every batch of exchange records, signed flows sum to zero — is Telux's fundamental law. It is enforced at the wire level by BitLedger. It cannot be bypassed by application code. An exchange that does not conserve is not an exchange; it is an error. The law applies equally to financial flows, power flows, work unit flows, and any other conserved quantity ZAKO tracks.

---

## 3. The Sovereign Network

### 3.1 A Device Is Already a Network

A single ZAKO device running Telux is not a terminal, a client, or an endpoint. It is a sovereign network — a collection of named nodes (Islands, daemons, sub-entities) exchanging signed, conserved records over the BitPads protocol. The protocol governing these internal exchanges is identical to the protocol governing exchanges between two people across an SMS link.

The internal network of a ZAKO device:

```
outstack-powerd     ←→     telux-ledgerd
telux-identd        ←→     Exchange Engine
Personal Island     ←→     Work Islands (×16)
Health Island       ←→     Academy Island
SIMBA Nodes         ←→     Capability grants from Identity sub-entity
```

Every one of these relationships is a Telux exchange. Records flow with DIDs, signatures, and chain hashes. The Conservation invariant applies. The wire format is BitPads. Nothing about the internal architecture is special — it is the same protocol at a shorter distance.

### 3.2 The Fractal Architecture

Telux operates self-similarly at every scale:

| Scale | Exchange | Protocol |
|-------|----------|----------|
| Record | Two accounts exchange a quantity | BitLedger conservation |
| Sub-entity | Slots within an Island exchange records | BitPads frames + capability gates |
| Device | Daemons and Islands exchange | Full Telux stack |
| Device-to-device | Two sovereign networks exchange | Full Telux stack |
| Newgroup | Multiple sovereign networks share a context | Full Telux stack + newgroup anchor |
| Distribution | Meta-network of sovereign networks | Full Telux stack + shared infrastructure |

The same protocol at every layer is not an accident. It means that a developer who understands how two daemons exchange power signals understands how two Sovereigns exchange payment records. The concepts do not change — only the distance.

### 3.3 Offline Is Not Disconnection

When a ZAKO device has no external carrier, it is not "offline." It is operating as a self-contained sovereign network. Internally it is fully functional, fully conserved, fully exchanging. The ledger grows. Processes communicate. Islands record events. When a carrier becomes available, two sovereign networks exchange — they do not "reconnect to a server." There is no server. There are networks, and there are the roads between them.

---

## 4. The Identity Foundation

### 4.1 Decentralized Identity

Every Telux participant — a Sovereign, a daemon, a SIMBA Node, a newgroup node — is identified by a W3C Decentralized Identifier using the `did:key` method. A DID is derived directly from a public key:

```
did:key:z6Mk<BASE58BTC(ed25519_public_key)>
```

No external registry. No certificate authority. No network lookup. The DID is verifiable by anyone in possession of the public key. It is valid for as long as the keypair is valid.

### 4.2 Key Material

The Sovereign's signing key is generated on-device. In hardware-backed deployments (Keymaster 4.0 / TrustZone), the private key never leaves the Trusted Execution Environment. In software-backed deployments, the key is encrypted at rest and zeroed in memory on device lock. The private key is never transmitted, backed up, or exported without an explicit Sovereign action that itself produces a signed ledger record.

Key rotation — replacing a signing key while maintaining identity continuity — is accomplished through a chain of BIND (task_code=0x0D) and ATTEST (task_code=0x0E) records: the old key signs the new key's DID; the new key attests continuity. The chain is locally verifiable. No external authority is required.

### 4.3 Islands

An Island is a named, sovereign container for a set of related exchange records. It is a node on the device network with its own ledger namespace, its own security context, and its own Sovereign. No process accesses an Island's records without a capability grant from that Island's Sovereign.

The Personal Island is the device's foundational node. It is created at provisioning and cannot be deleted. Its sub-entity slots are statically assigned (System, Identity, Exchange, Journal, Health, Academy, People, Places). Work Islands (sub_entity 16–31) are nodes created dynamically within the PADS service for work contexts.

Each Island's identity is derived from its genesis record:

```
island_id = BASE58URL(BLAKE3(sovereign_public_key || provisioning_epoch))
```

### 4.4 Capability Grants

A capability grant is the mechanism by which a Sovereign extends access to their Island's records to another entity. Without a grant, no process — internal or external — accesses the Island. A grant is a Full BitLedger record (task_code=0x08 GRANT) signed by the granting Sovereign, addressed to the grantee's DID, carrying a capability scope bitmask.

Grants are revocable immediately. A REVOKE record (task_code=0x09) cancels the grant; all downstream delegations are also cancelled. There is no grace period.

The capability grant is Telux's access control mechanism. It applies to every participant: native daemons, SIMBA Nodes, counterparty Sovereigns, and newgroup nodes. The rule is uniform — access without a grant does not exist.

### 4.5 The Identity Lock

When the device is locked (screen locked, user absent), `telux-identd` refuses new signing operations. Incoming records are staged unsigned. On unlock, the staging queue is processed in FIFO order: each record signed, then posted to `telux-ledgerd`. No staged record enters the canonical chain unsigned.

---

## 5. The Exchange Layer

### 5.1 The Exchange Engine

The Exchange Engine is the bilateral settlement core of Telux. It mediates every exchange that involves a counterparty — a payment, an invoice, an agreement acceptance, a work delivery — enforcing conservation across both legs before either posts.

Exchange Engine records are CRITICAL priority. They run in all Outstack modes including Emergency. An exchange that was initiated before a power mode drop is not abandoned — it is held pending and completed when both legs are present.

The bilateral exchange pattern:

```
Sovereign A → SEND record (value outflow) → Sovereign B
Sovereign B → RECEIVE record (value inflow) → Sovereign A
Exchange Engine: verify sum of signed flows = 0 → post both atomically
```

If the second leg does not arrive within the session, the first leg is held pending. The Exchange Engine never posts a single leg. Conservation is enforced at posting time, not at record creation time.

### 5.2 The SEND/RECEIVE Cycle

Every exchange between two Sovereigns follows the same cycle regardless of what is being exchanged:

```
SEND    (0x18) — record or value transmitted to counterparty
RECEIVE (0x19) — record or value received from counterparty
ACK     (0x1A) — receipt acknowledged
```

The full payment variant adds:

```
INVOICE (0x1B) — payment requested
PAY     (0x1C) — payment made
AGREE   (0x1E) — mutual commitment confirmed
COMPLETE (0x1F) — exchange fully resolved
```

These verbs are not application conventions. They are Telux protocol primitives. Any exchange between any two parties — financial, work, health data, capability delegation — flows through this vocabulary.

### 5.3 Conservation in Exchange

Every Full BitLedger frame carries the conservation invariant. For a batch of exchange records:

```
Σ (signed_value × direction) = 0
```

Where direction=0 is Outflow and direction=1 is Inflow. This is enforced by `telux-ledgerd` at write time. A batch that does not balance is rejected. A LEDGER_REJECT signal (C0 slot 0x06, category 0x1) is emitted. The Exchange Engine does not post partial results.

Three independent error detection mechanisms apply to every BitLedger record: CRC-15 over the session header, cross-layer direction/status bit mirroring, and the conservation invariant itself. Corruption that survives any one mechanism is caught by the others.

---

## 6. Newgroup Exchange

### 6.1 What a Newgroup Is

A newgroup is an emergent network that spans two or more sovereign device networks. When Sovereigns need to share a common record context — a multi-party agreement, a shared work context, a community ledger — they create a newgroup. The newgroup is not hosted on any one device. It exists as a shared record anchor whose truth is the frame_hash of its founding CREATE record.

The newgroup is Telux operating at its natural scale: multiple sovereign networks exchanging through a shared context, each maintaining their own sovereign copy, each verifiable by any other participant.

### 6.2 The Shared Truth Anchor

The founding CREATE record's frame_hash is the newgroup's shared truth. It encodes who the participants are, what the context is, and when it was established — all signed by the founding Sovereign. Any participant who holds a record referencing this frame_hash, with a valid sovereign_sig from a named participant, holds a verifiable piece of the shared context.

There is no server holding the canonical copy. The canonical state is the set of consistently-signed, correctly-chained records that all participants hold. Consistency is verifiable. Contradiction is detectable.

### 6.3 Hosted Newgroups

When a distribution operates infrastructure nodes, a newgroup may be hosted: a designated Ledger Node accepts submissions from all participants, chain-hashes them in arrival order, countersigns each, and distributes to all enrolled parties. The Ledger Node provides total ordering and real-time distribution without replacing the participants' sovereign copies. Each participant retains their own copy; the Ledger Node is a coordination service, not the owner of the records.

### 6.4 Newgroup Lifecycle

```
CREATE  (0x0C) — founding Sovereign establishes the newgroup
JOIN    (0x12) — participant invited
AGREE   (0x1E) — participant accepts and is enrolled
LEAVE   (0x13) — participant departs voluntarily
REMOVE  (0x14) — founding Sovereign removes a participant
CLOSE   (0x11) — newgroup formally closed; no further records accepted
```

All lifecycle records are signed by their author Sovereign and chain-hashed into the newgroup's ledger. The founding Sovereign's authority over REMOVE and CLOSE is established by the founding CREATE record's source_did.

---

## 7. Cross-Network Exchange

### 7.1 Network-to-Network

When two ZAKO devices exchange, two sovereign networks are connecting — not a client connecting to a server, not a user connecting to an account. Each device is a full network with its own Islands, daemons, ledger, and identity. The exchange happens at the network boundary: SEND records cross the boundary outbound; RECEIVE records confirm the crossing inbound.

The protocol is identical regardless of which carrier the frames travel over. A SEND record is a SEND record whether it travels via SMS, BLE, QR code, or IP. The carrier is transport. Telux is the language.

### 7.2 External Networks

Telux devices may exchange with parties who are not running ZAKO. The compatibility bridge is pads-v1 — the Workpads URL encoding that expresses Telux records as `#1pa/` URLs transmissible over any text channel. When a ZAKO device sends an invoice to a party running a legacy Workpads client, `telux-sharedb` encodes the record as a pads-v1 URL. When a pads-v1 URL arrives inbound, the pads-v1 codec decodes it into a BitPads frame and submits it to `telux-ledgerd` via the standard intake path.

Inbound pads-v1 records, once decoded, are indistinguishable in the ledger from natively-created BitPads records. They are stored, chain-hashed, and served to Service Views identically. The compatibility layer is in the codec, not in the record model.

---

## 8. The Carrier Layer

### 8.1 Carriers Are Roads, Not Networks

SMS, Bluetooth LE, QR code, IP — these are not "the network." They are the roads between networks. Telux is what the networks speak. The roads carry the packets but have no say in what the packets mean or whether the exchange conserves. A Telux exchange conducted over SMS is the same exchange as one conducted over IP. The carrier changes the latency and the byte cost. The protocol does not change.

### 8.2 Carrier Characteristics

| Carrier | Typical ceiling | Key property |
|---------|----------------|--------------|
| QR code | ~2,953 bytes | Zero network dependency; visual; offline |
| SMS | 160 bytes (1 segment) / 1,120 bytes (7 segments) | No data connection; global reach |
| Bluetooth LE | 27 bytes advertisement / larger via GATT | Short range; very low power |
| IP (WiFi/cellular) | Practical unlimited | High throughput; requires connectivity |

### 8.3 Carrier Selection

`telux-sharedb` selects the carrier based on: what the receiving party's device can accept (declared in their DID document service endpoints), what carriers are currently available on the sending device, and what the Outstack power mode permits. In Critical Reserve mode, IP is preferred for its efficiency; BLE is permitted; QR is always permitted (no transmit power cost); SMS is a last resort (radio transmit power cost).

The Sovereign may override carrier selection. Carrier selection does not affect the record — only the transport.

---

## 9. The Telux Daemon Triad

### 9.1 telux-ledgerd

The ledger daemon. CRITICAL process class. The single point of truth for all ZAKO records. It accepts signed frames, validates them (signature, JOURNAL invariant, conservation, CRC-15, compound group integrity, chain hash continuity), computes and stores the chain hash, and emits LEDGER_ACK or LEDGER_REJECT. Every record is fsynced before ACK — there is no deferred durability. The ledger is append-only: no UPDATE, no DELETE, no reordering. Amendment is a new record referencing the one it supersedes.

### 9.2 telux-identd

The identity daemon. CRITICAL process class. Sole custodian of the Sovereign's private key material. All signing operations pass through `telux-identd` — no other process touches private key bytes. It manages capability grant issuance and revocation, DID document generation, key rotation records, and the identity lock state. When locked, it queues signing requests for processing on unlock. When Outstack signals a mode change, `telux-identd` processes the mode's implications for pending capability operations before any other daemon.

### 9.3 telux-sharedb

The sharing daemon. INTERACTIVE process class. Mediates all outbound record transmission from the device. Enforces RESTRICT_FORWARD at the application layer (SELinux enforces it at the kernel layer for Journal records independently). Encodes records for transmission via the appropriate carrier. Generates SEND records in the Exchange sub-entity as proof of sharing. Manages the outbound queue for records awaiting carrier availability. Decodes inbound pads-v1 URLs and submits them to `telux-ledgerd` via the standard intake path.

### 9.4 The Triad as Exchange Infrastructure

The three daemons are named "telux-" because they are the infrastructure of exchange. The ledger proves exchange happened. Identity enables exchange to be trusted. The sharing daemon makes exchange travel. Together they are the operating infrastructure of the Telux protocol on a ZAKO device.

---

## 10. Outstack: Born from Telux's Operating Constraint

Telux was designed to enable the most critical exchange under the harshest conditions. In the field: low battery, no network, shared spectrum, degraded hardware. In the street: a single SMS as the only channel. In a power outage: a device at 4% battery that still needs to complete a payment.

The question "how do you keep exchange alive when resources are scarce?" demanded its own system. That system is Outstack. Outstack was born from Telux's operating constraint — it exists because Telux must not fail, and because sustaining exchange requires radical, principled prioritization of every process on the device.

Outstack is not a separate concern from Telux. It is Telux's operational companion: the system that ensures exchange survives by governing every claim on the device's energy. A Telux system without Outstack is a system that will die when the battery hits 3% and take unsettled exchanges with it. A Telux system with Outstack is a system that knows what must survive and protects it to the last available milliwatt.

The full Outstack specification is defined in the Outstack Protocol v1.0 document.

---

## 11. Conformance Requirements

A ZAKO distribution conforms to the Telux Protocol when:

1. **Exchange is the design criterion.** Identity, ledger, and capability decisions are made in service of exchange. No implementation choice that degrades exchange trustworthiness is justified by convenience.

2. **DIDs are self-issued.** No Sovereign identity is issued by an external authority. All DIDs use the `did:key` method derivable from on-device public keys.

3. **The ledger is the proof.** Every meaningful exchange produces a ledger record. No exchange of sovereign significance occurs without a corresponding `telux-ledgerd` entry.

4. **Conservation is enforced.** The conservation invariant is applied by `telux-ledgerd` at write time for all Full BitLedger frames. No partial posting of Exchange Engine records.

5. **Carriers are interchangeable.** A record transmitted via SMS is the same record as one transmitted via IP. The carrier changes the transport; the protocol does not change.

6. **Internal exchanges use the same protocol as external exchanges.** Daemon-to-daemon records, Island-to-Island capability grants, and Outstack power signals all use BitPads frames. There is no separate internal messaging format.

7. **The daemon triad is CRITICAL.** `telux-ledgerd`, `telux-identd`, and the core exchange functions of `telux-sharedb` run at CRITICAL process class and are never gated by Outstack regardless of power mode.

8. **Newgroups are sovereign.** No newgroup requires a central server for its core operation. The per-party sovereign copy is always available as the default model.

---

## Appendix A: Telux Exchange Vocabulary Quick Reference

```
SEND      (0x18) — record or value transmitted to counterparty
RECEIVE   (0x19) — record or value received from counterparty
ACK       (0x1A) — receipt acknowledged
INVOICE   (0x1B) — payment requested
PAY       (0x1C) — payment made
QUOTE     (0x1D) — estimate issued
AGREE     (0x1E) — mutual commitment confirmed
COMPLETE  (0x1F) — exchange fully resolved

GRANT     (0x08) — capability extended to entity
REVOKE    (0x09) — capability withdrawn
DELEGATE  (0x0A) — capability sub-delegated
BIND      (0x0D) — DID or key bound to entity
ATTEST    (0x0E) — signed assertion made
```

## Appendix B: The Exchange Invariant

For any batch of BitLedger records in Telux:

```
Σ (value[i] × sign(direction[i])) = 0

where direction = 0 (Outflow) → sign = -1
      direction = 1 (Inflow)  → sign = +1
```

This must hold for every settled Exchange Engine batch. Enforced by `telux-ledgerd`. No exceptions.

## Appendix C: Telux at Scale

```
1 device    = 1 sovereign network
              (Islands + daemons + sub-entities exchanging internally)

2 devices   = 2 sovereign networks exchanging
              (bilateral Telux exchange via any carrier)

N devices + newgroup = N sovereign networks + emergent shared exchange context

Distribution = meta-network of sovereign networks sharing
               ZAKO protocol version, codebook, Outstack profile, infrastructure
```

---

*Telux Protocol v1.0 — June 1, 2026*  
*Core protocol of ZAKO v1.x*  
*Cross-reference: Outstack Protocol v1.0, ZAKO Wire Conventions v1.0, ZAKO Codebook Standard v1.0, SIMBA Standard v1.0*
