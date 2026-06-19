# ZAKO Distribution Checklist

Minimum gates a distribution must pass before being considered release-ready. Work through phases in order — each phase must be complete before the next begins.

Reference `distribution-profile-template.md` for the profile fields that correspond to each gate.

---

## Phase 0 — Profile Complete

```
[ ] Distribution profile filed at ZAKO/DISTRIBUTIONS/PROFILES/[name].md
[ ] All [REQUIRED] fields in profile completed
[ ] Device folder created and named (MobileOS/[DistroFolder])
[ ] BASE seed copied into distribution folder
[ ] Kernel source identified and cloned
[ ] Firmware dump acquired from stock OS
```

---

## Phase 1 — Device Intelligence

Produce and validate:

```
[ ] device-profile.md — SoC, RAM, storage, display, radios, sensors, peripherals, pinouts
[ ] partition-map.md — full partition table, sizes, filesystem types, purpose of each
[ ] bootchain.md — BootROM → XBL/aboot → kernel → init sequence
[ ] kernel-source.md — GPL source identified, cloned, version confirmed
[ ] fastboot-edl-reference.md — fastboot and EDL commands verified on physical device
[ ] Bootloader unlock procedure documented and tested
```

---

## Phase 2 — Build Environment

```
[ ] AOSP base branch selected and synced
[ ] Device tree extracted from firmware and integrated
[ ] Vendor blobs extracted and catalogued in vendor-blobs.md
[ ] Kernel defconfig baseline builds cleanly
[ ] Full AOSP build completes without errors
[ ] Build boots on device (userdebug)
[ ] ADB shell accessible on first boot
```

---

## Phase 3 — Telephony Verified

*No distribution ships without telephony fully functional. These are non-negotiable.*

```
[ ] Voice calls: outgoing calls connect on all target carriers
[ ] Voice calls: incoming calls ring and connect
[ ] SMS: outgoing SMS delivered on all target carriers
[ ] SMS: incoming SMS received
[ ] MMS: send and receive (if applicable to market)
[ ] USSD: *code# menu trees work for mobile money on all carriers
[ ] SIM Toolkit (STK): STK menu accessible for all carriers
[ ] Data: LTE data connects on all carriers
[ ] Data: APN configured correctly per carrier
[ ] VoLTE: tested (or explicitly documented as not supported with reason)
[ ] Carrier switching: SIM swap works correctly
```

---

## Phase 4 — Outstack Power Governance

```
[ ] Outstack profile configured in distribution profile
[ ] All five power modes tested under load
[ ] Power mode transitions do not break telephony
[ ] Deep idle achieved (measure mW at locked screen, WiFi off)
[ ] LTE standby measured and within target (see distribution profile)
[ ] Battery discharge rate documented under standard use
[ ] No rogue wake locks consuming battery in any mode
[ ] Conservation mode does not drop calls in progress
```

---

## Phase 5 — Telux Exchange Layer

```
[ ] Telux daemon triad starts cleanly (telux-ledgerd, telux-identd, telux-sharedb)
[ ] Identity creation: DID generated and stored hardware-backed
[ ] Island creation: user can create and name an Island
[ ] Exchange: two-party exchange record written to ledger correctly
[ ] Record chain-hashing: each record references previous record hash
[ ] Carrier layer: at least one carrier transport (SMS/BLE/QR/IP) functional
[ ] Push relay configured and reachable
[ ] PADS record type functional end-to-end
```

---

## Phase 6 — De-Googling and Security

```
De-Googling:
[ ] No GMS packages in build (confirmed via: pm list packages | grep google)
[ ] No Google traffic on fresh boot (tcpdump audit)
[ ] captive_portal_server redirected to owned endpoint
[ ] NTP server changed from Google
[ ] DNS changed from Google 8.8.8.8
[ ] ro.error.receiver.default cleared
[ ] F-Droid installed as privileged system app

Security:
[ ] SELinux enforcing on production build (getenforce = Enforcing)
[ ] dm-verity active on system/vendor/product (production)
[ ] FBE active (getprop ro.crypto.state = encrypted)
[ ] All partition images AVB-signed with distribution keys (not AOSP test keys)
[ ] AVB rollback index set to 1 for first production release
[ ] Build keys stored securely outside build tree
[ ] Key fingerprints documented
```

---

## Phase 7 — Testing and Validation

```
Hardware:
[ ] WiFi: connects, sustains throughput, survives sleep/wake cycle
[ ] Bluetooth: pairs, transfers data
[ ] Camera: photo and video functional (or documented gap with reason)
[ ] GPS: acquires fix (or documented gap)
[ ] Sensors: accelerometer, proximity, ambient light functional
[ ] Audio: earpiece, speaker, microphone, headset all tested
[ ] Charging: device charges correctly, thermal behavior during charge documented

Stability:
[ ] 48-hour continuous soak: no unexpected reboots
[ ] Call soak: 20+ incoming and outgoing calls without failure
[ ] SMS soak: 50+ messages sent and received
[ ] Battery cycle: full charge to low battery through normal use

Form factor (if applicable):
[ ] Any device-specific hardware tested (flip hinge, secondary display, physical keyboard, etc.)
```

---

## Phase 8 — Documentation Complete

```
[ ] All [PRE-RELEASE] fields in distribution profile completed
[ ] gaps-and-blind-spots.md written and current
[ ] known-issues.md populated with all known issues
[ ] release-process.md adapted from BASE with device-specific commands
[ ] Regression log initialized with Phase 7 results
[ ] test-plan.md adapted from BASE template
[ ] Flash script tested end-to-end on clean device
[ ] Release notes written
[ ] SHA-256 checksums generated for all release artifacts
[ ] GPL sources published with release tag
[ ] OTA server configured and tested
```

---

## Final Sign-off

Before declaring a distribution released:

```
[ ] All Critical and High severity known issues either resolved or documented with user-visible note
[ ] Security hardening checklist in security-model.md passed
[ ] Distribution profile marked Status: Released
[ ] Profile ZAKO standard version field confirmed correct
```
