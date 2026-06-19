# ZAKO OS v1 — Component Inventory

Every distinct engineering element that must be researched, designed, implemented, and verified to produce an operational ZAKO OS.

Derived from: ZAKO-Standard-v1, Outstack-Protocol-v1, Telux-Protocol-v1, SIMBA-Standard-v1, Wire-Conventions-v1, BabbCat-Implementation-Plan.

---

## Layer 1: Bedrock (Kernel & Hardware)

| ID | Component | Description | Language | Status |
|----|-----------|-------------|----------|--------|
| B-01 | Kernel build | Linux 4.9 CAF, MSM8937 defconfig, ZAKO patches | C (kernel) | Specified |
| B-02 | Telux-SEC LSM | Linux Security Module for Island boundary enforcement at kernel level | C (kernel module) | Concept only |
| B-03 | Outstack exec gate | LSM hook at execve() checking process class vs power mode | C (kernel module) | Concept only |
| B-04 | Cover display driver | ST7789 SPI panel initialization and framebuffer for 1.44" cover | C (kernel driver) | Unknown (DTB pending) |
| B-05 | Lid sensor integration | Hall effect SW_LID input event, GPIO mapping | C (DTS + driver) | Specified |
| B-06 | T9 keypad driver | GPIO-keys scan codes for physical keypad | C (DTS + driver) | Reference exists |
| B-07 | Power domain controller | Hardware power gating interface via sysfs/RPM | Kernel interface | Reference exists |
| B-08 | dm-verity configuration | AVB vbmeta signing, hash tree generation | Build config | Specified |
| B-09 | FBE configuration | File-based encryption with Keymaster 4.0 ICE | Build config | Specified |

## Layer 2: Submerged (ZAKO Daemons)

### Outstack Subsystem

| ID | Component | Description | Language | Status |
|----|-----------|-------------|----------|--------|
| S-01 | outstack-powerd | Five-mode state machine, battery/thermal polling, mode transitions, record emission | C | Specified, no code |
| S-02 | outstack cgroup manager | cgroup freezer hierarchy creation, process class assignment, freeze/thaw | C (part of S-01) | Specified |
| S-03 | outstack radio helper | Thin service applying eDRX AT commands per mode transition | C or shell | Specified |
| S-04 | outstack-policy.xml parser | XML parser for process class assignments | C | Specified format |
| S-05 | outstack.conf parser | INI parser for distribution power profile | C | Specified format |

### Telux Subsystem

| ID | Component | Description | Language | Status |
|----|-----------|-------------|----------|--------|
| S-10 | telux-ledgerd | Append-only SQLite ledger, conservation enforcement, chain hashing, fsync-before-ACK | C | Specified, no code |
| S-11 | telux-identd | ed25519 key generation, DID formatting, capability grant records, identity lock | C | Specified, no code |
| S-12 | telux-sharedb | Outbound transmission mediation, channel selection (SMS/QR/BLE/IP), queue | C | Specified, no code |
| S-13 | telux-coverd | Cover display daemon — time, battery, mode, ledger summary, caller ID | C | Specified |
| S-14 | Exchange Engine | Bilateral settlement core — SEND/RECEIVE cycle, conservation check, atomic posting | C (part of S-10) | Specified |

### Protocol Codec Layer

| ID | Component | Description | Language | Status |
|----|-----------|-------------|----------|--------|
| S-20 | bitpads-codec | BitPads v2.0 frame encoder/decoder — Meta byte, 4 frame types | C library | Specified |
| S-21 | bitledger-codec | BitLedger v3.0 40-bit record encoder/decoder, conservation invariant check | C library | Specified |
| S-22 | C0 enhancement parser | C0 Enhancement Grammar — 13 positions, priority/ACK/continuation | C library | Specified |
| S-23 | Pictography codec | 4-bit symbol encoder/decoder, codebook management, Context Declaration | C library | Specified |
| S-24 | Chain hash engine | BLAKE3 hashing, chain_hash computation, genesis anchor | C library | Specified |
| S-25 | pads-v1 URL codec | Workpads URL format encoder/decoder for SMS transmission (<300 chars) | C library | Specified |

### Identity & Crypto

| ID | Component | Description | Language | Status |
|----|-----------|-------------|----------|--------|
| S-30 | ed25519 signing | Key generation, sign, verify — wrapping libsodium or TweetNaCl | C library | Well-understood |
| S-31 | DID formatter | did:key method, z6Mk prefix, BASE58BTC encoding | C library | Specified |
| S-32 | Capability system | GRANT/REVOKE/DELEGATE records, depth checking, cascade revocation | C (part of S-11) | Specified |
| S-33 | Keymaster HAL interface | Interface to TrustZone Keymaster 4.0 for hardware-backed keys | C + HAL | Reference exists |

### IPC & Bus

| ID | Component | Description | Language | Status |
|----|-----------|-------------|----------|--------|
| S-40 | ZAKO system bus | Unix domain socket IPC between daemons, C0 signal routing | C | Design needed |
| S-41 | Record intake socket | Unix socket for telux-ledgerd record submission | C (part of S-10) | Specified |
| S-42 | Mode signal broadcast | Broadcast mechanism for MODE_ENTER to all listening daemons | C (part of S-01) | Specified |

## Layer 3: Visible (Android Framework & Apps)

### AOSP Customization Points

| ID | Component | Description | Language | Status |
|----|-----------|-------------|----------|--------|
| V-01 | Device tree (BoardConfig) | Partition sizes, kernel cmdline, AVB config, SELinux paths | Makefile | Partially specified |
| V-02 | SELinux policy | Custom domains for ZAKO daemons, daemon-to-daemon access | SELinux TE | Design needed |
| V-03 | init.rc services | Service entries for outstack-powerd, telux-*, telux-coverd | Android init | Specified |
| V-04 | Overlay: carrier config | APN configs for Airtel/MTN/Zamtel, USSD allowlists | XML overlays | Specified |
| V-05 | Overlay: settings | De-Google captive portal, NTP, DNS, no GMS setup wizard | XML overlays + props | Specified |
| V-06 | ZAKO Setup Wizard | First-run: language, SIM detect, PIN, privacy, no Google account | Java/Kotlin | Design needed |
| V-07 | T9 IME | Custom input method with Bemba/Nyanja/Tonga/Lozi extended chars | Java/Kotlin | Design needed |
| V-08 | Outstack widget | Status bar/tile showing current power mode | Java/Kotlin | Design needed |

### ZAKO Service Apps

| ID | Component | Description | Language | Status |
|----|-----------|-------------|----------|--------|
| V-10 | PADS app | Field records UI — forms, photos, signoffs, Work Island management | Java/Kotlin | Design needed |
| V-11 | Exchange app | Telux exchange UI — send/receive, query ledger, natural language | Java/Kotlin | Design needed |
| V-12 | Sovereignty dashboard | Island management, capability grants, identity, ledger browser | Java/Kotlin | Design needed |
| V-13 | NLQ engine | Natural language query interface for ledger — rule-based FSM | C or Java | Specified |

### Third-Party Integration

| ID | Component | Description | Language | Status |
|----|-----------|-------------|----------|--------|
| V-20 | F-Droid privileged | Privileged system app for silent updates | Pre-built APK | Ready |
| V-21 | ntfy + UnifiedPush | Push notification relay, replaces FCM | Pre-built APK + server | Ready |
| V-22 | Organic Maps | Offline OSM, replaces Google Maps | Pre-built APK | Ready |
| V-23 | AOSP Dialer | Voice/SMS/USSD — stock, must preserve STK | Stock AOSP | Verified on AOSP |

## Layer 4: Build & Infrastructure

| ID | Component | Description | Language | Status |
|----|-----------|-------------|----------|--------|
| I-01 | AOSP manifest | Repo manifest for full source tree sync | XML | Design needed |
| I-02 | Vendor blob extraction | proprietary-files.txt + extract-files.sh for Cat S22 | Shell | Specified |
| I-03 | Kernel build pipeline | Cross-compilation, defconfig, patch application | Makefile + shell | Specified |
| I-04 | OTA generation | Full + incremental OTA, signing, distribution server | AOSP tools | Specified |
| I-05 | CI/CD pipeline | Automated build, basic boot test, artifact publishing | Docker + shell | Design needed |
| I-06 | Distribution server | OTA endpoint, update manifest, HTTPS | Server config | Design needed |

---

## Summary Counts

| Category | Total | Specified | Code Exists | Verified |
|----------|-------|-----------|-------------|----------|
| Bedrock (kernel/hardware) | 9 | 7 | 0 | 0 |
| Submerged (daemons) | 22 | 20 | 0 | 0 |
| Visible (Android/apps) | 12 | 5 | 0 | 0 |
| Build/Infrastructure | 6 | 4 | 0 | 0 |
| **Total** | **49** | **36** | **0** | **0** |

---

## Engineering Priority Order (Dependency-Driven)

### Phase 1 — Foundation Libraries (no hardware dependency)
1. S-24 Chain hash engine (BLAKE3)
2. S-30 ed25519 signing
3. S-31 DID formatter
4. S-20 bitpads-codec
5. S-21 bitledger-codec
6. S-22 C0 enhancement parser

### Phase 2 — Core Daemons (requires Phase 1 libraries)
7. S-10 telux-ledgerd
8. S-11 telux-identd
9. S-01 outstack-powerd
10. S-40 ZAKO system bus

### Phase 3 — Exchange & Transmission (requires Phase 2 daemons)
11. S-14 Exchange Engine
12. S-12 telux-sharedb
13. S-25 pads-v1 URL codec
14. S-32 Capability system

### Phase 4 — AOSP Integration (requires hardware + Phase 2)
15. B-01 Kernel build
16. V-01 Device tree
17. V-02 SELinux policy
18. V-03 init.rc services
19. I-01 AOSP manifest

### Phase 5 — Device Bring-Up (requires Phase 4)
20. B-04 Cover display driver
21. B-05 Lid sensor
22. B-06 T9 keypad
23. V-04/V-05 Overlays
24. I-02 Vendor blob extraction

### Phase 6 — Applications (requires Phase 3 + Phase 5)
25. V-06 Setup Wizard
26. V-07 T9 IME
27. V-10 PADS app
28. V-11 Exchange app

### Phase 7 — Hardening (requires all above)
29. B-02 Telux-SEC LSM
30. B-03 Outstack exec gate
31. V-08 Outstack widget
32. V-12 Sovereignty dashboard

---

*This inventory is the canonical reference. Components added or removed here trigger backlog updates in the workwarrior ledger.*
