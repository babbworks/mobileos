# Test Plan — [DISTRO_NAME] on [DEVICE_NAME]

Adapt this template for the distribution. Replace all `[PLACEHOLDERS]`. Sections marked `[DEVICE-SPECIFIC]` require content written for this device.

Priority levels:
- **P0** — must pass before any release. A P0 failure blocks the build.
- **P1** — must pass before production release. Can be waived for beta with documented justification.
- **P2** — regression tests. Track regressions but do not block beta.

---

## P0 — Boot and Telephony

These are the hardest gates. No release proceeds without these.

### Boot

```
[ ] Device boots to home screen
[ ] First boot completes in < [N] minutes (establish baseline)
[ ] ADB shell accessible
[ ] SELinux: adb shell getenforce = Enforcing
[ ] FBE: adb shell getprop ro.crypto.state = encrypted
[ ] No critical SELinux denials in dmesg on fresh boot
```

### Voice Calls (test on each target carrier)

```
[ ] Carrier: [CARRIER_1]
    [ ] Outgoing call connects
    [ ] Audio heard by both parties
    [ ] Call ends cleanly
    [ ] Incoming call rings
    [ ] Incoming call connects

[ ] Carrier: [CARRIER_2]
    [ ] (same set)

[ ] Carrier: [CARRIER_3] (if applicable)
    [ ] (same set)
```

### SMS (test on each carrier)

```
[ ] Outgoing SMS delivered
[ ] Incoming SMS received
[ ] MMS: [applicable / not applicable for this market]
```

### USSD / SIM Toolkit

```
[ ] [CARRIER_1] mobile money USSD code works: [*XXX#]
[ ] [CARRIER_2] mobile money USSD code works: [*XXX#]
[ ] STK menu accessible
[ ] STK menu navigation functional
```

---

## P0 — ZAKO Core

```
[ ] Outstack: device starts in Standard power mode
[ ] Outstack: mode transitions occur at correct battery thresholds
[ ] Outstack: no telephony drops during mode transition
[ ] Telux: telux-ledgerd, telux-identd, telux-sharedb start cleanly
[ ] Telux: identity (DID) created and stored on first run
[ ] Telux: exchange record written to ledger
[ ] PADS: record creation functional end-to-end
```

---

## P1 — Hardware

### Connectivity

```
[ ] WiFi: connects to WPA2 network
[ ] WiFi: sustains data throughput (basic speed test)
[ ] WiFi: survives sleep/wake cycle (reconnects after screen-off)
[ ] Bluetooth: discovers nearby devices
[ ] Bluetooth: pairs with device
[ ] Bluetooth: audio via BT headset (call and media)
[ ] GPS: acquires fix within [N] minutes in open sky
    OR: [documented gap — GPS non-functional, reason: ...]
```

### Audio

```
[ ] Earpiece: voice call audio audible
[ ] Speaker: speakerphone call audible
[ ] Microphone: far end hears caller clearly
[ ] 3.5mm headset: audio plays, microphone works (if device has headset jack)
[ ] Media audio: plays via speaker and headset
[ ] Ringtone: plays at configured volume
```

### Camera

```
[ ] Camera app launches
[ ] Photo capture: image saved to storage
[ ] Video capture: video recorded and playable
    OR: [documented gap — camera non-functional, reason: ...]
```

### Form Factor [DEVICE-SPECIFIC]

```
[Document device-specific hardware tests here]
[ ] [e.g., flip hinge: lid open/close detected]
[ ] [e.g., secondary display: shows time/status when lid closed]
[ ] [e.g., physical keyboard: all keys register correctly]
[ ] [e.g., hardware call button: answers/ends call]
```

---

## P1 — Power and Battery

```
[ ] Outstack Conservation mode: battery drain reduced vs Standard
[ ] Outstack Critical Reserve: only telephony processes running
[ ] Deep idle: measure mW at locked screen, WiFi off (target: < [N]mW)
[ ] LTE standby: measure mW (target: < [N]mW)
[ ] Charge: device charges from 0% to 100%
[ ] Charge: thermal behavior during charging within spec
[ ] No abnormal battery drain during overnight soak
```

---

## P1 — De-Googling

```
[ ] adb shell pm list packages | grep google → no results
[ ] Fresh boot network audit: no traffic to *.google.com or *.googleapis.com
[ ] captive_portal_server: adb shell settings get global captive_portal_server → [your endpoint]
[ ] NTP: adb shell getprop persist.sys.ntp_server → [regional pool]
[ ] F-Droid: installed and able to browse/install apps
```

---

## P1 — Security

```
[ ] SELinux enforcing (confirmed above in P0)
[ ] dm-verity active: adb shell getprop ro.boot.veritymode = enforcing
[ ] FBE active (confirmed above in P0)
[ ] AVB: vbmeta signed with distribution keys (not AOSP test keys)
[ ] Build keys: release build uses distribution keys
[ ] No AOSP test keys in final build
```

---

## P2 — Stability Soak

Run after all P0 and P1 tests pass:

```
[ ] 48-hour continuous soak: no unexpected reboots
[ ] Call soak: 20+ incoming and outgoing calls without failure
[ ] SMS soak: 50+ messages sent and received
[ ] Battery cycle: full charge → low battery through normal use
[ ] WiFi endurance: 12+ hours connected without disconnect
```

---

## P2 — Sensors

```
[ ] Accelerometer: screen rotation works
[ ] Proximity sensor: screen turns off during call (ear to speaker)
[ ] Ambient light sensor: auto-brightness responds to lighting change
    OR: [documented — auto-brightness not implemented]
[ ] Vibration motor: haptic feedback works
```

---

## Regression Test Notes

Record results in `project/regression-log.md` for each build candidate.

For each failed test, open an entry in `project/known-issues.md` before proceeding.
