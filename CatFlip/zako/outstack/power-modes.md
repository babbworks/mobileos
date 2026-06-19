# Ya — Outstack Power Mode Configuration

Ya-specific Outstack power mode settings for the Cat S22 Flip (1450mAh battery, MSM8937 / Cortex-A53 × 8). This document explains the rationale behind every threshold and governor choice in `outstack.conf`.

Reference: `ZAKO/PACKAGES/zako-outstack.md`, `ZAKO/PROTOCOLS/Outstack-Protocol-v1.md`

---

## The Constraint

The Cat S22 Flip has a 1450mAh battery. This is small relative to most modern smartphones (typical Android flagship: 4000–5000mAh). It sets the benchmark for ZAKO power doctrine on constrained hardware — every threshold below is justified against this number.

Target operating goals:
- **Standby:** 5+ days (LTE eDRX with 327.68-second paging cycles)
- **Active use:** 6+ hours mixed voice/data
- **LTE standby power:** <100mW
- **Deep idle floor:** <50mW

---

## outstack.conf — Ya Values

```ini
[modes]
full_power_min    = 80
standard_min      = 40
conservation_min  = 20
critical_min      = 5
hysteresis        = 3

[governors]
full_power    = schedutil
standard      = schedutil
conservation  = powersave
critical      = powersave
emergency     = powersave

core_park_threshold  = standard   # park in conservation, critical, emergency
core_park_min_online = 1

[display]
full_power    = 60
standard      = 30
conservation  = 20
critical      = 10
emergency     = 5
emergency_display_on_call_only = true

[radio]
standard_edrx_enable  = true
standard_edrx_cycle   = 8192
standard_ptw          = 2048

conservation_edrx_enable = true
conservation_edrx_cycle  = 8192
conservation_ptw         = 1024

critical_edrx_enable  = true
critical_edrx_cycle   = 8192
critical_ptw          = 512

emergency_edrx_enable = true
emergency_edrx_cycle  = 8192
emergency_ptw         = 256

psm_enable            = false
```

---

## Mode Rationale

### Full Power (>80%)

CPU governor: `schedutil`. This is AOSP's default energy-aware scheduler — it scales frequency based on actual CPU utilisation, which on MSM8937's uniform Cortex-A53 cluster gives good responsiveness without wasted frequency headroom. `performance` governor is intentionally avoided; there is no use case in the Zambia field deployment that justifies pinning all cores to max frequency.

No core parking in Full Power — all 8 cores available. Display timeout 60s is generous but appropriate for this mode.

### Standard (40–80%)

CPU governor: `schedutil` (maintained). Core parking activates: idle cores park to reduce leakage current. MSM8937 has no heterogeneous cluster — parking applies uniformly. `core_park_min_online = 1` ensures the device never deadlocks waiting for a core to come online.

Display timeout drops to 30s. This alone saves significant battery versus the standard Android 60s default.

eDRX activates at Standard: 327.68-second paging cycle with a 20.48-second paging time window. For a user on Airtel Zambia or MTN Zambia, this means the modem wakes to check for pages every ~5.5 minutes instead of continuously — dramatic standby improvement with minimal call latency impact (worst case: call rings 5 minutes after arrival, not instant).

### Conservation (20–40%)

CPU governor: `powersave` — hard lock to minimum CPU frequency. Qualcomm's power management IC throttles the cores to their lowest validated operating point. Fine for Telux exchange, PADS record creation, telephony. Poor for compute-intensive tasks — acceptable given the battery situation.

Process class gating takes effect: `background` class processes receive SIGSTOP. All background sync, installd, F-Droid background activity stops.

eDRX paging time window narrows to 10.24s (from 20.48s in Standard). Fewer pages processed per cycle → lower radio Rx power.

Display timeout: 20s.

### Critical Reserve (5–20%)

One primary goal: keep telephony alive and Telux exchange running.

CPU: `powersave`, single core online (all others parked). On MSM8937's Cortex-A53 cluster, a single 1.3GHz core is sufficient for voice call processing and Telux exchange.

`user-active` class gating activates: SurfaceFlinger, InputFlinger, AudioServer, Launcher all receive SIGSTOP unless an active call is in progress. Screen is dark most of the time.

eDRX PTW narrows to 5.12s. Device is spending minimal time with modem radio active.

Display timeout: 10s.

**Call behavior in Critical:** `communication` class processes (telecom, phone, rild) are never gated. Incoming calls still ring. Outgoing calls still connect. The user sees a minimal UI to handle the call.

### Emergency (<5%)

Survival mode. The single goal is completing any in-progress Telux exchange before shutdown, and remaining reachable by voice.

CPU: `powersave`, 1 core. Display is off unless an active call is in progress (`emergency_display_on_call_only = true`). Display timeout: 5s from unlock.

`communication` class gating: radio enters standby-only. Inbound SMS and calls still reach the device via the hardware modem (rild runs as `system-critical`). Active data connections are suspended.

**Telux exchange in Emergency:** The Exchange Engine is CRITICAL priority and runs in all modes including Emergency. Any pending exchange (a payment in-flight, a PADS invoice awaiting settlement) completes as long as the device has carrier access. This is a core design invariant of ZAKO: exchange survives power stress.

---

## eDRX Implementation Notes

eDRX values are passed to the RIL at mode entry by `outstack-governed`. The system.prop entries in `system.prop` set boot defaults before Outstack takes control:

```
ro.ril.edrx.enable=1
ro.ril.edrx.ptw=2048
ro.ril.edrx.cycle=8192
```

PSM (Power Saving Mode) is disabled (`psm_enable = false`) because Zambian carriers do not support it as of 2026-06. If carrier support becomes available, enabling PSM in Conservation and Critical modes would further reduce standby power.

**Carrier eDRX support status:**
- Airtel Zambia: eDRX supported ✓ (confirmed per carrier spec research)
- MTN Zambia: eDRX supported ✓
- Zamtel: unconfirmed — test on device

---

## Hysteresis

`hysteresis = 3` means the device does not re-enter a more restrictive mode until battery drops 3 percentage points below the threshold it just exited. This prevents oscillation at threshold boundaries — without hysteresis, a device at exactly 40% battery would rapidly toggle between Standard and Conservation as normal load causes minor battery fluctuation.

Three points is conservative; it can be reduced to 2 if testing shows unnecessary time in less-efficient modes.

---

## Tuning Log

Track observed power behavior and any threshold adjustments: `CatFlip/zako/outstack/tuning-log.md`
