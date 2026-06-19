# Embedded Field Device Architecture — Outstack on Ruggedized Hardware

**Date:** 2025-02-06
**Context:** Exploring how Outstack operates on field-deployed embedded devices with constraints different from a personal phone

---

## Scope

This note explores the architecture of Outstack on field devices: hardware deployed outdoors, in agricultural settings, in mining environments, in off-grid locations across sub-Saharan Africa. The assumptions are:

- No reliable network (cellular may be intermittent or absent for hours/days)
- No reliable power (solar with battery buffer; generator with unpredictable schedule)
- Harsh thermal environment (40°C+ ambient in direct sun; potential for <10°C at night in highland regions)
- Physical exposure (dust, rain, vibration)
- No human operator present most of the time
- Must record data continuously and transmit when possible

The question: how does Outstack's governance model adapt to this context, and what architectural decisions does the hardware constrain?

---

## Hardware Constraints

### Processing

Field devices in our target context are not running Snapdragon 8 Gen 3. They're running:
- ARM Cortex-A7 or A53 (single or dual core)
- 256MB–1GB RAM
- eMMC or NAND flash (not UFS)
- No GPU (or a trivial one irrelevant to our workload)

This means: Outstack's sampling loop and mode state machine are the primary compute workload during idle periods. The daemon must be small, fast, and frugal. No JVM. No Python. C with static allocation where possible.

Memory budget for `outstack-powerd` on a field device: target <2MB RSS. The daemon holds the mode state, the process class table (maybe 20-30 entries on a field device vs. hundreds on a phone), the threshold configuration, and a small buffer for pending ledger writes. This is achievable.

### No-Network Operation

The device must operate indefinitely without network connectivity. This means:

1. **Ledger writes are local.** `telux-ledgerd` writes to local storage. Records accumulate until a network window allows sync. Outstack's power records are no exception — they're written locally and synced later.

2. **No time sync.** Without network, the RTC is the only time source. RTC drift over weeks matters for record ordering. Outstack records should use both wall_ts (RTC) and lamport_ts (logical ordering). When network returns, NTP correction is applied and recorded as a CALIBRATE-equivalent event for time.

3. **No remote mode override.** A phone might receive a push notification that triggers a mode assessment. A field device can't receive anything when there's no network. Mode is determined entirely by local state: battery, thermal, solar input.

**Question:** Should Outstack on a field device have a "network available" signal that temporarily adjusts mode? If the cellular modem suddenly connects, the device might want to enter a "transmission burst" mini-mode where it prioritizes uploading accumulated data. This isn't a new mode — it might just be a BACKGROUND process (the upload daemon) getting scheduled because its maximum staleness has been reached and conditions now permit.

### Solar/Battery Hybrid Power

The canonical field device power architecture:

```
┌──────────────────┐
│   Solar Panel    │   20W typical, varies with angle/cloud/dust
│   (fixed mount)  │
└────────┬─────────┘
         │ Voc ~20V
┌────────▼─────────┐
│  Charge          │   MPPT controller
│  Controller      │   Regulates solar input to battery charge
└────────┬─────────┘
         │ Battery voltage (11-14.4V for 12V nominal)
┌────────▼─────────┐
│  Battery Pack    │   LiFePO4 or SLA, 20-100Ah
│                  │   Cycle-tolerant chemistry
└────────┬─────────┘
         │
┌────────▼─────────┐
│  DC-DC Regulator │   Steps down to 5V/3.3V for electronics
│                  │
└────────┬─────────┘
         │
┌────────▼─────────┐
│  Compute Module  │   SBC or SoM running Linux + Outstack
│  + Peripherals   │   Modem, sensors, storage
└──────────────────┘
```

**Key insight for Outstack:** The battery percentage on a field device is not a simple linear gauge. Solar input means the battery voltage (and thus percentage) fluctuates throughout the day. At noon, the battery might read 95% because solar is pouring in. At midnight, it might read 60% because 6 hours of darkness has drained it. By 6am (dawn), it might be at 45%.

Outstack's threshold model handles this correctly — it's percentage-based, so it doesn't care why the percentage is what it is. But the *hysteresis* matters more here. Without hysteresis, a device at dawn might oscillate between Conservation (45%) and Standard (crossing back above 50% as solar kicks in). The 3% hysteresis band prevents this — but only barely. A field device might want a wider hysteresis band (5-8%) to account for the slower, more gradual solar recharge curve.

**Distribution config implication:** Field device distributions should set HYSTERESIS_BAND higher than phone distributions. Maybe 5% minimum.

### Thermal Management in Hot Climates

A sealed enclosure in direct sun in Zambia can reach 60-70°C internal temperature. The compute module's maximum junction temperature is typically 85-105°C, but sustained operation above 70°C degrades battery life and reliability.

Outstack's thermal override (Emergency at 45°C sustained) needs adjustment for field devices:
- The compute module itself can tolerate higher temperatures (it's designed for industrial range)
- The battery cannot — LiFePO4 degrades above 45°C charge temperature, though it tolerates higher discharge temperatures
- The issue is not "the CPU is overheating" but "the battery is cooking"

**Architecture decision:** On a field device, thermal monitoring should track battery temperature specifically, not just CPU thermal zone. Most battery fuel gauges report temperature. The PMIC or charge controller may also report ambient. Outstack needs to read *battery* thermal, not just SoC thermal.

For the QM215 phone (CAT S22), this distinction is less important because everything is in one tightly-coupled thermal domain — if the CPU is hot, the battery is hot. On a field device with physical separation between compute module and battery pack, they can diverge.

**Proposed field device thermal hierarchy:**
1. Battery temp > 50°C → pause charging (charge controller handles this, but Outstack should also enter Conservation to reduce draw)
2. Battery temp > 55°C → enter Critical Reserve (reduce all draw to minimum)
3. CPU temp > 80°C → enter Emergency (reduce CPU to absolute minimum)
4. Battery temp > 60°C → enter Emergency + initiate controlled shutdown

These are different thresholds than the phone profile. Per-distribution thermal configuration is essential.

---

## Hardware Power Domains and Software Governance

### The Mapping Problem

On a phone, "power domains" are SoC-internal: CPU power domain, GPU power domain, modem power domain, WiFi power domain. The kernel's runtime PM and the SoC's PMIC manage these. Software (including Outstack) influences them indirectly by starting/stopping processes that use the hardware.

On a field device, power domains are *physical*: the cellular modem might be on a separate power rail with a GPIO-controlled FET. The sensor bus might be on another switchable rail. The compute module itself might have a "deep sleep" pin that cuts it to microamps.

This means Outstack on a field device can *directly* control hardware power domains, not just influence them through process management:

```
Outstack Mode → Process Gating (same as phone)
                   ↓
             + Power Domain Control (field device specific)
                   ↓
             GPIO pins → FET switches → hardware rails
```

### Example Power Domain Map (Field Device)

| Domain | Control | Quiescent Draw | Active Draw | Outstack Control |
|--------|---------|----------------|-------------|------------------|
| Compute (always on) | Main regulator | 200mW | 2W | CPU governor only |
| Cellular modem | GPIO + enable pin | 0mW (off) / 50mW (PSM) | 2W (Tx) | On/Off by mode |
| Sensor bus (I2C/SPI) | GPIO FET | 0mW (off) | 100mW | On/Off by sampling schedule |
| Status LED | GPIO | 0mW | 50mW | On/Off by mode |
| External storage | GPIO FET | 0mW | 300mW | On when writing |

**The governance-power domain mapping:**
- Full Power: all domains energized
- Standard: all domains energized, modem in eDRX
- Conservation: sensor bus powered only during sampling windows, modem in deep eDRX, LED off
- Critical Reserve: sensor bus powered only during sampling, modem powered only during scheduled comm windows, LED off
- Emergency: only compute + periodic sensor sample. Modem off. Record locally. Hope for sun.

### Interface to Outstack

How does `outstack-powerd` control GPIO-switched power domains? Options:

1. **Direct GPIO sysfs.** `/sys/class/gpio/gpioN/value`. Simple, standard, works. But: requires pinmux configuration and awareness of board-specific GPIO assignments.

2. **Linux regulator framework.** If the power rails are modeled as regulators in the device tree, `outstack-powerd` can use the regulator consumer interface. More abstract, more portable. But: requires device tree work for each board.

3. **Custom power domain driver.** A small kernel module that exposes field-device power domains via a standardized sysfs interface (e.g., `/sys/class/outstack_power/modem/state`). Outstack reads/writes this regardless of underlying GPIO assignment.

**Decision:** Option 3 is the cleanest interface, but it requires a kernel module (which we're trying to avoid). Option 1 is the most pragmatic for v1. The GPIO assignments go in the distribution configuration, and `outstack-powerd` reads them from config:

```ini
[power_domains]
modem_enable_gpio = 47
sensor_bus_gpio = 52
led_gpio = 33
external_storage_gpio = 61
```

This is a field-device-specific config section. Phone distributions don't have it. Phone distributions control hardware domains indirectly through process management and kernel runtime PM. Field device distributions control them directly.

---

## Operational Patterns

### The Sampling-Transmit Cycle

Most field devices follow a pattern:
1. Sample sensors periodically (every 1-60 minutes depending on application)
2. Accumulate samples locally
3. Transmit accumulated data periodically (every 1-24 hours depending on link budget and data urgency)

Outstack governs this cycle:
- The sensor sampling daemon is CRITICAL (its purpose is the device's purpose)
- The local storage writer is CRITICAL (data must be preserved)
- The transmission daemon is BACKGROUND or DEFERRED (can wait for good conditions)

In Conservation and below, the transmission daemon is gated. Data accumulates. When mode lifts (solar recharge pushes battery above STANDARD_FLOOR + HYSTERESIS), the transmission daemon runs and uploads the backlog.

**Edge case:** What if the storage fills before transmission can occur? This is a data management problem, not a power governance problem — but Outstack should be aware of it. If local storage exceeds a threshold, the transmission daemon might need to be promoted to INTERACTIVE temporarily (a distribution-specific rule that overrides the mode gating for the transmission process when storage pressure is high).

### The Comm Window Pattern

For satellite-connected devices, comm windows are scheduled and brief:
1. Modem powers on 30 seconds before window
2. Device transmits during window (2-10 minutes)
3. Modem powers off after window

Outstack must not gate the modem power-on if a comm window is imminent. This suggests a "scheduled reservation" mechanism:

```
task_code   = 0x06   SUSPEND (but inverted — a RESERVE?)
value       = comm_window_start_epoch
```

Or more simply: the comm window scheduler is a CRITICAL process that controls modem power directly. It's always permitted to run (CRITICAL class). When its timer fires, it energizes the modem, runs the transmission, de-energizes the modem. Outstack doesn't need to know about the schedule — it just needs to not gate a CRITICAL process. Which it won't, by definition.

This is cleaner. No schedule-aware mode exceptions needed.

---

## The Relationship Between Hardware and Governance

### Key Insight

On a phone, Outstack operates above the hardware: it governs processes, and the kernel manages hardware power domains in response to process activity (or inactivity).

On a field device, Outstack operates *alongside* the hardware: it governs processes AND directly controls hardware power domains. The separation between "software governance" and "hardware power management" is thinner.

This doesn't break the model — it extends it. The process class framework remains the same. The mode state machine remains the same. What changes is the enforcement mechanism: on a phone, enforcement is SIGSTOP + cgroup freezer. On a field device, enforcement is SIGSTOP + cgroup freezer + GPIO power rail control.

### Implication for Daemon Design

`outstack-powerd` needs a platform abstraction layer:

```
┌─────────────────────────────────────────────┐
│  outstack-powerd core                       │
│  (mode state machine, class gating, ledger) │
└────────────────────┬────────────────────────┘
                     │ platform API
┌────────────────────▼────────────────────────┐
│  Platform HAL                               │
│  ┌──────────────┐  ┌──────────────────────┐ │
│  │ Phone HAL    │  │ Field Device HAL     │ │
│  │ - sysfs power│  │ - sysfs power        │ │
│  │ - cgroup only│  │ - cgroup + GPIO rail │ │
│  │ - Android PM │  │ - direct HW control  │ │
│  └──────────────┘  └──────────────────────┘ │
└─────────────────────────────────────────────┘
```

The core logic is identical. The platform HAL provides:
- `read_battery_state()` — returns percentage, voltage, temperature, charging state
- `read_thermal_state()` — returns relevant temperature readings
- `gate_process_class(class)` — stops processes in the given class
- `restore_process_class(class)` — resumes processes in the given class
- `set_power_domain(domain, state)` — energize/de-energize a hardware power domain (no-op on phone HAL)
- `set_cpu_governor(governor)` — configure CPU frequency scaling

---

## Failure Modes Specific to Field Devices

### Solar Controller Failure

If the charge controller fails, the battery stops charging. Outstack sees: battery percentage declining, direction=0 (discharging) even during daylight hours. It progresses through modes normally. The device eventually dies.

Outstack can't fix this. But it can *record* it. The power ledger shows a pattern: POWER_CHANGE records with direction=0 continuously, no direction=1 periods even during expected solar hours. A remote operator reviewing the ledger (once comms resume) can diagnose "solar charge failure" from the record pattern.

### Battery Degradation

Over months/years, LiFePO4 capacity degrades. A battery that was 100Ah new might be 80Ah after 2 years. The fuel gauge might still report "100%" when full — but "100%" now means 80Ah, not 100Ah.

Outstack handles this naturally: it operates on percentage, not absolute capacity. If the battery degrades, the percentage-based thresholds still work correctly — they just represent less absolute energy. The device's time-in-mode at each level decreases proportionally.

The CALIBRATE record (task_code 0x04) exists for this: when a full charge cycle completes (charge to full, discharge to a known reference point), Outstack can recalibrate its understanding of capacity and record the calibration.

### Sensor Stuck High (Temperature)

If the temperature sensor fails and reads a fixed high value (stuck bit), Outstack's thermal override fires permanently. The device enters Emergency mode and stays there — even though the actual temperature is fine.

Mitigation: require sustained high temperature (the spec says 60 seconds). A stuck-high sensor will still trigger this, though. Better mitigation: if the mode is Emergency-thermal AND the battery percentage is high AND there are no other indicators of thermal stress (CPU throttling kernel events, etc.), flag this as a potential sensor fault and log a diagnostic record. Don't override the safety mechanism — but record that it might be wrong.

---

## Summary: What the Field Device Needs From the Spec

1. **Per-distribution power domain control** via GPIO config section
2. **Wider hysteresis** bands for solar-buffered systems
3. **Battery-specific thermal thresholds** (not just CPU thermal zone)
4. **Platform HAL abstraction** so the core daemon works on both phone and field device
5. **CRITICAL class for the device's primary function** (sensor sampling, not communication)
6. **Local-only operation** as the normal case, not a degraded case
7. **Storage pressure awareness** as a factor in transmission scheduling (per-distribution policy)
8. **Solar energy balance** signal in addition to simple charging direction

---

*This feeds into the distribution configuration spec (§9) and the conformance requirements. Field device distributions are first-class citizens, not afterthoughts.*
