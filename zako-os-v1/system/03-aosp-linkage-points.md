# ZAKO OS v1 — AOSP Linkage Points

Every point where ZAKO touches, customizes, bridges, or replaces AOSP framework behavior. This is the complete integration surface between the ZAKO sovereign layer and the Android Open Source Project base.

---

## Classification

| Type | Meaning |
|------|---------|
| **ADDITION** | New component added alongside AOSP (no upstream modification). Zero rebasing cost. |
| **CONFIGURATION** | AOSP behavior modified via supported configuration (overlays, props, XML). Zero rebasing cost. |
| **BRIDGE** | JNI or socket interface between ZAKO native daemons and AOSP Java framework. Low rebasing cost. |
| **MODIFICATION** | Change to AOSP framework source code. Carries rebasing debt on every AOSP upgrade. Avoid where possible. |
| **REPLACEMENT** | AOSP component entirely replaced by ZAKO equivalent. Must maintain API compatibility. |

---

## 1. Native Daemons (ADDITION)

These are new system services that don't exist in AOSP. They are wired via `init.rc` and run alongside existing daemons.

| Daemon | init.rc class | SELinux domain | IPC | AOSP Dependency |
|--------|--------------|----------------|-----|-----------------|
| `outstack-powerd` | core | `outstack_daemon` | Unix socket + C0 signals | Reads: `/sys/class/power_supply/`, `/sys/class/thermal/`, cpufreq sysfs. Writes: cgroup freezer. |
| `telux-ledgerd` | core | `telux_ledger` | Unix socket (`/var/run/telux/ledger.sock`) | Reads/writes: `/data/zako/ledger/`. Uses SQLite (AOSP's bundled sqlite). |
| `telux-identd` | core | `telux_identity` | Unix socket | Calls: Keymaster 4.0 HAL for hardware-backed key operations. |
| `telux-sharedb` | main | `telux_share` | Unix socket | Uses: AOSP RIL for SMS send. Uses: BluetoothManager for BLE. Reads: connectivity state. |
| `telux-coverd` | main | `telux_cover` | Framebuffer or SPI device | Writes: cover display framebuffer (device-specific path). |

**Rebasing cost: ZERO.** These are pure additions. They live in `packages/services/Zako*/` and are included via `PRODUCT_PACKAGES`.

---

## 2. Kernel Interfaces (ADDITION + CONFIGURATION)

ZAKO daemons access kernel subsystems via standard sysfs/procfs interfaces. No kernel modification required for basic operation.

| Interface | Used By | Path | Access Type |
|-----------|---------|------|-------------|
| Battery state | outstack-powerd | `/sys/class/power_supply/battery/*` | Read |
| Charger state | outstack-powerd | `/sys/class/power_supply/usb/*` | Read |
| Thermal zones | outstack-powerd | `/sys/class/thermal/thermal_zone*/temp` | Read |
| CPU governor | outstack-powerd | `/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor` | Read/Write |
| CPU max freq | outstack-powerd | `/sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq` | Read/Write |
| Core online | outstack-powerd | `/sys/devices/system/cpu/cpu*/online` | Read/Write |
| cgroup freezer | outstack-powerd | `/sys/fs/cgroup/freezer/outstack_*/freezer.state` | Read/Write |
| Backlight | outstack-powerd | `/sys/class/backlight/*/brightness` | Read/Write |
| Lid sensor | outstack-powerd | `/dev/input/event*` (SW_LID) | Read (poll) |

**Rebasing cost: ZERO.** All standard Linux kernel interfaces, stable across 4.9 LTS.

### Future Kernel Modules (Phase 7)

| Module | Purpose | Rebasing Cost |
|--------|---------|---------------|
| Telux-SEC LSM | Island boundary enforcement at kernel level | MEDIUM — must track LSM hook API changes |
| Outstack exec gate | Process class check at `execve()` | MEDIUM — same as above |

These are deferred to Phase 7. They are not required for v1 functional operation.

---

## 3. SELinux Policy (ADDITION)

New policy files added to the device tree. No modification of AOSP's base `system/sepolicy/`.

| File | Contents | Location |
|------|----------|----------|
| `outstack.te` | Domain for outstack-powerd: access to power_supply, thermal, cpufreq, cgroups | Package repo |
| `telux_ledger.te` | Domain for telux-ledgerd: access to /data/zako/, sqlite, Unix sockets | Package repo |
| `telux_identity.te` | Domain for telux-identd: access to Keymaster HAL, /data/zako/identity/ | Package repo |
| `telux_share.te` | Domain for telux-sharedb: access to RIL, Bluetooth, network | Package repo |
| `telux_cover.te` | Domain for telux-coverd: access to SPI/framebuffer device | Package repo |
| `file_contexts` | Path labeling for /data/zako/ hierarchy | Device tree |
| `*_device.te` | Device-specific rule additions | Device tree |

**Rebasing cost: LOW.** SELinux policy is additive. AOSP's base policy is never modified. New domains are simply new `.te` files.

---

## 4. system.prop / Build Properties (CONFIGURATION)

AOSP behavior redirected via supported property mechanism. No source modification.

| Property | Purpose | Value |
|----------|---------|-------|
| `captive_portal_server` | Redirect connectivity check from Google | `204.babb.tel` |
| `captive_portal_http_url` | HTTP probe endpoint | `http://204.babb.tel/generate_204` |
| `captive_portal_https_url` | HTTPS probe endpoint | `https://204.babb.tel/generate_204` |
| `persist.sys.ntp_server` | Time sync away from Google | `africa.pool.ntp.org` |
| `net.dns1` / `net.dns2` | DNS away from 8.8.8.8 | `9.9.9.9` / `149.112.112.112` |
| `ro.error.receiver.default` | Clear Google crash reporter | (empty) |
| `ro.ril.edrx.enable` | Enable eDRX for power savings | `1` |
| `ro.ril.edrx.ptw` | eDRX paging time window | `2048` |
| `ro.ril.edrx.cycle` | eDRX cycle length | `8192` |
| `ro.setupwizard.mode` | Disable stock setup wizard | `DISABLED` |

**Rebasing cost: ZERO.** These are standard supported configuration points.

---

## 5. Resource Overlays (CONFIGURATION)

AOSP's overlay mechanism allows overriding resources (strings, configs, drawables) without modifying framework source.

| Overlay | Purpose | Location |
|---------|---------|----------|
| Carrier config | APN definitions for Airtel/MTN/Zamtel | `device/cat/S22FLIP/overlay/frameworks/base/core/res/` |
| Settings defaults | Default brightness, timeout, locale | `device/cat/S22FLIP/overlay/packages/apps/Settings/` |
| SystemUI config | Status bar, quick settings tile additions | `device/cat/S22FLIP/overlay/frameworks/base/packages/SystemUI/` |
| Telephony config | USSD allowlists, STK config, IMS settings | `device/cat/S22FLIP/overlay/packages/services/Telephony/` |

**Rebasing cost: ZERO.** Overlays are the supported customization mechanism.

---

## 6. Application Replacements (REPLACEMENT)

These AOSP/GMS apps are removed and replaced by ZAKO or open alternatives.

| Removed | Replaced By | Integration Notes |
|---------|-------------|-------------------|
| Google Setup Wizard | ZAKO First-Run Experience | Must implement `android.intent.action.DEVICE_INITIALIZATION_WIZARD` intent. Privileged app. |
| Gboard | AOSP LatinIME + custom T9 IME | T9 IME implements `InputMethodService`. Standard API. |
| Google Messages | AOSP Messaging (MMS) | Stock AOSP, no modification needed. |
| Chrome | Mull (Firefox fork) or Chromium (ungoogled) | Pre-built APK. No integration needed. |
| Google Maps | Organic Maps | Pre-built APK. Offline OSM. |
| GMS Push (FCM) | ntfy + UnifiedPush | ntfy as privileged app for background execution. UnifiedPush protocol. |
| Play Store | F-Droid | Privileged system app (`priv-app/`) for silent updates. Needs privileged permissions grant. |

**Rebasing cost: LOW.** Replacements are pre-built APKs dropped into system/priv-app. The Setup Wizard and T9 IME are custom apps using standard APIs.

---

## 7. JNI Bridge Interfaces (BRIDGE)

Where ZAKO native C daemons need to communicate with Android Java framework services.

| Bridge | Direction | Purpose | Mechanism |
|--------|-----------|---------|-----------|
| Outstack → PowerManager | Native → Java | Report power mode to framework (so framework respects mode) | System property broadcast or custom service |
| Outstack → ConnectivityService | Native → Java | Request eDRX change via telephony framework | `outstack-radio-helper` Java service listening for C0 signals |
| telux-sharedb → SmsManager | Native → Java | Send SMS for record transmission | JNI call to `android.telephony.SmsManager` or direct use of `/dev/smd*` via RIL |
| telux-sharedb → BluetoothManager | Native → Java | BLE GATT operations for record exchange | JNI call or dedicated BLE helper service |
| telux-identd → Keymaster | Native → HAL | Hardware-backed key operations | Direct HIDL/AIDL call to `android.hardware.keymaster@4.0` (C++ HAL interface, callable from C) |
| PADS app → telux-ledgerd | Java → Native | Write records to ledger from UI | Unix socket from Java (`LocalSocket`) to native daemon |
| Exchange app → telux-sharedb | Java → Native | Initiate outbound transmission | Unix socket |
| Exchange app → telux-ledgerd | Java → Native | Query ledger for display | Unix socket |

**Rebasing cost: LOW.** Unix sockets and system properties are stable APIs. The Keymaster HAL is a standard HIDL interface.

### Bridge Architecture Decision

Two approaches for Java↔Native communication:

**Option A: Unix domain sockets (PREFERRED)**
- Java uses `android.net.LocalSocket` to connect to daemon's Unix socket
- Protocol: submit BitPads frame bytes, receive ACK/response
- Advantages: simple, no JNI complexity, same interface as inter-daemon IPC
- Used for: ledger writes, ledger queries, share requests

**Option B: JNI shared library**
- Native `.so` loaded by Java app, exposes functions
- Advantages: lower latency for high-frequency calls
- Disadvantages: JNI is fragile, complicates build, ties native code to Java lifecycle
- Used for: NLQ engine (rule-based FSM needs tight integration with UI)

**Option C: Dedicated Java helper service**
- Small Java service running in system_server or standalone process
- Listens for signals from native daemon, translates to framework API calls
- Used for: eDRX changes (requires TelephonyManager), BLE operations (requires BluetoothManager)

---

## 8. Potential Framework Modifications (MODIFICATION — Avoid if Possible)

These are touch points where we MIGHT need to modify AOSP source. Each carries rebasing debt. Investigate alternatives first.

| Area | Reason | Alternative | Decision |
|------|--------|-------------|----------|
| PowerManagerService | Make framework aware of Outstack modes | Use system property `persist.zako.power_mode` — framework reads prop, no source change needed | **AVOID — use prop** |
| BatteryService | Prevent framework from doing its own power management that conflicts with Outstack | Disable stock battery saver via overlay/prop (`config_lowBatteryWarningLevel=0`) | **AVOID — use overlay** |
| AlarmManagerService | Ensure CRITICAL alarms fire even when framework thinks device should be in Doze | Outstack keeps `wakelock` for CRITICAL daemons; framework Doze is effectively bypassed because our daemons hold their own wakelocks | **AVOID — use wakelock** |
| ActivityManagerService | Process priority/OOM scoring for ZAKO daemons | Set `oom_score_adj` via init.rc `writepid` + cgroup assignment. No AMS modification. | **AVOID — use init.rc** |
| SystemUI | Add Outstack mode indicator to status bar | Use QS tile (standard API) or notification (standard API). No SystemUI source change. | **AVOID — use standard API** |
| Settings app | Add ZAKO settings panel | Separate settings app or PreferenceFragment injected via overlay intent | **AVOID — separate app** |

**Current assessment: ZERO framework modifications required for v1.** All ZAKO functionality can be achieved through additions, overlays, properties, and standard APIs.

---

## 9. Android Init Integration (ADDITION)

ZAKO daemons start via `init.rc` service entries. Standard mechanism.

```
# init.zako.rc (included from device/cat/S22FLIP/init.qcom.rc)

service outstack-powerd /system/bin/outstack-powerd
    class core
    user system
    group system radio power
    capabilities KILL SYS_NICE NET_ADMIN
    writepid /dev/cpuset/system-background/tasks
    oneshot

service telux-ledgerd /system/bin/telux-ledgerd
    class core
    user system
    group system
    writepid /dev/cpuset/foreground/tasks

service telux-identd /system/bin/telux-identd
    class core
    user keystore
    group keystore system

service telux-sharedb /system/bin/telux-sharedb
    class main
    user radio
    group radio inet bluetooth

service telux-coverd /system/bin/telux-coverd
    class main
    user graphics
    group graphics
```

**Rebasing cost: ZERO.** init.rc is device-tree-level configuration.

---

## 10. Data Directory Structure (ADDITION)

ZAKO creates its own data hierarchy. No conflict with AOSP paths.

```
/data/zako/
├── ledger/
│   ├── personal.db          ← SQLite database (telux-ledgerd)
│   └── power.db             ← Power ledger (outstack-powerd)
├── identity/
│   ├── sovereign.did         ← DID document
│   └── grants/               ← Active capability grants
├── islands/
│   ├── personal/             ← Personal Island records
│   └── work/                 ← Work Island records (PADS)
├── share/
│   ├── outbox/               ← Pending outbound records
│   └── inbox/                ← Inbound staging
└── config/
    ├── outstack.conf         ← Runtime copy
    └── telux.conf            ← Runtime copy
```

SELinux labels in `file_contexts`:
```
/data/zako(/.*)?                u:object_r:zako_data_file:s0
/data/zako/ledger(/.*)?         u:object_r:zako_ledger_file:s0
/data/zako/identity(/.*)?       u:object_r:zako_identity_file:s0
```

---

## Summary: Integration Surface

| Type | Count | Rebasing Cost |
|------|-------|---------------|
| ADDITION (new daemons, services, apps) | 12 | Zero |
| CONFIGURATION (props, overlays, init.rc) | 18 | Zero |
| BRIDGE (JNI/socket between layers) | 8 | Low |
| MODIFICATION (framework source changes) | 0 | Zero |
| REPLACEMENT (GMS → open alternatives) | 7 | Low |
| **Total touch points** | **45** | **Zero to Low** |

**Key finding: ZAKO v1 requires ZERO modifications to AOSP framework source code.** The entire ZAKO layer is achievable through additions, standard configuration, and Unix socket bridges. This preserves the upstream compact completely — AOSP can be updated without rebasing any patches.

---

*This analysis confirms the architectural decision in ZAKO-Architecture-and-Vision.md: "The target state is: ZAKO is a set of packages layered on top of unmodified AOSP and an unmodified LTS kernel."*
