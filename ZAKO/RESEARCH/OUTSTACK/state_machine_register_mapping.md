# State Machine to Register Mapping — QM215 Platform

**Date:** 2025-02-12
**Purpose:** Bridge between the abstract five-mode state machine and the concrete hardware/kernel interfaces available on the QM215 (MSM8937) platform as deployed on the CAT S22 Flip

---

## Overview

The Outstack state machine operates on abstract concepts: "battery percentage," "temperature," "CPU governor," "process gating." On real hardware, these translate to specific sysfs paths, kernel interfaces, hardware registers, and control mechanisms. This document maps each abstract concept to its concrete implementation on our target platform.

Target hardware:
- SoC: Qualcomm MSM8937 (QM215 variant)
- CPU: 4× Cortex-A53 @ 1.4GHz + 4× Cortex-A53 @ 1.0GHz (big.LITTLE config on MSM8937; QM215 variant may be 4×A53 only — need to verify on actual device)
- Battery: 1450mAh Li-ion, single cell
- Fuel gauge: QTI QPNP (PMIC-integrated, via SPMI bus)
- PMIC: PM8937 (or PMI8937 for charger variant)
- Thermal: QTI thermal-engine (multiple thermal zones)
- Modem: MDM9x07 (integrated, shared die)

---

## 1. Battery State — Fuel Gauge Interface

### sysfs Paths

```
/sys/class/power_supply/battery/capacity          → integer 0-100 (percentage)
/sys/class/power_supply/battery/voltage_now        → integer (microvolts)
/sys/class/power_supply/battery/current_now        → integer (microamps, signed: negative=discharging)
/sys/class/power_supply/battery/temp              → integer (tenths of degree C: 250 = 25.0°C)
/sys/class/power_supply/battery/status            → string: "Charging", "Discharging", "Full", "Not charging"
/sys/class/power_supply/battery/health            → string: "Good", "Overheat", "Cold", etc.
/sys/class/power_supply/battery/technology        → string: "Li-ion"
/sys/class/power_supply/battery/charge_full       → integer (microamp-hours, design capacity)
/sys/class/power_supply/battery/charge_full_design → integer (microamp-hours, original capacity)
```

### Charger Information

```
/sys/class/power_supply/usb/online                → 0 or 1
/sys/class/power_supply/usb/type                  → "USB", "USB_DCP", "USB_CDP", "USB_PD"
/sys/class/power_supply/usb/current_max           → integer (microamps — max input current)
/sys/class/power_supply/usb/voltage_max           → integer (microvolts — negotiated voltage)
```

### How outstack-powerd Uses These

```c
// Pseudocode for battery state reading
struct battery_state {
    int capacity_pct;       // from capacity
    int voltage_uv;         // from voltage_now
    int current_ua;         // from current_now (negative = discharge)
    int temp_decidegc;      // from temp
    bool charging;          // derived from status
    int charge_rate_mw;     // computed: (voltage_uv/1000000) * (current_ua/1000000) * 1000
};

// Read cycle (every SAMPLE_INTERVAL):
// 1. Read capacity → compare against mode thresholds
// 2. Read temp → compare against THERMAL_CRITICAL
// 3. Read status → update direction bit for records
// 4. If capacity_delta > SAMPLE_THRESHOLD → write POWER_CHANGE record
// 5. If threshold crossed → initiate mode transition
```

### Fuel Gauge Accuracy Notes

The QM215's PMIC-integrated fuel gauge uses a coulomb counter + voltage-based correction. Known issues:
- At very low battery (<5%), the voltage curve is steep and small measurement errors produce large percentage swings. This is why we have confirmation samples before mode transitions.
- Temperature affects the voltage-to-capacity mapping. A cold battery reads lower percentage than actual. The fuel gauge driver *should* compensate, but on some firmware versions it doesn't fully.
- After deep discharge + recharge, the fuel gauge may need a full charge cycle (CALIBRATE event) to re-sync its learned capacity model.

**Question:** Should outstack-powerd read `charge_full` vs `charge_full_design` to detect battery degradation? If `charge_full` drops significantly below `charge_full_design`, the battery is aging. This is informational (write a CALIBRATE record) but shouldn't change threshold behavior — thresholds are percentage-based, and the fuel gauge's percentage already accounts for reduced capacity.

---

## 2. Thermal Zones — Temperature Monitoring

### sysfs Paths

The QM215 exposes multiple thermal zones. On a typical MSM8937 build:

```
/sys/class/thermal/thermal_zone0/type     → "tsens_tz_sensor0" (CPU cluster 0)
/sys/class/thermal/thermal_zone0/temp     → integer (millidegrees C: 42000 = 42°C)

/sys/class/thermal/thermal_zone1/type     → "tsens_tz_sensor1" (CPU cluster 1)
/sys/class/thermal/thermal_zone1/temp     → integer

/sys/class/thermal/thermal_zone2/type     → "tsens_tz_sensor2" (GPU)
/sys/class/thermal/thermal_zone2/temp     → integer

/sys/class/thermal/thermal_zone3/type     → "tsens_tz_sensor3" (modem)
/sys/class/thermal/thermal_zone3/temp     → integer

/sys/class/thermal/thermal_zone4/type     → "battery" (from PMIC)
/sys/class/thermal/thermal_zone4/temp     → integer

# Additional zones vary by device tree configuration
# The CAT S22 may have additional zones or different numbering
```

### Which Zone(s) Does outstack-powerd Monitor?

Multiple zones are relevant:

| Zone | Relevance to Outstack | Action |
|------|----------------------|--------|
| CPU cluster 0 | Primary computation thermal | Thermal override trigger |
| CPU cluster 1 | Secondary (if present) | Thermal override trigger |
| Battery | Battery safety | Thermal override trigger (different thresholds) |
| Modem | Modem thermal (affects radio performance) | Informational |

**Design decision:** Monitor ALL CPU zones + battery zone. Thermal override triggers if ANY monitored zone exceeds its threshold for the sustained period.

```c
// Thermal monitoring (every SAMPLE_INTERVAL)
#define MAX_THERMAL_ZONES 8

struct thermal_reading {
    int temp_mc;            // millidegrees C
    const char *zone_type;  // zone name
    bool critical;          // above threshold?
};

// Thermal override check:
// For each monitored zone:
//   if temp > THERMAL_CRITICAL_MC:
//     increment sustained_counter for this zone
//     if sustained_counter > SUSTAIN_SAMPLES (60s / SAMPLE_INTERVAL):
//       TRIGGER THERMAL OVERRIDE
//   else:
//     reset sustained_counter
```

### Qualcomm thermal-engine Interaction

The stock QM215 build includes Qualcomm's `thermal-engine` daemon, which independently monitors thermal zones and can throttle CPU frequency, shut down cores, or limit charging current. This operates below Outstack.

**Coexistence strategy:** Let thermal-engine do hardware-level thermal management (frequency throttling, charging current limits). Outstack adds governance-level thermal management (mode transition to Emergency). They don't conflict — thermal-engine protects the hardware; Outstack protects the user's power budget.

If thermal-engine throttles the CPU, `outstack-powerd` doesn't need to know. If the temperature is high enough for `outstack-powerd` to trigger Emergency, thermal-engine is already doing its hardware-level thing underneath.

**Question:** Should we disable thermal-engine and let Outstack handle everything? Probably not. thermal-engine has per-device calibration data (trip points, mitigation frequencies) from Qualcomm's thermal characterization of the SoC. Reimplementing this in Outstack would be error-prone. Let the specialist do the hardware mitigation; Outstack does the governance.

---

## 3. CPU Frequency Governor — Performance Control

### sysfs Paths

```
# Per-CPU frequency control
/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor        → r/w: current governor
/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors → r: list
/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq        → r: current frequency (kHz)
/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq        → r/w: floor frequency
/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq        → r/w: ceiling frequency
/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq        → r: hardware minimum
/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq        → r: hardware maximum

# Repeat for cpu1, cpu2, cpu3 (and cpu4-7 if 8-core variant)

# Available governors on QM215 (typical):
# "performance schedutil powersave ondemand conservative userspace"
```

### Core Online Control (Core Parking)

```
/sys/devices/system/cpu/cpu1/online    → 0 or 1 (r/w)
/sys/devices/system/cpu/cpu2/online    → 0 or 1 (r/w)
/sys/devices/system/cpu/cpu3/online    → 0 or 1 (r/w)
# cpu0 cannot be offlined (boot CPU)
```

### Mode-to-Governor Mapping

| Outstack Mode | Governor | Max Freq | Cores Online | Rationale |
|--------------|----------|----------|--------------|-----------|
| Full Power | schedutil | cpuinfo_max | All | Maximum performance available |
| Standard | schedutil | cpuinfo_max | All | Still responsive; schedutil manages dynamically |
| Conservation | powersave | 80% of max? | All or 3 | Reduce peak draw; powersave locks to minimum |
| Critical Reserve | powersave | cpuinfo_min | 2 | Minimum computation; only essential work |
| Emergency | powersave | cpuinfo_min | 1 | Single core, minimum frequency |

**Wait — powersave governor locks to scaling_min_freq.** That might be too aggressive for Conservation, where INTERACTIVE processes still run and need responsiveness. Let me reconsider:

Revised mapping:
| Mode | Governor | scaling_max_freq | Cores Online |
|------|----------|-----------------|--------------|
| Full | schedutil | cpuinfo_max (1.4GHz) | 4 |
| Standard | schedutil | cpuinfo_max | 4 |
| Conservation | schedutil | 1.0GHz (reduced ceiling) | 4 |
| Critical | powersave | cpuinfo_min (800MHz?) | 2 |
| Emergency | powersave | cpuinfo_min | 1 |

Using schedutil with a reduced scaling_max_freq in Conservation gives responsive frequency scaling (good for INTERACTIVE) but caps peak power draw. In Critical and Emergency, powersave is appropriate because the remaining work (CRITICAL processes) is mostly I/O-bound (ledger writes, modem communication), not CPU-bound.

### Implementation

```c
// Set governor for all online CPUs
void set_cpu_governor(const char *governor) {
    for (int i = 0; i < num_cpus; i++) {
        if (!cpu_is_online(i)) continue;
        char path[256];
        snprintf(path, sizeof(path), 
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
        write_sysfs(path, governor);
    }
}

// Set max frequency ceiling
void set_cpu_max_freq(int max_khz) {
    for (int i = 0; i < num_cpus; i++) {
        if (!cpu_is_online(i)) continue;
        char path[256];
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
        write_sysfs_int(path, max_khz);
    }
}

// Core parking
void park_cores(int keep_online) {
    // Never offline cpu0
    for (int i = num_cpus - 1; i >= 1; i--) {
        if (i < keep_online) {
            write_sysfs("/sys/devices/system/cpu/cpu%d/online", 1);
        } else {
            write_sysfs("/sys/devices/system/cpu/cpu%d/online", 0);
        }
    }
}
```

---

## 4. Process Gating — cgroup and Signal Interfaces

### cgroup v1 Hierarchy (Android/QM215 uses cgroup v1)

Android on the QM215 uses cgroup v1 with the following controllers relevant to Outstack:

```
/dev/cpuset/                          → CPU affinity control
/dev/cpuset/foreground/               → foreground tasks
/dev/cpuset/background/               → background tasks
/dev/cpuset/system-background/        → system background tasks

/sys/fs/cgroup/freezer/               → cgroup freezer (if available)
/sys/fs/cgroup/freezer/outstack_gated/  → gate target (we create this)

/dev/cpuctl/                          → CPU bandwidth control
```

### Freezer-Based Gating

The cgroup freezer is the primary gating mechanism. When a process class is gated:

```c
// Create freezer cgroup for gated class (at startup)
// mkdir /sys/fs/cgroup/freezer/outstack_background
// mkdir /sys/fs/cgroup/freezer/outstack_deferred
// mkdir /sys/fs/cgroup/freezer/outstack_opportunistic
// mkdir /sys/fs/cgroup/freezer/outstack_interactive

// Move process to freezer cgroup
void move_to_freezer(pid_t pid, const char *class_name) {
    char path[256];
    snprintf(path, sizeof(path), 
             "/sys/fs/cgroup/freezer/outstack_%s/cgroup.procs", class_name);
    write_sysfs_int(path, pid);
}

// Freeze a class (gate it)
void freeze_class(const char *class_name) {
    char path[256];
    snprintf(path, sizeof(path),
             "/sys/fs/cgroup/freezer/outstack_%s/freezer.state", class_name);
    write_sysfs(path, "FROZEN");
}

// Thaw a class (restore it)
void thaw_class(const char *class_name) {
    char path[256];
    snprintf(path, sizeof(path),
             "/sys/fs/cgroup/freezer/outstack_%s/freezer.state", class_name);
    write_sysfs(path, "THAWED");
}
```

### Fallback: SIGSTOP/SIGCONT

If the cgroup freezer is not available (some kernel configs don't include it, or cgroup v2 migration changes the interface):

```c
// SIGSTOP-based gating (fallback)
void gate_process_signal(pid_t pid) {
    kill(pid, SIGSTOP);
}

void restore_process_signal(pid_t pid) {
    kill(pid, SIGCONT);
}
```

SIGSTOP is less atomic than the freezer — there's a window between the signal being sent and the process actually stopping. The freezer is instantaneous for all processes in the cgroup simultaneously.

### cgroup v2 Considerations

Android is migrating to cgroup v2. On cgroup v2, the freezer is integrated differently:

```
# cgroup v2 freezer
/sys/fs/cgroup/outstack_background/cgroup.freeze    → write "1" to freeze, "0" to thaw
/sys/fs/cgroup/outstack_background/cgroup.events    → read: "frozen 1" or "frozen 0"
```

`outstack-powerd` should detect which cgroup version is active at startup and use the appropriate interface. Both are simple sysfs writes — the abstraction is thin.

---

## 5. Radio/Modem Control — eDRX and PSM

### Modem Interface

The QM215's integrated modem is controlled via RIL (Radio Interface Layer). `outstack-powerd` doesn't talk to the modem directly — it communicates with the RIL daemon, which translates to QMI messages for the modem firmware.

```
# Modem power state is not directly in sysfs
# RIL commands are issued via the telephony framework or via:
/dev/smd0              → QMI shared memory device (primary control channel)
/dev/smd7              → AT command channel (secondary/debug)
```

### eDRX Configuration

eDRX (Extended Discontinuous Reception) reduces modem power by extending the paging cycle. The modem wakes less frequently to check for incoming pages.

Configuration is via AT commands or QMI messages through the RIL:

```
# AT command approach (via /dev/smd7 if accessible):
AT+CEDRXS=1,4,"0010"    → enable eDRX for LTE, cycle value "0010"
AT+CEDRXRDP             → read current eDRX parameters

# The actual cycle values are 4-bit codes:
# "0000" = 5.12s
# "0001" = 10.24s
# "0010" = 20.48s
# "0011" = 40.96s
# "0100" = 61.44s
# "0101" = 81.92s (5.12s × 16)
# "1001" = 327.68s (maximum on many networks)
```

### Mode-to-Radio Mapping

| Mode | eDRX Setting | Effect |
|------|-------------|--------|
| Full | Disabled (or minimum: 5.12s) | Maximum responsiveness to incoming |
| Standard | Enabled, cycle=20.48s | Slight delay on incoming, significant power save |
| Conservation | Enabled, cycle=81.92s | ~82s max delay on incoming pages |
| Critical | Enabled, cycle=327.68s | ~5.5 min max delay on incoming |
| Emergency | Maximum eDRX + minimal PTW | Maximum power save, multi-minute response delay |

### Implementation Concern

`outstack-powerd` needs a way to request eDRX changes from the modem. Options:

1. **Via Android TelephonyManager API** — requires Java/JNI bridge. Ugly for a native daemon.
2. **Via RIL socket directly** — send RIL_REQUEST_SET_CARRIER_RESTRICTIONS or similar. Complex protocol.
3. **Via AT command on /dev/smd7** — simple text protocol. But: access to /dev/smd7 requires appropriate SELinux permissions and the port may not be configured for AT command mode on all builds.
4. **Via a helper service** — a thin Java service in the telephony process that listens for Outstack signals and applies eDRX changes. Most maintainable.

**Decision for v1:** Option 4. A thin "outstack-radio-helper" that runs in the telephony process, listens for MODE_ENTER C0 signals, and applies the eDRX configuration for the new mode. This keeps `outstack-powerd` purely native and avoids direct modem register manipulation from the power daemon.

---

## 6. Power Domain Control — RPM and SPMI

### The RPM (Resource Power Manager)

The QM215's RPM is a dedicated Cortex-M3 microcontroller that manages voltage regulators, clocks, and bus bandwidth. The Linux kernel communicates with it via SPMI (System Power Management Interface) through the `qcom,rpm-smd` driver.

```
# RPM-controlled resources visible via regulators:
/sys/class/regulator/                  → system voltage regulators

# Power domains (kernel runtime PM):
/sys/devices/platform/soc/*/power/runtime_status   → "active" or "suspended"
/sys/devices/platform/soc/*/power/control          → "auto" or "on"
```

### What outstack-powerd Can Control

`outstack-powerd` does NOT directly control power domains. The kernel's runtime PM framework handles this automatically:
- When WiFi daemon is stopped → WiFi chip goes to runtime suspend → RPM reduces WCNSS power domain
- When no display clients → display power domain suspends
- When CPU is idle → kernel cpuidle governor negotiates sleep with RPM

Outstack's role is to create the *conditions* for power domain suspension by gating the processes that use those domains. The kernel and RPM do the actual hardware power management.

```
Outstack gates BACKGROUND processes
  → WiFi sync daemon stops
    → No WiFi traffic
      → WiFi driver enters suspend
        → Kernel runtime PM signals RPM
          → RPM reduces WCNSS power rail
            → Hardware power saved
```

This indirect control is by design. `outstack-powerd` doesn't need `CAP_SYS_RAWIO` or SPMI access. It just needs to stop the right processes at the right time, and the kernel stack does the rest.

### Exception: Display Backlight

The display backlight is one domain Outstack might control directly, because screen brightness has massive power impact and the standard Android brightness control may not respect mode transitions:

```
/sys/class/backlight/panel0-backlight/brightness     → integer 0-255
/sys/class/backlight/panel0-backlight/max_brightness  → integer (read max)
/sys/class/leds/lcd-backlight/brightness             → alternative path on some builds
```

In Conservation mode, Outstack might cap brightness:
```c
void cap_backlight(int max_pct) {
    int max_hw = read_sysfs_int("/sys/class/backlight/panel0-backlight/max_brightness");
    int cap = (max_hw * max_pct) / 100;
    write_sysfs_int("/sys/class/backlight/panel0-backlight/max_brightness", cap);
    // Note: this caps the MAX, not the current value. User can still adjust within the cap.
}
```

**Question:** Is capping brightness a governance action or a power management action? It's a bit of both. The user can't exceed the cap (governance), but the cap exists to save power (management). For the spec: treat brightness capping as a mode-specific display policy, documented in the distribution profile. Don't make it a core spec requirement — some deployments (IoT) don't have displays.

---

## 7. Ledger Interface — Writing Records

### How outstack-powerd Writes to telux-ledgerd

`outstack-powerd` doesn't write directly to disk. It submits records to `telux-ledgerd` via a Unix domain socket:

```
/var/run/telux/ledger.sock    → Unix domain socket (SOCK_STREAM)
```

Protocol: submit a BitPads frame as a binary blob. `telux-ledgerd` validates, assigns lamport_ts, and appends to the ledger file.

```c
struct power_record {
    uint8_t  task_code;      // 0x00-0x07 per spec
    uint8_t  account_pair;   // 4-bit encoding
    uint8_t  domain;         // 0x01 = Engineering
    uint32_t value;          // record-specific
    uint64_t wall_ts;        // epoch seconds
    uint8_t  sub_entity;     // 0 = System
    // ... other BitPads frame fields
};

int submit_record(struct power_record *rec) {
    // Serialize to BitPads wire format
    uint8_t frame[29];  // max full record size
    int len = serialize_bitpads_frame(rec, frame);
    
    // Submit to telux-ledgerd
    int sock = connect_unix("/var/run/telux/ledger.sock");
    write(sock, frame, len);
    
    // Wait for ACK (ledgerd confirms append)
    uint8_t ack;
    read(sock, &ack, 1);
    close(sock);
    
    return (ack == 0x01) ? 0 : -1;
}
```

### Write Frequency Considerations

At SAMPLE_INTERVAL=30s in Full mode, Outstack evaluates battery every 30 seconds. But POWER_CHANGE records are only written when delta > SAMPLE_THRESHOLD (5%). At typical drain rates (~12%/hour), that's one POWER_CHANGE record every ~25 minutes. MODE_CHANGE records are rare (a few per day). GATE/RESTORE are triggered by mode changes.

Total estimated record volume: ~50-100 records per day in normal use. Negligible write load on telux-ledgerd.

In Emergency mode, SAMPLE_THRESHOLD drops to 1%, and sampling is every 30s. At rapid discharge rates, this could produce a record every few minutes. Still manageable, and Emergency mode is brief.

---

## 8. Complete Mode Transition Implementation

Putting it all together — what happens inside `outstack-powerd` on a mode transition from Standard to Conservation:

```c
void transition_to_conservation(void) {
    // 1. Emit MODE_ENTER C0 signal
    emit_c0_signal(C0_SLOT_MODE_ENTER, MODE_CONSERVATION);
    
    // 2. Write MODE_CHANGE record
    struct power_record rec = {
        .task_code = TASK_MODE_CHANGE,
        .account_pair = 0x08,   // 1000 = Transformation
        .domain = DOMAIN_ENGINEERING,
        .value = (MODE_STANDARD << 4) | MODE_CONSERVATION,  // 0x12
        .wall_ts = time(NULL),
        .sub_entity = 0,
    };
    submit_record(&rec);
    
    // 3. Write GATE record
    uint8_t gate_mask = CLASS_BACKGROUND | CLASS_DEFERRED | CLASS_OPPORTUNISTIC;
    struct power_record gate_rec = {
        .task_code = TASK_GATE,
        .account_pair = 0x01,   // 0001 = Parent→Child
        .domain = DOMAIN_ENGINEERING,
        .value = gate_mask,
        .wall_ts = time(NULL),
        .sub_entity = 0,
    };
    submit_record(&gate_rec);
    
    // 4. Freeze BACKGROUND processes (allow suspension point first)
    signal_suspension_point("background");
    sleep(2);  // allow 2s for safe suspension
    freeze_class("background");
    
    // 5. Freeze DEFERRED + OPPORTUNISTIC (already frozen from Standard, but confirm)
    freeze_class("deferred");
    freeze_class("opportunistic");
    
    // 6. Adjust CPU governor
    set_cpu_governor("schedutil");
    set_cpu_max_freq(1000000);  // 1.0GHz ceiling
    
    // 7. Adjust display timeout
    set_display_timeout(20);
    
    // 8. Signal radio helper for eDRX adjustment
    emit_c0_signal(C0_SLOT_RADIO_CONFIG, RADIO_CONSERVATION);
    
    // 9. Update internal state
    current_mode = MODE_CONSERVATION;
    
    // 10. Write per-process SUSPEND records
    for_each_frozen_process("background", write_suspend_record);
}
```

---

## Summary: Interface Map

| Abstract Concept | Kernel Interface | Path | Access |
|-----------------|-----------------|------|--------|
| Battery % | power_supply sysfs | /sys/class/power_supply/battery/capacity | read |
| Battery temp | power_supply sysfs | /sys/class/power_supply/battery/temp | read |
| Charging state | power_supply sysfs | /sys/class/power_supply/battery/status | read |
| Charge rate | power_supply sysfs | /sys/class/power_supply/usb/current_max × voltage | read |
| CPU temperature | thermal sysfs | /sys/class/thermal/thermal_zoneN/temp | read |
| CPU governor | cpufreq sysfs | /sys/devices/system/cpu/cpuN/cpufreq/scaling_governor | r/w |
| CPU max freq | cpufreq sysfs | /sys/devices/system/cpu/cpuN/cpufreq/scaling_max_freq | r/w |
| Core online | cpu sysfs | /sys/devices/system/cpu/cpuN/online | r/w |
| Process freeze | cgroup freezer | /sys/fs/cgroup/freezer/outstack_*/freezer.state | r/w |
| Process membership | cgroup | /sys/fs/cgroup/freezer/outstack_*/cgroup.procs | r/w |
| Backlight cap | backlight sysfs | /sys/class/backlight/*/max_brightness | r/w |
| Ledger write | Unix socket | /var/run/telux/ledger.sock | write |
| eDRX config | Radio helper | C0 signal to helper service | signal |

All read/write operations are standard file I/O on sysfs pseudofiles. No custom kernel interfaces. No ioctl. No SPMI direct access. The daemon requires:
- `CAP_KILL` (for SIGSTOP/SIGCONT fallback)
- write access to cgroup freezer hierarchy
- write access to cpufreq sysfs
- write access to cpu online sysfs
- read access to power_supply and thermal sysfs
- write access to backlight sysfs
- connect access to telux-ledgerd socket

All of this is expressible in SELinux policy without granting broad system access.

---

*This mapping is specific to the QM215/MSM8937 platform on kernel 4.9 CAF. Other platforms will have the same sysfs interfaces (they're standard Linux) but potentially different paths for platform-specific features. The abstraction layer in outstack-powerd should parameterize paths from the distribution configuration.*
