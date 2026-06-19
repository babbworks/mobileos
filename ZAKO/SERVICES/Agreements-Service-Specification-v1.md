# Agreements Service Specification
## Version 1.0

*June 1, 2026*

---

> An agreement is not a document stored on a server. It is a set of signed records, each held by the party who signed it, each verifiable against the frame_hash they share. The shared truth is not a location — it is a cryptographic fingerprint. The server holding the canonical copy is optional infrastructure, not a sovereignty requirement.

---

## 1. Purpose and Scope

This document is a service specification within the HOME Standard. It defines the Agreements service: the per-party sovereign copy model, the hosted newgroup variant, the agreement lifecycle, and the integration with the Exchange Engine for obligation enforcement.

The Agreements service stores and manages bilateral and multi-party agreements — contracts, commitments, terms of service, capability delegation agreements, and any other structured mutual commitment between two or more Sovereigns.

In scope:
- The per-party sovereign copy model (default)
- The hosted newgroup model (infrastructure variant)
- Agreement lifecycle records and state machine
- Breach, amendment, suspension, and arbitration records
- Integration with Exchange Engine for obligation settlement
- Conformance requirements

---

## 2. The Sovereignty-First Model

### 2.1 Per-Party Sovereign Copy

The default model for HOME agreements is that each party holds their own copy of the agreement records, signed by themselves, on their own device. There is no shared server. There is no synchronization requirement.

The parties' records are linked not by location but by the frame_hash of the agreement's founding CREATE record — the shared truth anchor. Any party in possession of the other party's records can verify:

1. Both parties' records reference the same CREATE frame_hash
2. Each record is signed by the DID of the party who wrote it
3. The chain of records traces a consistent history from the CREATE record forward

This is sufficient for most agreement contexts. If a dispute arises, both parties present their chain of signed records. The chain that is complete and consistently signed is authoritative.

### 2.2 What the frame_hash Anchors

The founding CREATE record's frame_hash is the shared truth. It encodes:
- The agreement terms (in the note component or as a hash of an external document)
- The parties (source_did and dest_did, plus additional party DIDs in the note component for multi-party agreements)
- The creation epoch (wall_ts)
- The Sovereign's signature over all of the above

A party cannot repudiate their acceptance of the agreement terms without repudiating their own AGREE record, which is signed by their own key and references the CREATE frame_hash. The cryptographic chain makes repudiation provably false.

### 2.3 Agreements Island

Agreement records are stored in the Agreements Island, a dedicated context within the Personal Island's custom domain space. The Agreements Island uses:

```
domain      = 11     Custom
domain_ext  = 0x07   Agreement domain
file_sep    = 2      Exchange sub-entity (bilateral agreements are exchange events)
record_sep  = assigned per agreement (one record_sep per active agreement)
```

Each active agreement occupies one record_sep slot (0–31) within the Exchange sub-entity. The record_sep is the agreement's session identifier for all wire-level records related to that agreement.

---

## 3. Agreement Lifecycle

### 3.1 State Machine

```
[PROPOSE] → [COUNTER]* → [ACCEPT] → [ACTIVE]
                ↓                       ↓
            [REJECT]           [BREACH | AMEND | SUSPEND]
                                        ↓
                               [ARBITRATE | RESOLVE]
                                        ↓
                               [COMPLETE | EXPIRE | ENFORCED]
```

Each state transition is a BitPads record. No mutable state field exists — state is derived from the most recent relevant record in the chain.

### 3.2 PROPOSE (CREATE + AGREE draft)

The proposing party creates a draft agreement:

```
task_code    = 0x0C   CREATE
account_pair = 0000   (Proposed — Agreement domain)
domain       = 11; domain_ext = 0x07
file_sep     = 2      Exchange sub-entity
record_sep   = <assigned slot>
group_sep    = 0
value        = term_count
source_did   = proposing_sovereign_did
dest_did     = counterparty_did
note         = <terms text or BLAKE3 hash of external document>
```

The CREATE record's frame_hash becomes the agreement's shared truth anchor. The proposing party transmits this record to the counterparty via `telux-sharedb`.

### 3.3 COUNTER

A counterparty who does not accept the proposed terms may counter-propose. A COUNTER record references the original CREATE frame_hash and carries revised terms:

```
task_code    = 0x17   DISPUTE  [repurposed: in Agreement domain, DISPUTE = Counter-Proposal]
account_pair = 0011   (Counter — Agreement domain)
value        = revision_number  (increments per counter)
source_did   = counterparty_did
dest_did     = proposing_sovereign_did
note         = <revised terms or revised terms hash>
```

Counter-proposals may chain: each references the prior frame_hash in its note component. The final accepted version's frame_hash becomes the agreement anchor.

### 3.4 ACCEPT

Acceptance is an AGREE record referencing the CREATE (or final COUNTER) frame_hash:

```
task_code    = 0x1E   AGREE
account_pair = 0001   (Accepted — Agreement domain)
domain       = 11; domain_ext = 0x07
value        = accepted_frame_hash_truncated
source_did   = accepting_sovereign_did
dest_did     = proposing_sovereign_did
```

Each party that is party to the agreement must submit their own AGREE record. The agreement enters ACTIVE state when all parties have submitted AGREE records referencing the same frame_hash, as verified by the Exchange Engine.

### 3.5 REJECT

A definitive rejection:

```
task_code    = 0x17   DISPUTE  [in Agreement domain: used as Rejection when account_pair=0010]
account_pair = 0010   (Rejected — Agreement domain)
source_did   = rejecting_sovereign_did
value        = rejected_frame_hash_truncated
```

A REJECT terminates the agreement thread. No further records are accepted on that record_sep. The record_sep slot is freed for reuse.

### 3.6 ACTIVE state records

Once all parties have submitted AGREE records, the Exchange Engine posts an OPEN record marking the agreement as active:

```
task_code    = 0x10   OPEN
account_pair = 0100   (Active — Agreement domain)
source_did   = first_agreeing_sovereign_did
value        = active_from_epoch
```

From this point, obligation records may be posted against the agreement.

### 3.7 BREACH

A party records a breach when an obligation has not been met:

```
task_code    = 0x17   DISPUTE
account_pair = 0101   (Breached — Agreement domain)
domain       = 11; domain_ext = 0x07
value        = breached_obligation_frame_hash_truncated
source_did   = alleging_sovereign_did
dest_did     = breaching_party_did
note         = <breach description>
```

A BREACH record does not automatically terminate the agreement. It is a formal record that a party has failed to meet an obligation. Subsequent resolution paths: RESOLVE (breach acknowledged and remedied), ARBITRATE, or escalation.

### 3.8 AMEND

Terms amended by mutual consent. Requires a new AGREE record from all parties referencing the AMEND record's frame_hash:

```
task_code    = 0x16   AMEND
account_pair = 0110   (Amended — Agreement domain)
value        = amendment_number
note         = <amended terms or delta>
source_did   = proposing_amendment_did

[followed by new AGREE records from all parties]
```

### 3.9 SUSPEND

Agreement temporarily paused by mutual consent. Requires AGREE records from all parties:

```
task_code    = 0x06   SUSPEND
account_pair = 1010   (Suspended — Agreement domain)
value        = suspension_duration_days  (0 = indefinite)
```

### 3.10 COMPLETE

All obligations fulfilled:

```
task_code    = 0x1F   COMPLETE
account_pair = 0111   (Completed — Agreement domain)
value        = completion_epoch
```

### 3.11 EXPIRE

Agreement lapsed by time without completion:

```
task_code    = 0x0B   EXPIRE
account_pair = 1011   (Expired — Agreement domain)
value        = expiry_epoch
```

EXPIRE records are generated by `telux-identd` automatically when an agreement's defined duration elapses without a COMPLETE record.

### 3.12 Tally Records

Agreements with periodic obligation settlements use Tally records to close each settlement period:

```
task_code    = 0x15   COMMIT
account_pair = 1101   State Commit
domain       = 11; domain_ext = 0x07
group_sep    = 63     Period Close
value        = obligations_settled_in_period
```

---

## 4. The Hosted Newgroup Variant

### 4.1 When Hosting Is Used

For distributions that operate micro-servers or CDNs, a hosted newgroup provides a shared canonical ledger for the agreement. This is appropriate when:

- Three or more parties need a single shared reference
- The agreement involves frequent obligation records that all parties must see in real time
- The distribution operates infrastructure that can serve as a trusted intermediary

### 4.2 Hosted Newgroup Architecture

The distribution operates a Newgroup Ledger Node — a HOME-compatible ledger service that:

1. Accepts agreement records from all parties
2. Chain-hashes records in arrival order
3. Signs each stored record with the Newgroup Node's own key (an additional signature alongside the submitting party's sovereign_sig)
4. Makes the chain available to all parties for read access via the standard `telux-ledgerd` read protocol

The Newgroup Node's signing key is published in the agreement's CREATE record. Any party can verify that a record came from the Newgroup Node without trusting the node's contents — they verify the Node's signature and the submitting party's sovereign_sig independently.

### 4.3 Selecting the Model

The CREATE record carries a `newgroup_type` flag in its note component (or in a distribution-defined extension field):

```
newgroup_type = "sovereign"   Per-party copy (default)
newgroup_type = "hosted"      Hosted Newgroup Node
newgroup_type = "peer"        Peer-to-peer replicated (all parties replicate to all)
```

If no `newgroup_type` is specified, the implementation assumes "sovereign" (per-party copy, no synchronization).

### 4.4 Hosted Node Conformance

A Hosted Newgroup Node operated by a HOME distribution must:

1. Accept records signed by any enrolled party DID
2. Chain-hash and countersign each accepted record within 2 seconds of receipt
3. Make the chain readable to all enrolled parties
4. Never modify, delete, or reorder stored records
5. Publish its own signing DID in a distribution-accessible registry
6. Maintain an append-only ledger equivalent to `telux-ledgerd` conformance on-device

---

## 5. Exchange Engine Integration

When an agreement contains financial obligations (payment schedules, fees, penalties), those obligations are settled via the Exchange Engine using the standard compound pattern:

```
Record 1 — Obligation settlement (Completeness=1):
  task_code    = 0x1C   PAY
  account_pair = 0001   (Asset/Liability in Financial domain)
  domain       = 00     Financial
  value        = obligation_amount
  source_did   = paying_party_did

Record 2 — Agreement reference (1111, sub_type=00, Completeness=0):
  account_pair = 1111
  task_code    = 0x1A   ACK
  value        = agreement_frame_hash_truncated
  note         = "obligation: period 3"
```

The conservation invariant applies: the paying party's PAY record and the receiving party's RECEIVE record must balance within the Exchange Engine batch.

---

## 6. Conformance Requirements

1. **Per-party sovereign copy is the default.** Implementations must not require a shared server for basic agreement creation and lifecycle management.

2. **frame_hash is the shared truth anchor.** All records in an agreement lifecycle reference the founding CREATE frame_hash. Implementations must store and index this field for efficient lookup.

3. **EXPIRE is system-generated.** `telux-identd` monitors active agreements for expiry and automatically generates EXPIRE records at the defined expiry epoch.

4. **BREACH does not auto-terminate.** A BREACH record is a claim, not a termination. The agreement remains in a disputed-active state until RESOLVE, COMPLETE, or ARBITRATE.

5. **AMEND requires fresh AGREE records.** An AMEND record without corresponding AGREE records from all parties does not change the agreement terms. The prior terms remain in effect.

6. **Hosted Node countersigns within 2 seconds.** A Hosted Newgroup Node that cannot meet this latency must queue submissions and emit a SUSPEND signal to enrolling parties.

---

## Appendix: Agreement State Derivation

State is derived at read time from the most recent relevant record:

```
Most recent record        → Derived state
CREATE only               → Proposed (awaiting counterparty)
AGREE (partial parties)   → Partially accepted
AGREE (all parties)       → Active
BREACH                    → Disputed-active
AMEND + partial AGREE     → Amendment pending
AMEND + all AGREE         → Active (amended)
SUSPEND + all AGREE       → Suspended
COMPLETE                  → Completed
EXPIRE                    → Expired
```

---

*Agreements Service Specification v1.0 — June 1, 2026*  
*Sub-protocol of HOME Standard v1.x*  
*Cross-reference: Telux Protocol v1.0 §9, HOME Codebook Standard v1.0 §7.5*
