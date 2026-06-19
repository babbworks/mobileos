# Potential for Telux
## A Visionary Technical Brief

**Date:** 2026-05-31  
**Status:** Exploratory / Vision  
**Thread:** Telux + Outstack synthesis

---

## Premise

No operating system in existence today unifies group identity, power management, and transactional security into a single coherent model. Telux is a proposal to do exactly that — and the research confirms it occupies genuine uncharted territory.

This document synthesizes everything known about Telux and Outstack, fuses it with current technical context, and argues the case for Telux as a historically significant system built around one foundational thesis:

> **Power is the ultimate security boundary. A system that controls electricity controls everything.**

---

## Part I: The Landscape — What Exists and What Doesn't

### 1.1 The Three Traditions Telux Inherits From

**Plan 9 from Bell Labs (1987–)**

Plan 9 was the most architecturally coherent OS ever built from the Unix tradition. Its core innovation was the **per-process namespace**: every process sees its own view of the filesystem, constructed from mount tables rather than shared global state. This design made namespace isolation fundamental — not retrofitted — which is why Docker, Kubernetes, and Linux's own namespace features are descendants of Plan 9's insight, filtered through decades of compromise.

Plan 9 never succeeded commercially, but it proved that an OS could be designed from the ground up around the principle that every principal (process) inhabits a constructed reality rather than a shared one. Telux inherits this lineage: every Island is a constructed namespace, every group inhabits it on sovereign terms.

**seL4 Microkernel (2009–)**

seL4 is the first formally mathematically verified microkernel. Its model: every operation in the system requires a cryptographic **capability** token to proceed — an unforgeable credential owned by the calling thread. There are no ambient authorities. You cannot access memory you don't have a capability for. You cannot send a message to a process you don't have a capability for. You cannot schedule a thread without a capability.

seL4 proved that a minimal kernel (~10KB of code) could provide stronger, formally verified security than any amount of runtime checking in a large system. The TCB (trusted computing base) is small enough to be human-auditable.

Telux inherits the capability model: the Sovereign mechanism is a capability system for group membership, power budget delegation, and exchange authorization.

**COBOL Transaction Monitors (CICS, IMS) — 1960s–present**

The global banking system runs on CICS — IBM's Customer Information Control System. CICS is not primarily a programming language; it is a **transaction orchestration environment** that ensures every operation is atomic, logged, and recoverable. It handles millions of transactions per second, has been running since the 1960s, and is not going away.

Its relevance to Telux: CICS proved that a system built around the concept of a **transaction** — a structured, recorded, reversible (or at minimum auditable) unit of exchange — can be the bedrock of civilization-scale infrastructure. Telux aspires to the same role, but for the digital-physical exchange layer of an interplanetary civilization.

---

### 1.2 The Gap That Telux Fills

A 2025 survey of current OS and identity systems reveals:

| System | Group Identity | Power Management | Transactional Ledger | Non-Human Entities |
|--------|---------------|-----------------|---------------------|-------------------|
| Linux groups | Static, UID-based | Separate (cgroups v2) | None | None |
| Kubernetes | Namespaces (not groups) | Resource quotas | None | None |
| Google Cloud IAM | Hierarchical, orgs/groups | None | None | Service accounts |
| Microsoft Entra ID | Deep groups + PIM | None | None | Managed identities |
| seL4 | Capability tokens | None | None | Abstract subjects |
| Plan 9 | Per-process namespaces | None | None | None |
| W3C DIDs | Any entity, decentralized | None | None | **Yes — full spec** |

**No system combines all four.** Telux is not incremental — it proposes an OS-level synthesis where group identity, power management, transactional recording, and non-human entity inclusion are **one unified primitive**, not four separate subsystems bolted together.

---

### 1.3 The Power-Security Gap is Real and Unexploited

Recent security research (2024-2025) has confirmed what Outstack's designers intuited: **clock and power gating mechanisms are attack surfaces**. Adversaries who can manipulate which subsystems receive power can selectively disable security controls. The security community is responding by hardening Hardware Roots of Trust — but defensively, reactively.

No OS project has turned this around: using power control **offensively as a security primitive**. The insight is profound:

> Software isolation can be bypassed by software. Hardware power isolation cannot be bypassed by software. A peripheral with no power has no attack surface — not because of a policy, not because of a privilege level, but because of physics.

Telux, through Outstack, makes power gating a first-class security operation. Outstack can physically kill a compromised subsystem. This is qualitatively different from any software firewall or mandatory access control system.

---

## Part II: Outstack as Telux's Power Core

### 2.1 The Name and Its Meaning

Outstack is named for the northernmost exposed rock of the United Kingdom: permanently uninhabited, geologically stable, essential to the maritime definition of where Britain ends. It is the bedrock beneath everything. It has no residents. It is not optional.

This is exactly what Outstack does for Telux: it is the uninhabited, always-running bedrock that enforces the rules. It has no groups on it. It is not optional.

### 2.2 Outstack's Three-Layer Role

In the Telux architecture, Outstack is not a single daemon — it is a **vertical stack** running from hardware to user interface:

```
┌─────────────────────────────────────────────────────────────┐
│  VISIBLE LAYER                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐ │
│  │ Group Chat   │  │ NL Query API │  │ Exchange Records  │ │
│  │ Interface    │  │ (SQL + prose)│  │ (member-visible)  │ │
│  └──────────────┘  └──────────────┘  └───────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│  SUBMERGED LAYER                                            │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐ │
│  │ Outstack     │  │ Sovereignty  │  │ Permissioned Logs │ │
│  │ Daemon       │  │ Enforcer     │  │ + Power Governor  │ │
│  └──────────────┘  └──────────────┘  └───────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│  BEDROCK LAYER                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐ │
│  │ LSM Module   │  │ Immutable    │  │ TPM/HSM Key Store │ │
│  │ (Telux-SEC)  │  │ Audit Trail  │  │ + Power Gates     │ │
│  └──────────────┘  └──────────────┘  └───────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │ Linux Kernel     │
                    │ + cgroups v2     │
                    │ + power domains  │
                    │ + custom LSM     │
                    └──────────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │ Hardware         │
                    │ PMICs, voltage   │
                    │ regulators,      │
                    │ clock gates,     │
                    │ power rails      │
                    └──────────────────┘
```

### 2.3 Power Management as the Primary Differentiator

Outstack's power management is not a feature of Telux — it is Telux's operating premise. Consider what this enables:

**Power budgets per Island:** An Island is not just a security namespace — it has an electricity budget. Exceeding that budget is a security event, not just a resource event. An Island consuming unexpectedly more power may have been compromised. Outstack detects this at the hardware sensor level before any software alarm fires.

**Power as access control:** When a sovereign revokes a group's access, they don't just set a permission bit. They can power-gate the hardware resources that group was using. This is physically unforgeable revocation.

**Power states as system modes:** Outstack's five-mode model (FULL → NORMAL → CONSERVE → CRITICAL → EMERGENCY) applies to Islands, not just the whole device. An Island running a high-priority AI model in a field device under battery pressure can be granted INTERACTIVE power class. An Island running background sync is DEFERRED. The scheduler knows.

**Execution gating:** At `exec()` time, Outstack checks whether the power budget of the calling Island permits the new process to start at all. In EMERGENCY mode, only CRITICAL-class processes execute. This is not a firewall rule — it is a scheduling primitive that operates at the most fundamental OS boundary.

**The aerospace heritage:** Outstack's power model was inspired by RTG-powered spacecraft, where every milliwatt must be accounted across the mission lifetime. The same model — five source types (RTG, battery, solar, fuel cell, external), battery health tracking with degradation and cycle count, predictive runtime calculation — applies across terrestrial professional hardware and, eventually, to actual spacecraft running Telux nodes.

---

## Part III: The newgroup Primitive and Identity Architecture

### 3.1 Why Linux Groups Are Insufficient

Linux's native group model was designed for a world of human users sharing a filesystem. It is:
- **Static** — group membership requires root and a file edit
- **Local** — no concept of remote members
- **Human** — UIDs and GIDs have no identity model for AI agents, APIs, or IoT devices
- **Non-sovereign** — there is no authority hierarchy; root can modify any group
- **Unpowered** — groups have no connection to power budgets or hardware resources

The gap between Linux groups and Telux's **newgroup** is not evolutionary — it is categorical.

### 3.2 The newgroup Model

A Telux newgroup is a **dynamic, sovereign-governed coalition** of any mix of:

- **Human members** — authenticated via hardware keys (TPM, FIDO2, or smartcard)
- **AI entities** — identified by DID (W3C Decentralized Identifier), with capability tokens scoping what they can ask for and receive
- **System services** — local daemons with service identities, power class assignments, and scoped filesystem capabilities
- **Commercial APIs** — remote endpoints represented by broker processes with constrained outbound capability
- **IoT / physical devices** — hardware-identified nodes with power-domain-aware capabilities

The W3C DID standard (v1.1 in progress as of 2026) provides exactly the right identity primitive for non-human members: self-administered, cryptographically verifiable, decentralized, persistent. Telux can adopt DIDs as the native identity format for all non-human group members, giving it interoperability with the emerging global identity ecosystem at no cost.

### 3.3 Islands as Sovereign Containers

An Island is the unit of sovereignty. It is:

1. A **power domain** — Outstack allocates hardware power budget to it
2. A **security namespace** — the LSM (Telux-SEC) enforces its boundaries
3. A **communication space** — its members share a group message bus
4. A **ledger** — every exchange within it is recorded

The Sovereign of an Island declares:
- Who may enter and how (invitation, brokerage, self-sovereign join)
- Who may grant membership to others (sub-sovereignty)
- What power budget the Island operates within
- What the logging policy is (which tier receives which events)
- What happens when the Sovereign is unreachable (succession rules)

Islands can nest. A corporate Island may contain departmental sub-Islands. A field mission Island may contain a human team Island and a hardware Island. Sovereignty flows down but does not automatically reverse — a sub-Island sovereign cannot override the parent Island's bedrock rules.

---

## Part IV: The Exchange Layer — Telux as Infrastructure

### 4.1 The COBOL Parallel is Not a Metaphor

COBOL transaction monitors (CICS, IMS) have handled global banking since the 1960s because they enforce one rule above all others: **every operation is recorded before it is executed**. The write-ahead log is the bedrock of financial trust.

Telux's exchange layer is this principle, generalized:

- Every resource transfer between entities in a group is recorded
- The record is created before the transfer completes (or atomically with it)
- The record is hashed, signed, and distributed to the appropriate visibility tier
- The record is idempotent — resending it causes no harm
- The record is low-bit — designed for transmission across degraded or high-latency links

The shorthand accounting notation (not yet fully documented in these files) will be the record format. Whatever it is, it must satisfy the constraints of ultra-low-bit encoding while being human-readable in natural language via the group chat layer.

### 4.2 Natural Language as the Query Interface

One of Telux's most distinctive design choices: **the primary user interface for querying the ledger is natural language**.

A human member of a commercial Island asks: *"What did the inventory service send to the billing service last Tuesday, and was payment acknowledged?"*

This query is routed through the group's AI member (or a built-in query engine), which translates it into a ledger query scoped to the requesting member's identity. The response contains only records the member is authorized to see.

This is not an AI wrapper bolted onto a ledger. It is the ledger's designed query interface. The scoping by identity is enforced by the Sovereignty model, not by the AI's judgment.

### 4.3 Broker Architecture for External Parties

Telux supports a **broker pattern** for transient participation:

A corporate Island can have a broker member whose role is to:
1. Represent an external party (customer, vendor, regulator) temporarily within the Island
2. Receive records on behalf of that external party
3. Forward signed copies of records to the external party's own system
4. Expire and remove the external representation after the transaction concludes

This is exactly how correspondent banking works — a domestic bank acts as a broker for international counterparties. Telux makes this pattern a native OS primitive.

---

## Part V: Power Management as the Outstack Service Differentiator

### 5.1 Why Power Makes Outstack Different From Everything Else

Every existing approach to OS-level security focuses on software boundaries: access control lists, mandatory access controls, capability tokens, encrypted memory, formal verification. These are all essential. But they share a common vulnerability: a sufficiently privileged software actor can, in principle, bypass them.

Outstack adds a layer beneath software: **physical power control**. A hardware peripheral that is power-gated is not merely denied access — it does not exist as a computing entity. No software privilege escalation, no kernel exploit, no hypervisor escape changes this fact.

This creates a new class of security guarantee:

> **Outstack-isolated resources are secure by physics, not by policy.**

### 5.2 The Five Differentiating Capabilities

**1. Power-Anomaly Security Alerts**  
Unexpected power draw from a subsystem triggers a security event, not a power event. If a network interface suddenly draws 3× its budgeted power, it may be exfiltrating data. Outstack detects this before any network monitoring software could.

**2. Hardware Power-Kill for Incident Response**  
When Outstack detects a compromise (or a sovereign invokes emergency isolation), affected hardware can be physically power-gated. This is irreversible from within the compromised subsystem — you cannot software-reconnect a hardware power gate from a process running on the disconnected hardware.

**3. Power State in Attestation Reports**  
When a Telux node performs remote attestation (proving its integrity to a remote party), the power state of all hardware domains is included in the attestation. A remote party can verify not just that the software is unmodified, but that all hardware domains are in their expected power states.

**4. Per-Island Power Budgets as Resource Governance**  
Islands receive electricity budgets, not just CPU time and memory. An AI workload Island cannot monopolize the system by consuming hardware accelerator power beyond its allocation, even if it has somehow escaped its CPU cgroup.

**5. BABB-Style External Power Source Integration**  
Physical security devices (like BABB, the USB-C security dongle in the project's broader context) can be detected by Outstack as both an authentication event and a power topology change. The arrival of a trusted hardware device can: unlock encrypted storage, elevate the user's power class to INTERACTIVE, and enable OPPORTUNISTIC background tasks — all simultaneously, because all three are Outstack-governed events.

---

## Part VI: The Outstack Service Model — Primary but Not Only Differentiator

Outstack is the primary differentiator of Telux, but Telux has several secondary differentiating properties that make it compelling independent of any single feature:

### 6.1 Interoperability Without Centralization

Unlike corporate identity platforms (Google IAM, Entra ID), Telux has no central authority. The W3C DID standard, adopted natively, means:
- Any entity can create its own identity
- No vendor can revoke identities they didn't create
- The system operates without an internet connection (DIDs can be resolved locally or via distributed methods)
- This is critical for deep-space operation, remote field deployment, or adversarial environments where central servers are unavailable or compromised

### 6.2 Ultra-Low-Bit Resilient Records

The record format is designed for **hostile transmission environments**: compressed, forward-error-corrected (Reed-Solomon or LDPC), store-and-forward, idempotent, hash-chained. A record created in a Telux node in the Atacama Desert can survive transmission over degraded radio links and arrive intact. The same record, eventually, could survive an 8.5-light-minute transmission delay from Mars.

The design is conservative: assume latency, assume packet loss, assume adversarial interception. Records that arrive are verifiable. Records that don't arrive are retransmitted. The ledger eventually converges.

### 6.3 The Group Chat UX as Transparency Tool

The group chat interface is not a consumer feature bolted on — it is the transparency mechanism of the sovereignty model. When a group member asks "what happened?" the answer is the exchange record, rendered in natural language. The chat IS the audit trail, made legible.

This collapses the traditional gap between system logs (for admins) and user experience (for operators). In Telux, the operator IS the admin of their Island, and their chat IS the log.

---

## Part VII: Deployment Vision

### 7.1 Near-Term: Industrial Field Deployment

**Target hardware:** A Telux node on an STM32H7 or ARM Cortex-A class device, running an Alpine-derived base with the Outstack power daemon, custom LSM, and newgroup machinery.

**Scenario:** A four-person inspection team with two AI members (one voice assistant, one anomaly detector), two sensor services, and a real-time SCADA gateway form an Island. The Sovereign is the team lead's hardware key. Power budget: 3W total. The Island records every sensor handoff, every AI inference request, every SCADA command. The field team can ask the Island "what did the anomaly detector flag in the last hour?" in natural language and receive scoped, signed records.

When the team returns to base, the Island syncs its ledger to the corporate system via a broker. The external corporate system receives signed copies of all records relevant to it, without ever having direct access to the Island's bedrock layer.

### 7.2 Medium-Term: Server-Hosted Group Exchange Infrastructure

**Target:** Telux servers providing Islands as a service — the "group exchange as infrastructure" model. Organizations create Islands for projects, transactions, workflows. AI entities are first-class members. Physical IoT devices authenticate via hardware identity and join Islands.

This is Telux's COBOL moment: not a consumer product, but foundational infrastructure that other systems depend on. The natural language query layer makes it accessible. The power-managed bedrock makes it trustworthy.

### 7.3 Long-Term: Interplanetary / Interstellar Node

The Sumerian logo is not aesthetic ambition — it is a design constraint. Telux records must be readable in 1,000 years. Telux nodes must operate without Earth infrastructure. Telux ledgers must survive transmission across distances where light-speed lag is measured in minutes, hours, or years.

The record format (low-bit, forward-error-corrected, idempotent, hash-chained) already satisfies this requirement. The identity model (DID, no central authority) already satisfies this requirement. The power model (RTG support, autonomous mode, no human intervention required) already satisfies this requirement.

What Telux is engineering, in its terrestrial deployments, is also the protocol for economic and organizational coordination among dispersed human settlements beyond Earth. The fact that it works on a handheld industrial tool today is a proof of concept for something much larger.

---

## Part VIII: What Telux Is Not

**Telux is not a blockchain.** Records are not globally replicated. Consensus is scoped to Islands. There is no proof-of-work, no token speculation, no public ledger. Telux borrows the cryptographic integrity of blockchain thinking (hash chains, signed records) without the architectural overhead.

**Telux is not a chat application.** The group chat interface is the transparency layer of a security and exchange infrastructure. The conversations are audit trails. The queries are ledger operations.

**Telux is not Android or iOS.** It is not a consumer OS. It targets professional operators, industrial deployments, field teams, and eventually autonomous systems. Accessibility and aesthetics are secondary to sovereignty, integrity, and efficiency.

**Telux is not a cloud service.** It operates without cloud infrastructure. The cloud can be a member of an Island, via broker, just like any other external entity. But Telux nodes function in isolation.

---

## Part IX: Open Questions Remaining

From the existing documents and analysis, the following architectural questions remain unresolved and are the most important to address next:

1. **The accounting notation.** The shorthand record format for commercial exchange has been mentioned but not documented. This is load-bearing — the entire exchange layer depends on it. Documenting it is the single most valuable next step.

2. **Identity model for AI entities.** How is an AI model instance identified? Is it the model weights? The inference endpoint? The model + configuration? This determines whether AI members are persistent (same identity across sessions) or ephemeral.

3. **Sovereignty succession.** If a sovereign entity becomes unreachable, what governs the Island? Multi-sovereign quorum? Time-limited deputy sovereignty? Frozen state? This is the failure mode that determines whether Telux can operate in genuinely hostile or degraded environments.

4. **Cross-Island communication protocol.** How do members of different Islands exchange records? Via brokers only? Via a federated protocol? What prevents cross-Island leakage?

5. **The bedrock access model.** Who or what can read the bedrock audit trail? Hardware only (TPM-sealed)? A designated root sovereign? This determines what "full investigation" of a security incident looks like.

6. **Bootstrap and key ceremony.** How is the first Island created on a new Telux deployment? What is the key ceremony for establishing the root of trust? This is the security-critical moment that everything else depends on.

---

## Summary Assessment

Telux occupies genuinely uncharted architectural territory. The research confirms:
- No OS merges group identity with power management as a unified primitive
- No OS treats hardware power gating as a security primitive (only as an attack surface)
- No OS provides built-in transactional accounting with natural language query
- No OS natively includes non-human entities (AI, IoT) in its identity model at the OS level
- W3C DIDs provide a ready-made, production-quality identity primitive for non-human members

Outstack is not just the power management component of Telux — it is the reason Telux's security model is physically enforceable rather than merely policy-enforced. That distinction is the architectural foundation everything else rests on.

The Sumerian token was the first technology that let humans make binding commitments to strangers. Telux is an attempt to build the next one: a substrate for binding commitments between any entity — human, machine, AI, or yet unimagined — across any distance, with physical enforcement and millennia-scale durability.

That is what Telux could be.

---

*Document Version: 1.0 — Initial synthesis*  
*Based on: Early Stab at Telux System.md, Outstack System.md, Outstack Jan 23 aft 1.md, Overview of Project Options.md, embedded_field_device_architecture.md, state_machine_examples.md, and external research on Plan 9, seL4, W3C DIDs, and power-gating security (2024-2026)*
