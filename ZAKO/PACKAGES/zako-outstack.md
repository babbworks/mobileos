# zako-outstack — Package Configuration Reference

The Outstack power governance daemon. Manages the five-mode power state machine, enforces process class gating, writes to the power ledger, and coordinates with the kernel's CPU governor and radio subsystems.

Normative specification: `ZAKO/PROTOCOLS/Outstack-Protocol-v1.md`

This document covers the build integration and configuration API — what distributions set in their device tree to configure the package.

---

## Build Integration

```makefile
# In device.mk:
PRODUCT_PACKAGES += ZakoOutstack

# Copy distribution config files into system image:
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/zako/outstack/outstack.conf:system/etc/zako/outstack.conf \
    $(LOCAL_PATH)/zako/outstack/outstack-policy.xml:system/etc/zako/outstack-policy.xml
```

The package provides its own SELinux policy (`outstack.te`). If the device tree adds custom process classes or new daemon entries, add supplementary rules to `device/[vendor]/[codename]/sepolicy/outstack_device.te`.

---

## outstack.conf

Runtime configuration file. Read at daemon startup; reload via `outstack-ctl reload` without restarting.

### [modes] section

Defines the five power modes and the battery percentage thresholds at which transitions fire.

```ini
[modes]
# Battery thresholds — mode is entered when battery falls to or below the upper bound
# and exits when battery rises above it (with hysteresis defined in [hysteresis])
full_power_min    = 80   # Full Power: 80–100%
standard_min      = 40   # Standard: 40–79%
conservation_min  = 20   # Conservation: 20–39%
critical_min      = 5    # Critical Reserve: 5–19%
# Emergency: <5% (no floor — always active below critical_min)

# Hysteresis: mode will not re-enter until battery rises this many points above threshold
# Prevents rapid oscillation at threshold boundaries
hysteresis        = 3
```

### [governors] section

CPU frequency governor per mode. Must match a governor available in the kernel (`cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors`).

```ini
[governors]
full_power    = schedutil
standard      = schedutil
conservation  = powersave
critical      = powersave
emergency     = powersave

# Core parking: park idle cores in modes below standard
core_park_threshold  = standard   # park in conservation, critical, emergency
core_park_min_online = 1          # always keep at least 1 core online
```

### [display] section

Screen timeout (seconds before screen-off) per mode.

```ini
[display]
full_power    = 60
standard      = 30
conservation  = 20
critical      = 10
emergency     = 5

# Prevent display-on entirely in emergency unless active call
emergency_display_on_call_only = true
```

### [radio] section

LTE eDRX and PSM (Power Saving Mode) configuration per mode. Values are passed to the RIL at mode entry.

```ini
[radio]
# eDRX: Extended Discontinuous Reception
# edrx_cycle: paging cycle in units of 10.24s (8192 = 327.68s maximum)
# ptw: paging time window in units of 10.24s

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

# PSM: device requests deep sleep from network between pages
# Only enable if carrier supports PSM (not all do)
psm_enable            = false
```

### [ledger] section

Power event ledger configuration.

```ini
[ledger]
# Path to the power ledger (BitPads format)
path          = /data/zako/ledger/power.bpd

# Write interval: ledger entry written every N seconds while mode is stable
write_interval = 300

# Always write on mode transition
write_on_transition = true
```

---

## outstack-policy.xml

Assigns every daemon, service, and application to one of five process classes. The daemon enforces these assignments — processes in a class that is gated in the current mode are suspended (SIGSTOP) or killed (SIGKILL) per the kill_on_gate field.

### Process Classes

| Class | Description | Gated in modes |
|---|---|---|
| `system-critical` | Must never be interrupted | Never gated |
| `communication` | Telephony, SMS, Telux exchange | Gated in Emergency (radio-standby only) |
| `user-active` | Foreground app | Gated in Critical, Emergency |
| `background` | Background sync, non-critical services | Gated in Conservation, Critical, Emergency |
| `deferred` | Low-priority work, analytics | Gated in Standard, Conservation, Critical, Emergency |

### XML Format

```xml
<outstack-policy version="1">

  <!-- System Critical — never gated -->
  <process class="system-critical" kill_on_gate="false">
    <entry type="service" name="servicemanager" />
    <entry type="service" name="vold" />
    <entry type="service" name="keystore" />
    <entry type="service" name="gatekeeperd" />
    <entry type="service" name="outstack-governed" />   <!-- outstack itself -->
    <entry type="service" name="rild" />                <!-- modem RIL -->
    <entry type="service" name="netd" />
  </process>

  <!-- Communication — gated in Emergency -->
  <process class="communication" kill_on_gate="false">
    <entry type="service" name="telecom" />
    <entry type="service" name="phone" />
    <entry type="app"     package="com.android.phone" />
    <entry type="service" name="telux-ledgerd" />
    <entry type="service" name="telux-identd" />
    <entry type="service" name="telux-sharedb" />
    <entry type="app"     package="com.android.messaging" />
  </process>

  <!-- User Active — gated in Critical and Emergency -->
  <process class="user-active" kill_on_gate="false">
    <entry type="service" name="surfaceflinger" />
    <entry type="service" name="inputflinger" />
    <entry type="service" name="audioserver" />
    <entry type="app"     package="com.android.launcher3" />
    <entry type="app"     package="org.fdroid.fdroid" />
  </process>

  <!-- Background — gated in Conservation, Critical, Emergency -->
  <process class="background" kill_on_gate="false">
    <entry type="service" name="installd" />
    <entry type="app"     package="io.ente.photos" />
  </process>

  <!-- Deferred — gated in Standard and below -->
  <process class="deferred" kill_on_gate="true">
    <entry type="service" name="BackgroundDexOptService" />
    <entry type="service" name="StorageManagerService" />
  </process>

</outstack-policy>
```

### kill_on_gate

- `false` — process is suspended (SIGSTOP) when gated; resumed (SIGCONT) when ungated. State preserved.
- `true` — process is killed (SIGKILL) when gated; restarted by init when ungated. Use for idempotent background workers only.

---

## init.outstack.rc

Provided by the package. Starts `outstack-governed` at boot in the `late-init` trigger. Distributions do not normally need to modify this file. If a device-specific init sequence is required, override via the device tree's `init.[codename].rc`.

```
service outstack-governed /system/bin/outstack-governed
    class core
    user root
    group system radio
    writepid /dev/cpuset/foreground/tasks
    onrestart restart outstack-governed
```

---

## Runtime Control

```bash
# Query current mode:
outstack-ctl status

# Force a mode (testing only — overridden by next battery event):
outstack-ctl force-mode conservation

# Reload config without restart:
outstack-ctl reload

# Show process class assignments and current gate state:
outstack-ctl policy

# Show power ledger tail:
outstack-ctl ledger tail 20
```

---

## Distribution Override Notes

Distributions should override only what differs from the defaults. The minimum required configuration for a distribution is:

1. `[modes]` — battery thresholds for this device's battery size
2. `[governors]` — confirm governor names match what the kernel provides
3. `[display]` — screen timeouts appropriate to use context
4. `[radio]` — eDRX values appropriate to target carriers
5. `outstack-policy.xml` — process class assignments for distribution-specific daemons

Do not modify the package source to change behavior. If a class or gating rule genuinely cannot be expressed in the config, open an issue against `zako-outstack`.
