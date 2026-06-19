# Ya — Release Index

Release history for the Ya distribution on Cat S22 Flip. Each entry links to build artifacts and documents the state of the distribution at that release.

Release process: inherited from `ZAKO/DISTRIBUTIONS/BASE/project/release-process.md`

---

## Release Naming

```
Format: ya-s22flip-MAJOR.MINOR.PATCH-YYYYMMDD
Example: ya-s22flip-1.0.0-20260101

Build target: babb_S22FLIP-user
OTA naming:   ota-babb-s22flip-VERSION-DATE-full.zip
```

---

## Releases

*No releases yet — first hardware build pending.*

---

## Dev Build Log

Track internal dev/test builds here before they become formal releases.

```
Format:
### [YYYYMMDD] — [brief description]
Build: [commit hash or build ID]
Status: [what works / what doesn't]
Test notes: [informal observations]
```

### Example entry (template):

```
### 20260801 — First boot attempt

Build: [commit hash]
Status: Boots to Android 11 lock screen. Telephony not yet functional.
        Wi-Fi works. Bluetooth not tested. ZAKO daemons starting (outstack-governed running).
        
Test notes:
  - Boot time ~3 minutes on first boot (dex opt running)
  - outstack-ctl status returns "Standard" at 65% battery ✓
  - telux-ctl identity returns DID — provisioning worked ✓
  - PADS app opens; Work Island creation crashes (pads-workd not yet wired) ✗
  - Outstack gating not yet visible in UI ✗
  - No calls possible — RIL issue TBD
```

---

## Pre-Release Checklist

Before any formal release, all items in `CatFlip/project/release-process.md` must be complete, plus:

```
Ya-specific items:
[ ] All GAPs in gaps-and-blind-spots.md reviewed and dispositioned
[ ] STK/USSD tested on Airtel Zambia and MTN Zambia SIMs (GAP-001)
[ ] Voice calls working on all three carriers
[ ] Outstack mode transitions verified at each battery threshold
[ ] Telux identity provisioning tested on fresh flash
[ ] PADS Work Island creation and ASSIGN→FINISH→INVOICE flow tested end-to-end
[ ] pads-v1 inbound via SMS tested with a Workpads client
[ ] F-Droid installs K-9 Mail without "unknown sources" prompt
[ ] captive_portal_server returns 204.babb.tel (not google.com)
[ ] No google.com traffic on fresh boot (tcpdump audit)
[ ] OTA full package installs successfully
[ ] Battery drain test: 24-hour standby with Zambian SIM, eDRX active
```

---

## GPL Source Publication

Per the release process, each release must tag the kernel source:

```bash
# Tag release on kernel repo:
cd kernel/cat/s22flip
git tag ya-s22flip-1.0.0-20260101
git push origin ya-s22flip-1.0.0-20260101
```

Published at: `github.com/babb-os/android_kernel_cat_s22flip` (planned)

---

## OTA Server

Release artifacts hosted at:
- Full OTA: `https://update.babb.tel/babb-s22flip/`
- Changelog: `https://update.babb.tel/babb-s22flip/changelog-VERSION.txt`
- Update manifest: `https://update.babb.tel/babb-s22flip/update.json`

Delta OTAs are a priority for Zambia — full OTAs are 400–600 MB which is expensive on metered Zambian data. Delta OTAs between minor releases should be <50 MB.
