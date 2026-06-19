# ZAKO Infrastructure Extension Profile
## Version 1.0

*June 1, 2026*

---

> Sovereignty does not require isolation. A Sovereign who operates their own infrastructure — a micro-server, a CDN node, a community ledger relay — is more sovereign than one who depends on others'. The Infrastructure Extension Profile defines what it means to be an infrastructure participant in the ZAKO network: the additional requirements, the additional capabilities, and the additional obligations. It is an extension of the ZAKO Standard, not a departure from it.

---

## 1. Purpose and Scope

This document is an extension profile of the ZAKO Standard. It defines the additional requirements, capabilities, and behavioural contracts for ZAKO distributions that operate infrastructure: micro-servers, CDN nodes, community ledger relays, and hosted newgroup services.

A distribution that implements this profile extends the baseline ZAKO Standard. All baseline requirements continue to apply. This profile adds requirements for the infrastructure layer.

In scope:
- Infrastructure participant roles and their definitions
- Multi-device model (personal device + infrastructure node)
- Hosted Newgroup Ledger Node (full specification)
- CDN content hosting model
- Service endpoint publication
- Telux-ledgerd replication model for infrastructure nodes
- Revenue sharing and distribution records
- Conformance requirements for infrastructure participants

Out of scope:
- Network topology, routing, and IP infrastructure design
- Hardware specifications for infrastructure nodes
- Legal and regulatory compliance for infrastructure operators

---

## 2. Infrastructure Participant Roles

### 2.1 Role Definitions

The ZAKO infrastructure network defines four participant roles. A single distribution may implement one or more roles.

**Personal Device (baseline):**  
A standard ZAKO device implementing the ZAKO Standard. No infrastructure obligations. The sovereign reference implementation.

**Relay Node:**  
A device or server that forwards ZAKO records between parties on behalf of Sovereigns. The relay node does not store records beyond the forwarding buffer. It has no ledger. It authenticates senders and receivers by DID and delivers records without reading their contents.

**Ledger Node:**  
A device or server that operates a `telux-ledgerd`-equivalent append-only ledger for one or more hosted newgroups or community contexts. It stores records durably, chain-hashes them, countersigns them, and makes them available to enrolled parties. The Hosted Newgroup Ledger Node specified in the Agreements Service Specification is a Ledger Node.

**Content Node:**  
A device or server that stores and delivers ZAKO-compatible content (Academy course content, document references, media assets). It operates as a CDN for ZAKO content, caching and serving content to enrolled devices. It does not process records — it serves content referenced by records.

### 2.2 Node Identity

Every infrastructure node is a ZAKO entity with its own sovereign identity:

```
Node DID:     did:key:z6Mk<node_public_key>  [standard did:key method]
Node type:    published in node's DID document service endpoint
Node scope:   the set of communities or distributions it serves
```

A node's DID is published by the distribution that operates it. Participants verify node identity by resolving the DID (locally, from a published key) before enrolling.

---

## 3. Multi-Device Model

### 3.1 Personal Device + Node

A Sovereign who operates personal infrastructure (a home server, a Raspberry Pi, a VPS) extends their sovereign boundary to that device. The personal device and the infrastructure node share the Sovereign's identity context:

- The infrastructure node holds a delegated signing key (DELEGATE record from the Sovereign's primary key)
- Records written by the infrastructure node carry the delegated DID as `source_did`
- The Sovereign's primary device can verify infrastructure node records as authentically sovereign

The delegation is bounded:
- Delegation depth: 1 (node cannot further delegate)
- Scope: defined by the Sovereign in the DELEGATE record's capability bitmask
- Expiry: set by the Sovereign; renewable via a new DELEGATE record

### 3.2 Synchronization Between Devices

When the Sovereign operates both a personal device and an infrastructure node, record synchronization is their own concern. The protocol does not define a required synchronization method. Options in order of preference:

1. **Direct sync over LAN (preferred):** BitPads frames transmitted over IP when both devices are on the same network
2. **BLE relay:** Short-range sync via Bluetooth LE when in proximity
3. **pads-v1 URL exchange:** For low-bandwidth or high-latency links

Sync produces SEND + RECEIVE record pairs in each device's Exchange sub-entity, providing a ledger-auditable history of what was synced, when, and between which DIDs.

---

## 4. Hosted Newgroup Ledger Node

### 4.1 Full Specification

The Hosted Newgroup Ledger Node is the infrastructure extension of the Agreements and People services. It provides a canonical shared ledger for multi-party contexts. This section is the definitive specification; it extends the summary given in the Agreements Service Specification §4.

### 4.2 Node Architecture

A Hosted Newgroup Ledger Node operates:

- A `telux-ledgerd`-equivalent append-only SQLite ledger (same schema as the personal device ledger)
- A node signing keypair (distinct from any Sovereign's key; published in the node's DID document)
- An enrollment registry (mapping newgroup_ids to enrolled party DIDs)
- An inbound record queue (accepting submissions from enrolled parties)
- A distribution service (pushing new records to enrolled parties via configured channels)

### 4.3 Record Intake and Countersigning

When an enrolled party submits a record to the Ledger Node:

```
1. Node authenticates sender: verify source_did is an enrolled party for this newgroup_id
2. Node validates record: same validation path as telux-ledgerd (signature, JOURNAL invariant, etc.)
3. Node computes chain_hash: extends the newgroup's chain from the previous record
4. Node countersigns: node_sig = ed25519_sign(node_private_key, frame_hash || chain_hash)
5. Node stores the record with both sovereign_sig and node_sig
6. Node distributes the countersigned record to all other enrolled parties
7. Node emits LEDGER_ACK to submitter within MAX_COUNTERSIGN_LATENCY (default: 2 seconds)
```

The node_sig is an additional field in the stored record, not a replacement for sovereign_sig. A receiving party verifies both independently: the sovereign_sig proves the record came from the named Sovereign; the node_sig proves the node accepted and ordered it.

### 4.4 Ordering Guarantee

The Ledger Node provides total ordering over all submitted records for a given newgroup. Records are ordered by arrival order at the node, not by wall_ts (which is set by the submitting device and may drift). The lamport_ts assigned by the node is the authoritative sequence counter for the newgroup.

Wall_ts from the submitting device is preserved as a separate field (`submitter_wall_ts`). The node's own `wall_ts` at acceptance time is stored alongside it.

### 4.5 Enrollment

Party enrollment in a hosted newgroup is a three-step protocol:

```
Step 1 — Founding Sovereign creates the newgroup:
  CREATE record with newgroup_type="hosted" and node_did in note component
  Posted directly to the Ledger Node

Step 2 — Founding Sovereign invites a party:
  JOIN record with invitee_did
  Posted to Ledger Node; node notifies invitee

Step 3 — Invitee accepts:
  AGREE record signed by invitee
  Posted to Ledger Node; node registers invitee as enrolled
```

Enrollment is recorded on-ledger. A party that does not submit an AGREE record is not enrolled. An enrolled party that submits a LEAVE record is removed from the distribution list; their prior records remain in the ledger.

---

## 5. Content Node (CDN Model)

### 5.1 Content Storage

A Content Node stores ZAKO-compatible content assets referenced by Academy records. Content is addressed by the BLAKE3 hash of its bytes:

```
content_address = BLAKE3(content_bytes)
```

An Academy record that references content carries the content_address in its note component. A device that needs the content queries any Content Node that has it, using the content_address as the key. The response is the content bytes; the device verifies the hash before use. There is no trust relationship with the Content Node — the hash is the verification.

### 5.2 Content Distribution Contract

A Content Node that serves content to an enrolled device agrees to:
1. Serve content identified by content_address within MAX_CONTENT_LATENCY (default: 5 seconds for cached content; 30 seconds for origin fetch)
2. Verify content integrity before serving (compute BLAKE3 and compare)
3. Not modify content — delivery of modified content is detectable by the hash check and constitutes a protocol violation
4. Not log content requests in any form that associates a Sovereign's DID with the content they accessed (privacy requirement)

### 5.3 Content Prefetch

Content Nodes may be instructed by the Academy service to prefetch content for enrolled devices. Prefetch is requested by the Academy service daemon and executed at BACKGROUND process class. Content is delivered to the device and cached in the Academy content store before the Sovereign initiates a study session. This enables offline study of content that was fetched during a prior online window.

---

## 6. Service Endpoint Publication

### 6.1 Publishing Endpoints

An infrastructure node publishes its service endpoints in the ZAKO distribution's node registry. This registry is a signed document (ATTEST record from the distribution's root DID) listing all nodes operated by the distribution:

```
task_code    = 0x0E   ATTEST
source_did   = distribution_root_did
note         = JSON: {
  "nodes": [
    {
      "did": "did:key:z6Mk...",
      "type": "ledger",  [or "relay", "content"]
      "endpoint": "https://node.distribution.example",
      "scope": ["newgroup", "community_name"],
      "pubkey_fingerprint": "BLAKE3_of_public_key_hex"
    }
  ],
  "version": 1,
  "issued_at": 1748736000
}
```

### 6.2 Endpoint Discovery

A ZAKO device discovers infrastructure nodes through:
1. Distribution-bundled node list (shipped with the distribution image; updated via distribution update)
2. DID document service endpoints (if the Sovereign has a DID document with service endpoints published)
3. Direct peer introduction (a peer shares a node's DID via a SEND record)

ZAKO does not define a central node registry or DNS-based discovery. Discovery is sovereign: each device knows about nodes it has been explicitly told about.

---

## 7. Revenue Sharing and Distribution Records

### 7.1 Distribution Revenue Model

When a ZAKO distribution generates revenue through content delivery, credential issuance, or managed services, that revenue is recorded on-ledger using standard Exchange Engine records. The distribution operates as a ZAKO entity with its own DID.

**Content delivery fee:**
```
task_code    = 0x1C   PAY
account_pair = 0001   (Asset/Liability — Financial domain)
value        = fee_amount
source_did   = sovereign_did
dest_did     = distribution_entity_did
```

**Revenue share to content creator:**
```
task_code    = 0x1C   PAY
account_pair = 0000   (Source→Sink)
value        = creator_share_amount
source_did   = distribution_entity_did
dest_did     = content_creator_did
```

The conservation invariant applies across the full revenue distribution batch: total fees received must equal the sum of all outgoing payments (creator shares, infrastructure costs, etc.) plus retained balance.

---

## 8. Infrastructure Conformance Requirements

A ZAKO distribution claiming Infrastructure Extension Profile conformance must satisfy all baseline ZAKO Standard conformance requirements plus the following:

1. **Node identity is published and verifiable.** Every infrastructure node operated by the distribution has a published DID derivable from its public key. The public key's fingerprint appears in the distribution's signed node registry.

2. **Ledger Node countersigns within MAX_COUNTERSIGN_LATENCY.** The default is 2 seconds. If a node cannot meet this bound, it must emit a SUSPEND signal to submitting parties and queue submissions. It must not silently delay without notification.

3. **Append-only is enforced on infrastructure nodes.** The same append-only guarantees required of `telux-ledgerd` on personal devices apply to Ledger Nodes. No UPDATE, DELETE, or reordering of submitted records is permitted.

4. **Content Node does not log access associations.** A Content Node must not maintain logs that associate a Sovereign DID with the content they accessed. Access logs may be maintained in aggregate form (content_address + count, no DID) for capacity planning.

5. **Enrollment records are on-ledger.** All newgroup enrollment events (JOIN, AGREE, LEAVE) are stored in the hosted newgroup's ledger. There is no out-of-band enrollment mechanism.

6. **Revenue distribution satisfies conservation.** Revenue records posted by the distribution must balance: fees received equal fees distributed. A distribution that posts revenue records that do not balance has violated the conservation invariant.

7. **Multi-device sync is auditable.** When a Sovereign syncs between personal device and infrastructure node, sync events produce SEND + RECEIVE record pairs in both devices' Exchange sub-entities.

---

## Appendix: Infrastructure Role Summary

| Role | Ledger | Signs Records | Serves Content | Routes Messages |
|------|--------|--------------|----------------|-----------------|
| Personal Device | Yes (personal) | Yes (own key) | No | No |
| Relay Node | No | No | No | Yes |
| Ledger Node | Yes (newgroup) | Yes (countersigns) | No | Optional |
| Content Node | No | No | Yes | No |

---

*ZAKO Infrastructure Extension Profile v1.0 — June 1, 2026*  
*Extension of ZAKO Standard v1.x*  
*Cross-reference: Telux Protocol v1.0 §9, Agreements Service Specification v1.0 §4*
