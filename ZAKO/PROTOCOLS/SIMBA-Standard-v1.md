# SIMBA Standard
## Service Integration for Mobile Business Applications — Version 1.0

*June 1, 2026*

---

> A HOME device is a sovereign network. Every process running on it is a node in that network, governed by the same protocol, subject to the same power authority, contributing to the same ledger. SIMBA is the compliance standard that defines what an external service must become in order to be admitted as a node. It does not bring a foreign world inside HOME. It defines the terms under which a foreign service translates itself into HOME's world to earn entry.

---

## 1. Purpose and Scope

SIMBA — Service Integration for Mobile Business Applications — is the standard governing how external service providers integrate into a HOME distribution. It defines the compliance requirements a service must satisfy to be admitted as a participating node on the HOME device network, and the terms under which that participation is maintained or revoked.

SIMBA applies to any external application, service, or provider that wishes to:
- Present functionality within the HOME device environment
- Access data held in any HOME Island or sub-entity
- Produce records that enter the sovereign ledger
- Mediate exchanges between the Sovereign and external parties

A service that does not seek HOME integration is not subject to SIMBA. A service that does seek integration is fully subject to it. There is no partial compliance path.

**Out of scope in this document:**
- Visual design system, UI components, and CSS conventions — these are defined in the HOME Design System Standard (forthcoming)
- Transport encoding and carrier-level integration — defined in HOME Transmission Adapters
- Distribution-specific approval processes for service admission

---

## 2. The Integration Principle

Inside HOME there is one process model, one record format, one power governor, and one design language. An integrating service adopts all of them. What the service contributes is function and content. What governs how it runs, what it can access, what it records, and how it presents — that is HOME's domain entirely.

This is not a constraint on what a service can do. It is a constraint on what kind of entity a service can be inside HOME. Outside HOME, the service may operate however it chooses. Inside HOME, it operates as a SIMBA-compliant node.

The result for the Sovereign is coherence: a HOME device does not feel like a collection of foreign applications sharing a screen. It feels like one system with depth. SIMBA is what produces that coherence at the integration boundary.

---

## 3. The SIMBA Node Model

An external service admitted under SIMBA becomes a **SIMBA Node** — a named, registered participant on the HOME device network with:

- Its own registered identity (a DID derived from the service provider's signing key)
- A declared process class governing its execution under Outstack
- A capability grant from the Sovereign defining what it may access
- A record model defining what kinds of TELUX records it produces
- A manifest on file with the HOME distribution declaring all of the above

A SIMBA Node is not a guest. It is not sandboxed outside the HOME protocol. It is a compliant participant governed by exactly the same rules as native HOME daemons — because from the HOME device network's perspective, there is no meaningful distinction between a native daemon and a SIMBA-compliant external service. Both are nodes. Both are governed.

---

## 4. The Service Manifest

Every SIMBA-compliant service must publish and maintain a Service Manifest — a signed document that declares the service's identity, process class, capability requirements, and record model. The manifest is signed by the service provider's DID and submitted to the HOME distribution for admission review.

The manifest is not a configuration file. It is a declaration of intent and a compliance commitment. Deviation from the manifest after admission is a compliance violation and grounds for revocation.

### 4.1 Required Manifest Fields

**Identity:**
```
service_did:       did:key:<provider_public_key>
service_name:      <human-readable name; max 64 bytes>
provider_name:     <legal or trading name of provider>
manifest_version:  <semver>
simba_version:     1.0
```

**Process class declaration:**
```
process_class:     CRITICAL | INTERACTIVE | BACKGROUND | DEFERRED | OPPORTUNISTIC
process_class_rationale: <why this class is required>
suspension_safe:   true | false
max_deferral_hours: <integer; required if DEFERRED>
```

The service declares the highest process class it will ever require. It may operate at a lower class when conditions permit, but it may not exceed its declared class. `suspension_safe: true` means the service has a clean suspension point and will honour Outstack suspension signals within the standard window (2 seconds for BACKGROUND; see Outstack Protocol §8.3). `suspension_safe: false` is only permissible for CRITICAL class services, and requires explicit distribution approval.

**Capability requirements:**
```
capabilities_requested:
  - scope: read_island
    island: health | academy | exchange | pads | people | places | agreements
    rationale: <why required>
  - scope: write_exchange
    rationale: <why required>
  - scope: read_work_island
    rationale: <why required>
```

Capability requests are the minimum required for the service to function. The Sovereign grants or denies each scope individually at installation time. A service must function in a degraded but non-crashing state if a requested capability is denied. A service that requires a specific capability to function at all must declare it as `required: true`; the Sovereign is informed that denying it means the service will not be installed.

**Record model:**
```
record_types_produced:
  - task_code: 0x20     # ASSIGN
    domain: 10          # Hybrid
    description: "Work assignment records when tasks are created via this service"
  - task_code: 0x1B     # INVOICE
    domain: 00          # Financial
    description: "Invoice records generated from service billing events"
```

The record model declares every type of TELUX record the service will write to `telux-ledgerd`. The service may not produce record types not listed in its manifest. `telux-ledgerd` enforces this: records submitted by a SIMBA Node with a task_code not declared in the node's manifest are rejected with a LEDGER_REJECT signal.

**Network behaviour declaration:**
```
outbound_connections:
  - endpoint: api.provider.example
    purpose: content delivery
    data_sent: content requests (no Sovereign PII)
    data_received: content bytes
    records_produced: [RECEIVE, LEARN]
    offline_capable: true  # service functions without this connection
```

Every external network call the service makes must be declared. Each declared connection must map to one or more record types the service will produce when the connection is made. Undeclared outbound connections are prohibited and are treated as a compliance violation if detected.

---

## 5. Process Class Compliance

A SIMBA Node is governed by Outstack identically to any native HOME daemon. When Outstack changes power mode:

- The node receives the MODE_ENTER signal
- The node honours the process class gate for its declared class
- If the node's class is gated, it reaches its suspension point and confirms suspension within the standard window
- When the gate lifts, the node resumes from the suspension point

A node that does not respond to suspension signals within the required window is treated by Outstack as an unresponsive process. Outstack escalates: first a SUSPEND record is posted, then if unresolved, the node's capability grants are temporarily revoked by `telux-identd` until the node confirms suspension.

A SIMBA Node may not spawn sub-processes at a higher class than its own declared class. If a service declares BACKGROUND, it cannot initiate INTERACTIVE sub-operations. It must queue those operations and process them when the Sovereign initiates an INTERACTIVE session.

---

## 6. Record Sovereignty

### 6.1 The Ledger Is HOME's

Any interaction the service mediates that constitutes a meaningful event — a transaction, an exchange, a task, a content access, a measurement, a message — must produce a TELUX record written to `telux-ledgerd`. The service does not maintain a parallel data store for sovereign data. The record is the data. HOME's ledger is where it lives.

This is not optional and is not negotiable. A service that silently stores sovereign interaction data on its own servers without producing a corresponding ledger record is not SIMBA-compliant regardless of what else it does correctly.

### 6.2 The Service Signs With Its Own DID

Records produced by a SIMBA Node carry `source_did = service_did`. They are signed by the service provider's key. They are chain-hashed by `telux-ledgerd` into the relevant Island's chain. The Sovereign's own sovereign_sig is applied to records that are the Sovereign's own actions; service-produced records carry the service's signature.

The Sovereign can at any time inspect any record in their ledger and see exactly which service produced it. Provenance is not hidden.

### 6.3 RESTRICT_FORWARD Applies

A service may not export or transmit records with `restrict_fwd=1`. `telux-sharedb` enforces this: the service cannot bypass the RESTRICT_FORWARD check regardless of its declared capabilities. Journal records (sub_entity=3) are inaccessible to SIMBA Nodes by SELinux policy — no capability grant can override this.

### 6.4 No Silent Network Calls

Every outbound network call a service makes maps to a record in the ledger. If the service calls its API to fetch content, a RECEIVE record is produced. If the service sends data to its servers, a SEND record is produced. The Sovereign's ledger reflects all data movement the service performs on their behalf. A service that makes network calls not reflected in any ledger record is in violation.

---

## 7. Capability Grants

At installation, the Sovereign is presented with the service's capability requests from the manifest. The Sovereign grants or denies each scope individually. The grant is written as a standard GRANT record (task_code=0x08) in the Identity sub-entity, with `dest_did = service_did` and the approved capability bitmask.

The service receives its grants and operates within them. It does not request additional capabilities at runtime. If a capability is later revoked by the Sovereign, the revocation is immediate. The service must degrade gracefully — it must not crash, must not loop on retry, and must notify the Sovereign clearly that a required capability has been revoked and certain functions are unavailable.

A SIMBA Node may not request capability grants from any source other than the Sovereign's own `telux-identd`. A service that attempts to acquire capabilities through any other path — social engineering, manifest misrepresentation, or API exploitation — is permanently banned from the distribution.

---

## 8. The UI Boundary

Inside HOME, there is one design language. A SIMBA-compliant service presents its functionality within that design language. It does not import its own brand chrome, typefaces, colour systems, or UI component libraries. It contributes function. HOME contributes the frame.

The specific requirements of the HOME design language — tokens, components, layout constraints, motion behaviour, accessibility requirements — are defined in the HOME Design System Standard (forthcoming). This document establishes the principle: the UI boundary is HOME's to define, and SIMBA compliance requires operating within it.

The permitted exception is narrow and explicitly case-by-case: a service may, with distribution approval, present a small brand mark (logo) in a specifically defined context. This exception does not extend to colour systems, typefaces, or component styling. The exception is granted by the distribution and documented in the service's admission record.

---

## 9. The SIMBA Lifecycle

### 9.1 Admission

A service seeking admission submits its manifest to the HOME distribution. The distribution reviews for:
- Manifest completeness and accuracy
- Process class appropriateness (a content delivery service claiming CRITICAL will be rejected)
- Record model validity (declared task_codes must exist in the HOME Codebook Standard)
- Network behaviour plausibility (declared endpoints must be consistent with stated purpose)
- UI compliance commitment

On admission, the distribution signs an ATTEST record (task_code=0x0E) from the distribution's root DID asserting that the service has been reviewed and admitted at the stated manifest version. This record is published in the distribution's service registry.

### 9.2 Installation

When a Sovereign installs a SIMBA service, the following sequence occurs:

```
1. Service manifest is retrieved and verified against the distribution's ATTEST record
2. Sovereign is presented with capability requests; grants or denies each
3. GRANT records are written to the Identity sub-entity for approved scopes
4. Service DID is registered with telux-identd as a SIMBA Node
5. Outstack receives the service's ASSIGN record declaring its process class
6. Service is admitted and may begin operating
```

The entire installation sequence produces ledger records. The Sovereign has a complete on-device record of what was granted, when, and to whom.

### 9.3 Suspension and Resumption

SIMBA Nodes are suspended and resumed by Outstack using the standard SUSPEND and RESUME record mechanism (Outstack Protocol §5). The service does not control its own suspension — Outstack does. The service must honour it.

### 9.4 Revocation

The Sovereign may revoke a SIMBA Node's access at any time by revoking its capability grants via `telux-identd`. Revocation is immediate. The node loses access to all granted Island reads and writes. Its process class registration remains until the node is uninstalled, but it can produce no meaningful records without capabilities.

The distribution may also revoke a service's admission — posting a REVOKE against the admission ATTEST record in the service registry. A revoked service's manifest is no longer valid for installation on any device running that distribution.

---

## 10. What SIMBA Is Not

**SIMBA is not an app store model.** Admission under SIMBA does not grant the service permission to market itself within HOME. The distribution controls what services are surfaced to Sovereigns and how.

**SIMBA is not a revenue guarantee.** Admission means compliance, not commercial terms. Revenue arrangements between service providers and the distribution are commercial agreements outside this standard.

**SIMBA is not a sandbox.** A SIMBA Node is not isolated from the HOME device network inside a security sandbox. It is a participant in it — with a declared identity, declared capabilities, and the full weight of TELUX protocol governance on its behaviour. Its constraint is compliance, not containment.

**SIMBA is not optional for HOME integration.** There is no pathway to integration inside a HOME distribution that bypasses SIMBA compliance. A service that wishes to be present on HOME device must meet this standard fully.

---

## 11. Conformance Requirements

A SIMBA-compliant service satisfies all of the following:

1. **Manifest is complete, signed, and admitted.** A service without a distribution-attested manifest is not SIMBA-compliant regardless of its technical behaviour.

2. **Process class is declared and honoured.** The service declares one process class and never exceeds it. It honours suspension signals within the standard window.

3. **All sovereign data enters the ledger.** Every meaningful interaction produces a TELUX record in `telux-ledgerd`. No parallel sovereign data store is maintained by the service.

4. **All network calls are declared and recorded.** No outbound connection is made that is not in the manifest. Every connection made produces a corresponding ledger record.

5. **Capabilities are not exceeded.** The service operates within the capability grants the Sovereign has issued. It degrades gracefully on revocation.

6. **RESTRICT_FORWARD is never bypassed.** The service does not attempt to transmit, export, or relay records marked RESTRICT_FORWARD=1.

7. **UI renders within the HOME design language.** The service presents no independent brand chrome, typeface, or component system. The HOME Design System Standard governs presentation.

8. **Records match the declared record model.** `telux-ledgerd` enforces this at write time. A service that produces undeclared record types has its records rejected.

---

## Appendix: SIMBA Node Identity in the Ledger

When a SIMBA Node produces a record, it appears in `telux-ledgerd` as:

```
source_did    = service_did         (the service provider's DID)
sovereign_sig = service_signature   (signed by the service's key)
frame_hash    = BLAKE3(frame_bytes) (computed by telux-ledgerd as normal)
chain_hash    = BLAKE3(frame_hash || prev_chain_hash)
```

The Sovereign can inspect any record, see its `source_did`, and know precisely which service produced it. The record is chain-hashed into the relevant Island's ledger exactly like any other record. There is no second-class record type for service-produced data — it is ledger data, with all the immutability and provenance guarantees that entails.

---

*SIMBA Standard v1.0 — June 1, 2026*  
*Companion standard to HOME Standard v1.x*  
*Cross-reference: Telux Protocol v1.0 §7, Outstack Protocol v1.0 §4–§5, HOME Design System Standard (forthcoming)*
