# Ya — Outstack Tuning Log

Running log of observed power behavior, threshold adjustments, and governor tuning results on the Cat S22 Flip. Update this file as hardware testing produces data.

---

## Format

Each entry:

```
## [YYYY-MM-DD] — [Brief description]

**Build:** [build ID or commit hash]
**Device state:** [stock, custom ROM, specific kernel version]
**Test conditions:** [charge level range, carrier, WiFi on/off, screen on/off duration, apps running]

**Observation:**
[What was measured or observed]

**Action:**
[Config change made, if any]

**Outcome:**
[Result after change, if known]
```

---

## Log

*No entries yet — hardware testing has not begun. This log will be populated once the Cat S22 Flip is running a Ya build with Outstack active.*

---

## Metrics to Capture

When hardware testing begins, capture the following for each Outstack mode:

```bash
# Current power mode:
outstack-ctl status

# Battery drain rate (mA drain, read from fuel gauge):
adb shell cat /sys/class/power_supply/battery/current_now

# CPU frequency per core:
adb shell cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq

# Online cores:
adb shell cat /sys/devices/system/cpu/cpu*/online

# CPU governor:
adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

# Wakelock stats (identify which process is preventing deep sleep):
adb shell cat /proc/wakelocks

# Battery stats since last charge:
adb shell dumpsys batterystats | grep -E "Estimated|Screen|WiFi|Cellular|CPU"
```

---

## Known Power Cost Estimates (MSM8937 Reference)

These are platform-level estimates, not measured values for this specific device. Use as a baseline until measured values replace them.

| Component / State | Estimated draw |
|---|---|
| Idle, screen off, WiFi off, LTE eDRX | 8–15 mA |
| Idle, screen off, WiFi on, LTE eDRX | 15–25 mA |
| Screen on, 50% brightness | +40–60 mA |
| LTE active data transfer | +100–200 mA |
| Voice call active | +50–80 mA |
| CPU at max frequency (all 8 cores) | +200–400 mA |
| CPU at min frequency (1 core) | +10–30 mA |
| GPS active | +25–40 mA |

At 1450mAh, 15mA idle draw = ~4 days standby (96h ÷ 24h = 4 days).
Target <10mA idle with eDRX fully active.

---

## Open Tuning Questions

- [ ] What is the actual CPU frequency range on this MSM8937 unit? (`/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies`)
- [ ] Does `schedutil` governor on Linux 4.9 / CAF behave as expected, or does it need patches?
- [ ] What is measured idle current at each Outstack mode? (capture with USB power meter at each threshold)
- [ ] Does eDRX activate reliably on Airtel Zambia and MTN Zambia SIMs?
- [ ] Is `core_park_min_online = 1` sufficient, or does single-core operation cause UI jank in Conservation mode?
- [ ] What is the display power draw at 100% brightness vs. minimum? (cover display is 1.44" — does it contribute meaningful draw when closed?)
