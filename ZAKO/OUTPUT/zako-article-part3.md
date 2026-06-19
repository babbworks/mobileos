# ZAKO Part III: The Context

*De-Googling as founding decision. Zambia as design center. The distribution model. The performance contract. The open questions. And the claim.*

---

## De-Googling — The Founding Decision

### What GMS Is

Google Mobile Services. The Play Store, Play Services, Google Maps, Firebase Cloud Messaging, Google Account. Dozens of background processes that constitute Google's ongoing relationship with the device.

Persistent connections. The device checks in, reports, receives instructions. Push notifications route through Google's servers. Application signatures verify against Google's keys. Location pings at intervals determined on another continent. Battery drain from maintaining a relationship the user never explicitly agreed to maintain.

This isn't a conspiracy. It's architecture. GMS is the infrastructure layer through which Google delivers its services, and those services are the reason most people buy Android phones. For most people, it works.

For ZAKO's contexts, it doesn't.

---

### What GMS Removal Means

Zero GMS packages. Zero GMS network traffic — verified by packet capture, not assumed.

The measurable impact:
- 60–120mW of persistent background power draw eliminated
- All data transmission to Google endpoints eliminated
- Infrastructure dependency on Google's servers eliminated
- The device's relationships are only the ones its Sovereign has explicitly authorized

This isn't an optimization. It's the founding decision. Everything else in ZAKO follows from this: if you remove the infrastructure layer that modern Android depends on, what do you replace it with? The answer is Telux.

---

### What ZAKO Replaces

Every replacement follows one rule: it must be more sovereign, not merely different.

**Push notifications.** Firebase Cloud Messaging → UnifiedPush + ntfy. An open protocol. Provider of the user's choice. Self-hostable. The notification path belongs to the Sovereign's Island, not to Google's servers.

**App distribution.** Play Store → F-Droid. Installed as a privileged system application for silent updates. Every application in the catalog is built from auditable source code. No user data transmitted to a central commercial party.

**Location services.** Google's Fused Location Provider → AOSP standard GPS + network location. Maps via OpenStreetMap (Organic Maps — offline-capable, no tracking).

**Authentication.** Google Account → Telux DID layer. W3C Decentralized Identifiers backed by hardware keys. Applications receive capability tokens from the Island's identity service rather than from Google's OAuth endpoints.

**Maps.** Google Maps → Organic Maps (OpenStreetMap data, offline-first, no telemetry).

**Device attestation.** SafetyNet/Play Integrity → hardware security module direct attestation. A narrower claim — it attests the key's hardware provenance, not Google's verification of the boot chain — but it's honest about what it can verify.

F-Droid is not chosen because it's "better than" the Play Store on features. It's chosen because its distribution relationship does not require user data to be transmitted to a central commercial party. Organic Maps is not chosen because it's "better than" Google Maps. It's chosen because it works offline and tells no one where you are.

---

## Zambia — The Design Center

### Why Zambia

Not a test market. Not an afterthought. The design center.

Mobile phones are the primary computing device for most Zambians. Mobile money — Airtel Money, Zamtel Kwacha, MTN MoMo — is the primary financial infrastructure, more accessible than banking for most of the population. No existing Android OS was built with this context as its starting condition.

And the constraints sharpen everything: electricity access is irregular outside urban centers, making battery life a genuine daily concern. Mobile data is expensive relative to income, making background data consumption a real cost. Banking infrastructure is limited, making USSD-based mobile money platforms not a feature but a necessity.

A phone that drains its battery running Google services, sends data to US cloud endpoints, and fails to properly support mobile money menus is simply a worse phone for this context. Not philosophically worse — functionally worse. Less useful per charge. More expensive per megabyte. Less capable at the one financial task most users need daily.

---

### Three Carriers, One Radio

Airtel Zambia: LTE Bands 3, 7, 8. Mobile money via *778#. VoLTE deployed.

Zamtel: LTE Bands 7, 40 (TDD). Mobile money via *303#. State-owned. Stronger in some rural areas where Airtel has less presence.

MTN Zambia: LTE Band 3. Mobile money via *112#. Smaller network but serves important urban and commercial segments.

All three fall within the QM215 modem's supported band set. No hardware modification. No RF recertification. Pre-configured APNs, MCFG carrier profiles, eDRX targets for each.

Live SIM testing — not emulation — as the validation gate before any release. If mobile money doesn't work on a live SIM from all three carriers, the build doesn't ship.

---

### Mobile Money as Primary Use Case

This deserves its own space because it's the thing that makes or breaks the device's usefulness.

Mobile money access works through two mechanisms:

**STK (SIM Toolkit)** — the SIM card itself runs a small application presenting menus on the phone's screen. No internet required. No data connection. Runs entirely over the GSM signaling channel. Insert an Airtel SIM, the Airtel Money menu appears. If the STK app is killed by aggressive battery management — as happens on many custom ROMs — the menu disappears and the user loses access to their money. ZAKO keeps STK alive and exempts it from all power restrictions.

**USSD** — short codes dialed as phone numbers (*778#, *303#, *112#) that open interactive text sessions over the network's signaling channel. Works on 2G. No LTE or data plan required. The AOSP dialer handles this natively, but it must be tested on each carrier with each release.

The failure mode to avoid: treating STK as bloatware. Removing it. Throttling it. Letting a battery optimizer kill it. In ZAKO's world, mobile money access runs at CRITICAL priority. It does not get suspended. It does not get gated. A Zambian user with 5% battery can still check their balance and send money. That's the requirement.

---

### Language and Input

Default locale: `en_ZM`. Default timezone: `Africa/Lusaka` (UTC+2, no daylight saving).

Compiled-in locale support for Bemba, Nyanja, Tonga, Lozi. The T9 input method handles extended characters for all four languages — long-press sequences for diacritics and special characters that no existing Android T9 IME supports.

First-run experience: language selection, timezone confirmation, SIM detection, PIN creation, privacy disclosure. No Google account prompt. No "sign in to continue." No external account creation of any kind. The device is yours from the first screen.

---

## Distributions and Infrastructure

### The Standard vs. The Distribution

ZAKO OS is to Babb Cat as Linux is to Ubuntu.

The Standard — everything defined in the ZAKO spec documents — does not change when the hardware changes. What changes is the implementation profile: which hardware capabilities are available, which features are P0 versus deferred, which carriers are pre-configured, which languages are compiled in, what the power thresholds are for the specific battery size.

A distribution is a specific expression of the Standard on specific hardware for a specific context. Babb Cat is the first distribution: Cat S22 Flip, Zambia, three carriers, T9 input, 1450mAh power profile. But the Standard is ready for others.

---

### Infrastructure Roles

Beyond personal devices, ZAKO defines infrastructure participants:

**Personal Device** — the baseline. No infrastructure obligations. Your phone. Your sovereignty. Your network.

**Relay Node** — forwards records between parties without storing them. Authenticates sender and receiver by DID. Delivers without reading. A post office that doesn't open the letters.

**Ledger Node** — operates a `telux-ledgerd`-equivalent for hosted newgroups. Accepts submissions from enrolled parties, chain-hashes in arrival order, countersigns each, distributes to all members. Provides total ordering and real-time distribution without replacing participants' sovereign copies. A coordination service, not an owner.

**Content Node** — CDN for Academy content. BLAKE3-addressed, hash-verified. The node proves it delivered the right bytes. It cannot link content requests to a DID. Privacy-preserving by architecture, not by policy.

---

### Multi-Device Model

A Sovereign who operates personal infrastructure — a home server, a Raspberry Pi, a VPS — extends their sovereign boundary. The personal device and the infrastructure node share identity context through a delegated signing key (depth 1, scope-limited, expirable, renewable).

Synchronization between devices produces SEND + RECEIVE record pairs in each device's Exchange sub-entity. Auditable sync history. You can prove what was synced, when, and between which DIDs.

---

### Revenue Sharing

When distributions operate infrastructure that charges fees — content delivery, credential issuance, managed services — those fees are recorded on-ledger. Not in a separate billing system. On the same ledger as everything else, with the same conservation invariant.

Total fees = sum of outgoing payments + retained balance. The same conservation law that applies to a mobile money payment applies to infrastructure revenue. BitLedger records all the way down.

---

## The Performance Contract

### Adjustability Codes

Not everything in ZAKO can be changed. The system declares, for every parameter, who gets to adjust it:

**FIXED** — protocol-defined. No implementation may change it. Chain hash algorithm (BLAKE3). Sovereign signature (ed25519). Max pictography sequence (8 symbols). These are the laws of the system.

**DIST** — distribution-configurable at build time. Carrier APNs. Default locale. Service enable/disable. The distribution makes these choices and ships them.

**DIST+SOV** — distribution sets the range, Sovereign overrides at runtime. Power mode thresholds. Display timeouts. eDRX intervals. The distribution says "this is the safe range." The Sovereign picks their value within it.

**SOV** — Sovereign-adjustable within protocol-defined range. Personal preferences that don't affect protocol correctness.

---

### Key Constants

The numbers that define ZAKO's behavior:

**Mode thresholds (defaults):** STANDARD_FLOOR 50%, CONSERVE_FLOOR 20%, LOW_FLOOR 10%, SHUTDOWN_FLOOR 3%.

**Hysteresis band:** 3%. Prevents flapping at boundaries.

**Sampling intervals:** 30s in FULL, 60s in STANDARD, 120s in CONSERVATION, 300s in CRITICAL, 30s in EMERGENCY (fast, because you need to know discharge rate when survival depends on it).

**Thermal critical:** 45°C sustained 60 seconds → EMERGENCY. Recovery requires <40°C for 120 seconds.

**Ledger fsync:** synchronous before ACK. Always. No deferred option. No configuration to disable it. FIXED.

**Chain hash:** BLAKE3. FIXED.

**Sovereign signature:** ed25519. FIXED.

**Capability revocation:** immediate. Cascades all delegations. No grace period.

**Max pictography sequence:** 8 symbols (4 bytes) without intervening structural record. FIXED.

These aren't aspirational. They're the contract. A distribution that ships with a different chain hash algorithm is not a ZAKO distribution. A device that defers ledger fsync is not ZAKO-conformant. The performance contract is what makes "ZAKO" mean something specific.

---

## The Open Questions

Honest engineering means naming what isn't solved.

### AI Entity Identity

How are LLM instances identified persistently across sessions? The ZAKO architecture gives AI entities first-class Island membership — capabilities, power budgets, auditable records. But what constitutes the "identity" of a model that can be instantiated on different hardware, fine-tuned into different versions, or run simultaneously in multiple contexts?

The DID framework works. The question is what the DID *points to* when the entity isn't a stable keypair in hardware but a probabilistic model that changes with every training run.

---

### Sovereignty Succession in Degraded Conditions

Six succession types are defined in the protocol: voluntary transfer, emergency delegation, time-based expiry, incapacitation, death, and organizational dissolution.

But: what happens when a Sovereign becomes unreachable in a remote deployment? A relay node on a mountain. An excavator in a mine. A device whose Sovereign is simply gone and whose records need to continue flowing. The edge cases are where succession gets hard.

---

### Cross-Island Federation

Federated ledger sync across physical servers. The concept is defined. WireGuard-based transport. But the protocol details for conflict resolution when two Ledger Nodes receive contradictory submissions for the same newgroup — that's not fully specified. Total ordering via Lamport timestamps handles most cases. "Most" isn't "all."

---

### VoLTE Provisioning

Voice over LTE requires carrier-side provisioning. The Zambian carriers have not been asked whether they'll provision a custom device. CSFB (2G/3G voice fallback) is the guaranteed baseline. VoLTE is documented as a post-release enhancement, dependent on carrier willingness.

---

### The Interplanetary Horizon

The same architecture at light-speed latency. Records that survive millennia in durable storage. Autonomous operation without Earth infrastructure.

This is in the outline because the architecture genuinely supports it. Offline-first means no assumption of connectivity. Conservation means records are self-verifying. Chain hashing means tamper evidence without external reference. ed25519 means identity without an online authority.

But "supports in principle" and "proven in practice at 20-minute round-trip delay" are different things. This remains horizon work. Named. Unproven. Honest about the gap.

---

## The Claim

### What ZAKO Is Not

Not a hobbyist alternative. Not a privacy paranoia exercise. Not a degoogled ROM with a nice wallpaper. Not an argument that the existing ecosystem is wrong for who it currently serves.

### What ZAKO Is

A serious engineering project with serious philosophical foundations.

Sovereignty as engineering discipline — enforced by hardware, kernel, and protocol. Not a marketing claim. Not a toggle in settings.

Conservation at the wire level — structural property of the encoding, not a rule the application enforces. Five bytes for double-entry. Batches that don't balance cannot be formed.

Power governance from aerospace — the organizing principle of the entire runtime. Not battery-saver settings. Not "reduce brightness when low." The device knows what it can afford to run and makes that decision at the kernel boundary.

Identity from cryptographic first principles — ed25519 in hardware, W3C DIDs, capability tokens as the sole mechanism for access. Not platform accounts. Not login screens. Not third-party verification of who you are.

### The Scale

A phone in Lusaka, Zambia. A woman checking her mobile money balance on 5% battery, knowing the device will keep that function alive because it's CRITICAL class.

An autonomous excavator in a Mongolian copper mine. Outstack governing power across subsystems, telux-ledgerd recording every operational exchange, the machine operating for days without human contact.

A humanoid assistant in a hospital in Lagos. Health records as conserved quantities, RESTRICT_FORWARD on reproductive data, alerts that never get deferred by power gating.

A relay node on a spacecraft between Earth and Mars. Twenty-minute light delay. Records created, signed, chain-hashed, stored locally. Transmitted when the link window opens. Verified on receipt without contacting any Earth authority.

The architecture does not change. The deployment profile changes. ZAKO adapts.

The sovereignty does not.

---

*This concludes the three-part series. The protocol specifications, service definitions, and hardware documentation that underpin this article are maintained across 48 documents in the ZAKO corpus. Babb Cat — the first distribution — is entering its experimental phase on the Cat S22 Flip. What happens next will be recorded. On the ledger.*
