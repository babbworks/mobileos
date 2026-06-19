# Ya — ZAKO Services Manifest

Overview of all ZAKO service packages wired into the Ya distribution, their config file locations in the device tree, and their current integration status.

---

## Services at a Glance

| Service | Package | Status | Config location |
|---|---|---|---|
| Outstack | `ZakoOutstack` | Active — v1 | `device/cat/S22FLIP/zako/outstack/` |
| Telux | `ZakoTelux` | Active — v1 | `device/cat/S22FLIP/zako/telux/` |
| PADS | `ZakoPADS` | Active — v1 | `device/cat/S22FLIP/zako/pads/` |
| SIMBA | `ZakoSIMBA` | Active — v1 (empty registry) | `device/cat/S22FLIP/zako/simba/` |
| Agreements | `ZakoAgreements` | Active — v1 (via Telux) | No separate config; uses Telux exchange layer |
| Health | `ZakoHealth` | Deferred — post-v1 | — |
| Academy | `ZakoAcademy` | Deferred — post-v1 | — |

---

## Package Repos

Wired into the AOSP manifest via `.repo/local_manifests/babb-zako.xml`. See `CatFlip/briefings/04-zako-integration.md` for the full manifest XML.

| Package | Repo | Revision |
|---|---|---|
| `ZakoOutstack` | `github.com/babb-os/zako-outstack` | `main` |
| `ZakoTelux` | `github.com/babb-os/zako-telux` | `main` |
| `ZakoPADS` | `github.com/babb-os/zako-pads` | `main` |
| `ZakoSIMBA` | `github.com/babb-os/zako-simba` | `main` |

---

## Outstack

**Config files:**

| File | Description |
|---|---|
| `zako/outstack/outstack.conf` | Battery thresholds, CPU governors, display timeouts, eDRX radio config, power ledger path |
| `zako/outstack/outstack-policy.xml` | Process class assignments for all Ya daemons and apps |

**Ya-specific notes:**
- 1450mAh battery requires aggressive threshold placement — see `CatFlip/zako/outstack/power-modes.md`
- MSM8937 is Cortex-A53 only (no big.LITTLE) — core parking applies uniformly
- All 8 cores are identical; `core_park_min_online = 1` applies in Conservation and below
- eDRX cycle at 8192 (327.68s) is the maximum supported paging interval — key for 5-day standby target

**Process class assignments for Ya daemons:**
- All ZAKO platform daemons: `system-critical`
- Telux communication daemons: `communication`
- Foreground UI (SurfaceFlinger, InputFlinger, AudioServer, Launcher): `user-active`
- Background services (installd, F-Droid background): `background`
- Deferred work (dex opt, storage maintenance): `deferred`

Detailed assignments: `CatFlip/zako/outstack/process-classes.md`

---

## Telux

**Config files:**

| File | Description |
|---|---|
| `zako/telux/telux.conf` | Identity, ledger, exchange engine, carrier, sharedb configuration |
| `zako/telux/carriers/airtel-zm.conf` | Airtel Zambia — MCC/MNC 64504, APN, USSD mobile money |
| `zako/telux/carriers/mtn-zm.conf` | MTN Zambia — MCC/MNC 64501 |
| `zako/telux/carriers/zamtel.conf` | Zamtel — MCC/MNC 64503 |

**Ya-specific notes:**
- Three Zambian carriers configured as Telux transport surfaces
- Primary carrier for exchange: SMS (widest coverage across rural Zambia)
- IP secondary when LTE available
- QR code for identity exchange at initial pairing (no network required)
- `did_method = did:key` — no external registry; fully offline-capable identity
- `key_backend = hardware` — Keymaster 4.0 on TrustZone (MSM8937)
- Captive portal: `204.babb.tel`
- NTP: `africa.pool.ntp.org`

Carrier detail: `CatFlip/zako/telux/carriers.md`

---

## PADS

**Config files:**

| File | Description |
|---|---|
| `zako/pads/pads.conf` | Work Island limits, currency (ZMW), pads-v1 codec settings, Service Views |

**Ya-specific notes:**
- `base_currency = ZMW` (Zambian Kwacha)
- Primary use case: informal sector field workers tracking daily work assignments and payment
- pads-v1 inbound enabled for compatibility with existing Workpads clients in Zambia
- Up to 16 active Work Islands per device (normative max)

Field worker flows: `CatFlip/zako/pads/field-worker-flows.md`

---

## SIMBA

**Config files:**

| File | Description |
|---|---|
| `zako/simba/simba.conf` | Distribution DID, admission policy, process class permissions |
| `zako/simba/registry/` | Pre-admitted service manifests (empty at v1 launch) |

**Ya-specific notes:**
- No external services pre-admitted at v1
- `distribution_did` must be set to Ya's root signing DID before production build
- All process class claims by future services are capped at `background` by default — distribution must explicitly approve any service claiming `interactive`

---

## Service Dependency Map

```
telux-ledgerd ←──── outstack-governed (ledger writes on mode transition)
telux-ledgerd ←──── telux-identd (staging queue processing)
telux-ledgerd ←──── telux-sharedb (inbound record submission)
telux-ledgerd ←──── pads-workd (all PADS record writes)
telux-identd  ←──── telux-sharedb (signing for all outbound records)
telux-identd  ←──── simba-governed (capability grant issuance)
outstack-governed → telux-identd (mode signals affect identity lock behavior)
outstack-governed → pads-workd (process class gating)
outstack-governed → simba-governed (process class gating for all SIMBA Nodes)
```

All ZAKO daemons are `system-critical` or higher and are never gated by Outstack power modes.

---

## Version Matrix

| ZAKO standard version | Outstack protocol | Telux protocol | PADS spec | SIMBA standard |
|---|---|---|---|---|
| v1 | v1.0 | v1.0 | v1.0 | v1.0 |

Ya v1 ships all packages at their v1 protocol versions. Breaking changes to any protocol require a MAJOR version bump for any distribution shipping that protocol.
