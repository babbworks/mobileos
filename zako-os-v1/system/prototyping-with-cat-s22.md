# Prototyping with Cat S22 — Next Steps Report

**Date:** June 2026  
**Status:** Phases 1–3 complete (1,207 tests passing). Software stack ready for hardware integration.  
**Device:** Cat S22 Flip · QM215 (MSM8937) · 4×A53 1.4GHz · 2GB RAM · 16GB eMMC · Linux 4.9 CAF

---

## Current State

Twelve components built and tested on macOS host:

| Layer | Components | Tests |
|-------|-----------|-------|
| Foundation (Phase 1) | libzako-hash, sign, did, bitpads, bitledger, c0 | 495 |
| Core Daemons (Phase 2) | libzako-bus, telux-ledgerd, telux-identd, outstack-powerd | 425 |
| Exchange (Phase 3) | libzako-padsurl, libzako-pictography, exchange-engine, telux-sharedb | 287 |

All code is C99, no dynamic allocation in steady state, static libraries. Ready to cross-compile for ARM32 and integrate into an AOSP build.

---

## Section 1: Work to Be Done in Linux Environment (Before Device)

This is workstation-only work. Requires a Linux machine (x86_64) with ~200GB disk for AOSP source, a functioning `repo` tool, and the ARM32 NDK/toolchain.

### 1.1 AOSP Source Sync and Environment Setup

| Task | Detail | Time |
|------|--------|------|
| Install `repo` + AOSP dependencies | Python 3, git-lfs, OpenJDK 8, ccache | 1 hour |
| Sync AOSP source (android-11.0.0_r48 or android-12.0.0_r1) | `repo init` with Qualcomm CAF manifest for MSM8937 | 4–8 hours download |
| Verify host build tools | `m -j$(nproc)` on generic target to confirm env | 2 hours |
| Set up ccache (50GB recommended) | Dramatically reduces rebuild time after first build | 10 min |

**Decision required:** Android 11 (Go edition, matches stock Cat S22) or Android 12 (newer HALs, slightly more work). Recommend Android 11 for initial bring-up to minimize unknowns.

### 1.2 Device Tree Creation

Create `device/babb/s22flip/` with:

```
device/babb/s22flip/
├── AndroidProducts.mk
├── BoardConfig.mk          ← partition sizes, kernel config, SELinux
├── device.mk               ← what packages to build, overlays
├── s22flip.mk              ← lunch target (aosp_s22flip-userdebug)
├── fstab.qcom              ← mount table (boot, system, vendor, data)
├── init.s22flip.rc         ← ZAKO daemon service entries
├── overlay/                ← resource overlays (no GMS, captive portal)
├── proprietary-files.txt   ← blob manifest
├── recovery/
│   └── root/
│       └── etc/twrp.fstab
└── sepolicy/               ← SELinux .te files for ZAKO daemons
```

**Key files to write:**

- `BoardConfig.mk`: TARGET_ARCH := arm, TARGET_BOARD_PLATFORM := msm8937, BOARD_KERNEL_CMDLINE, partition sizes from stock `partition.xml`
- `init.s22flip.rc`: service entries for telux-ledgerd, telux-identd, outstack-powerd, telux-sharedb, libzako-bus (busd)
- `sepolicy/*.te`: per-daemon SELinux domains (telux_ledgerd_t, telux_identd_t, outstack_powerd_t)

### 1.3 Kernel Source Preparation

| Task | Detail |
|------|--------|
| Obtain CAF kernel source | `git clone` from CodeLinaro (tag LA.UM.8.6.2.r1 or similar for MSM8937) |
| Apply ZAKO defconfig additions | `CONFIG_CGROUP_FREEZER=y`, `CONFIG_THERMAL_GOV_USER_SPACE=y` |
| Cross-compile for ARM32 | `ARCH=arm CROSS_COMPILE=arm-linux-androideabi-` |
| Verify boot on QEMU (optional) | `qemu-system-arm -M virt` with the zImage — confirms compilation |

**Required kernel configs for ZAKO:**
- `CONFIG_CGROUP_FREEZER=y` — Outstack process gating
- `CONFIG_CPUFREQ_GOV_POWERSAVE=y` — Critical/Emergency governors
- `CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y` — Full/Standard governors
- `CONFIG_THERMAL_GOV_USER_SPACE=y` — outstack-powerd thermal control
- `CONFIG_UNIX=y` — Unix domain sockets (bus IPC)

### 1.4 Cross-Compile ZAKO Components

Add a top-level `system/Makefile.arm32` that cross-compiles all 12 components:

```makefile
CC := arm-linux-androideabi-gcc
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 \
          -march=armv7-a -mfloat-abi=softfp -mfpu=neon
```

Test with QEMU user-mode (`qemu-arm ./test_ledgerd`) before touching the device. This validates all code runs correctly on ARM32.

### 1.5 Vendor Blob Extraction

| Task | Detail |
|------|--------|
| Boot stock Cat S22 | Ensure device is functional on stock ROM |
| Enable ADB | Settings → Developer Options → USB Debugging |
| Run extraction script | `extract-files.sh` pulls all proprietary blobs |
| Organize into `vendor/babb/s22flip/` | Follows AOSP vendor tree convention |

Critical blobs needed:
- **Modem firmware** (`modem.mdt`, `mba.mbn`) — telephony won't work without these
- **ADSP/CDSP firmware** — audio, sensors
- **GPU blobs** (`libGLES_adreno.so`, etc.) — display
- **WiFi firmware** (`wlan/prima/WCNSS_qcom_wlan_nv.bin`)
- **Bluetooth firmware**
- **Camera firmware** (less critical for initial bring-up)

### 1.6 ZAKO Integration into AOSP Build

Create `system/packages/zako-daemons/Android.bp`:

```blueprint
cc_binary {
    name: "telux-ledgerd",
    srcs: ["components/telux-ledgerd/ledgerd_store.c",
           "components/telux-ledgerd/ledgerd_daemon.c",
           ...],
    static_libs: ["libzako-hash", "libzako-bus", "libzako-bitledger"],
    shared_libs: ["libsqlite"],
    init_rc: ["telux-ledgerd.rc"],
    ...
}
```

Each daemon becomes an AOSP `cc_binary`, each library a `cc_library_static`. The build system handles ARM32 cross-compilation, linking, and installation into the system image.

### 1.7 SELinux Policy

Minimum policy for initial bring-up (permissive domains, tightened later):

```
# telux_ledgerd.te
type telux_ledgerd, domain;
type telux_ledgerd_exec, exec_type, file_type;
init_daemon_domain(telux_ledgerd)
allow telux_ledgerd zako_data_file:dir create_dir_perms;
allow telux_ledgerd zako_data_file:file create_file_perms;
allow telux_ledgerd zako_socket:unix_stream_socket { create bind listen accept read write };
```

### 1.8 First Build (No Device Required)

```bash
source build/envsetup.sh
lunch aosp_s22flip-userdebug
m -j$(nproc)
```

This produces `boot.img`, `system.img`, `vendor.img`. Even without flashing, a successful build confirms the integration is structurally correct.

---

## Section 2: Direct Device Work (Loading onto Cat S22)

This requires the physical Cat S22 Flip, a USB cable, and `fastboot`/`adb`.

### 2.1 Bootloader Unlock

| Step | Command | Risk |
|------|---------|------|
| Enable OEM unlock | Settings → Developer Options → OEM Unlock | None |
| Reboot to bootloader | `adb reboot bootloader` | None |
| Unlock | `fastboot oem unlock` | **Wipes all data** |
| Verify | `fastboot getvar unlocked` → "yes" | None |

**Important:** Take a full stock ROM backup before unlocking. Use `dd` to dump all partitions from ADB root shell if possible.

### 2.2 Stock Partition Dump (Preservation)

Before flashing anything custom:

```bash
adb root
adb shell "dd if=/dev/block/bootdevice/by-name/boot of=/sdcard/stock_boot.img"
adb shell "dd if=/dev/block/bootdevice/by-name/system of=/sdcard/stock_system.img"
adb shell "dd if=/dev/block/bootdevice/by-name/vendor of=/sdcard/stock_vendor.img"
adb pull /sdcard/stock_*.img ./backup/
```

### 2.3 Flash ZAKO Build

```bash
fastboot flash boot out/target/product/s22flip/boot.img
fastboot flash system out/target/product/s22flip/system.img
fastboot flash vendor out/target/product/s22flip/vendor.img
fastboot reboot
```

### 2.4 First Boot Validation

Expected sequence on first successful boot:

1. Qualcomm splash logo (from bootloader — unchanged)
2. Kernel boots (serial console via `adb logcat` if display doesn't come up)
3. Android init starts, mounts partitions
4. ZAKO daemons start (defined in init.rc)
5. SurfaceFlinger starts, display shows AOSP launcher (initially — replaced later)

**First-boot checklist (via ADB):**

```bash
adb shell ps | grep telux       # daemons running?
adb shell ps | grep outstack    # power governor running?
adb shell ls /data/zako/        # data directory created?
adb shell cat /sys/class/power_supply/battery/capacity  # battery readable?
adb shell getenforce            # SELinux enforcing?
adb shell getprop ro.build.display.id  # our build ID?
```

### 2.5 Iterative Bring-Up (Expected Issues)

| Issue | Likely Cause | Fix |
|-------|-------------|-----|
| Bootloop (no display) | Missing vendor blob or wrong kernel | Flash stock boot.img, check logcat |
| Daemons crash on start | Missing /data/zako directory, wrong SELinux context | Add `mkdir` to init.rc, fix file_contexts |
| No modem | Missing modem firmware or wrong persist partition | Verify blob extraction included modem.* |
| No WiFi | Missing WCNSS firmware | Check vendor blob for wlan/ directory |
| SQLite fails | Missing /system/lib/libsqlite.so | Add to device.mk dependencies |

---

## Section 3: Testing on Device

### 3.1 Daemon Smoke Tests

Run the existing test binaries on-device via ADB push:

```bash
# Cross-compile tests for ARM32
make -f Makefile.arm32 test-bins

# Push and run
adb push test_ledgerd /data/local/tmp/
adb shell chmod +x /data/local/tmp/test_ledgerd
adb shell /data/local/tmp/test_ledgerd
# Expected: "=== Results: 121/121 passed ==="
```

Repeat for all component test binaries. This confirms the code runs correctly on ARM32 hardware (not just QEMU).

### 3.2 Integration Tests (Daemons Communicating)

With all daemons running under init:

```bash
# Verify bus is up
adb shell ls /var/run/zako/bus.sock

# Send a test frame to ledgerd via a CLI tool
adb shell /system/bin/zako-ctl ledger append --value 500 --direction in --pair 4

# Check it was stored
adb shell /system/bin/zako-ctl ledger last

# Verify chain
adb shell /system/bin/zako-ctl ledger verify --from 1 --to 5

# Check outstack mode
adb shell /system/bin/zako-ctl outstack status

# Test identity
adb shell /system/bin/zako-ctl identity sovereign
```

### 3.3 Telephony Tests (Phase 5)

| Test | Method | Pass Criteria |
|------|--------|--------------|
| SIM detection | `adb shell getprop gsm.sim.state` | "READY" |
| Network registration | `adb shell dumpsys telephony.registry` | Registered on Canadian carrier |
| Outbound voice call | Dial from device | Call connects, audio both ways |
| Inbound voice call | Call the device | Ring, answer, audio both ways |
| Outbound SMS | Send via CLI or messaging app | Delivered |
| Inbound SMS | Send SMS to device | Received, readable via ADB |
| LTE data | `adb shell ping 8.8.8.8` | Packets returned |

### 3.4 Power Governance Tests

| Test | Method | Pass Criteria |
|------|--------|--------------|
| Mode transition at 50% | Drain battery to 50%, observe | outstack reports STD mode |
| Process gating | Check `cat /sys/fs/cgroup/freezer/outstack_opportunistic/freezer.state` | "FROZEN" in STD+ |
| Thermal override | Heat device (sunlight/thermal pad) | Mode enters EMRG at 45°C sustained |
| Charging override | Plug USB-C charger in CRIT mode | Mode improves by one step |
| Lid close | Close flip | SW_LID event fires (read via `getevent`) |

### 3.5 Performance Benchmarks

| Metric | Target | Measurement |
|--------|--------|-------------|
| Daemon startup | < 100ms each | `adb logcat | grep "started"` timestamps |
| Record append latency | < 5ms | Instrument `lds_append()` with `clock_gettime` |
| Chain verify (1000 records) | < 50ms | Benchmark tool |
| Exchange round-trip | < 2s (loopback) | End-to-end timer |
| Idle power (SIM registered) | < 100mW | Monsoon power monitor or battery drain rate |

---

## Section 4: User Interface Development

### 4.1 When UI Work Begins

UI development starts in Phase 6, **after the device is a functional phone** (Phase 5 complete). The reason: UI rendering on Android requires understanding the specific display pipeline of the device — SurfaceFlinger behavior, display resolution (480×800 main, 240×240 cover), refresh paths, and input routing. This can only be validated on the physical device.

However, **UI logic and layout code can be developed in parallel** on the host using a framebuffer simulator (SDL2 window simulating 480×800 resolution).

### 4.2 Rendering Approach Decision

A 2-week research sprint at Phase 6 start evaluates these options:

| Approach | Pros | Cons | Recommendation |
|----------|------|------|----------------|
| **NativeActivity + EGL** | Official Android API, composites with status bar, gets HW acceleration | Minimal JVM shim required, EGL setup boilerplate | **Primary candidate** |
| **lvgl on framebuffer** | Pure C, designed for embedded, no Android dependency | Doesn't composite with Android UI, no hw accel on Android | Best for cover display |
| **SurfaceFlinger native client** | Full HW acceleration, no JVM at all | Undocumented internal API, may break across versions | Risky |
| **Direct /dev/fb0** | Simplest possible, zero dependencies | No compositing, bypasses Android entirely, no input routing | Cover display only |

**Likely outcome:** NativeActivity+EGL for main display apps, lvgl/direct-fb for cover display.

### 4.3 The ZAKO Application Suite

These are not Android apps. They are native C programs communicating with ZAKO daemons via Unix socket IPC. They launch in <200ms, use <5MB RAM, and are governed by Outstack like any other process.

| Application | Function | Input | Display |
|-------------|----------|-------|---------|
| **ZAKO Setup** | First-run wizard: language, SIM, PIN, privacy settings | T9 keypad | Main 480×800 |
| **PADS** | Work Islands, task management, field records, invoicing | T9 keypad | Main |
| **Exchange** | Send/receive money, query history, natural language | T9 keypad | Main |
| **Sovereignty** | Island management, capability grants, identity browser | T9 keypad | Main |
| **Outstack Display** | Power mode indicator, always-visible | — | Status bar element |
| **telux-coverd** | Time, date, battery, mode, caller ID, last record | — | Cover 240×240 |
| **T9 IME** | Input method for all apps | Hardware keypad | Overlay |

### 4.4 UI Development Sequence

```
Phase 5 complete (phone works)
    │
    ├── Sprint: Rendering research (2 weeks)
    │   └── Decision: NativeActivity+EGL confirmed
    │
    ├── libzako-ui (4 weeks)
    │   ├── EGL context management
    │   ├── Text rendering (bitmap font, ASCII + Nyanja subset)
    │   ├── Layout primitives (vertical list, horizontal row, grid)
    │   ├── Input routing (T9 keypad → focus navigation)
    │   ├── Theme (dark, high-contrast, 480×800 aware)
    │   └── Tested on-device: renders a hello world screen
    │
    ├── T9 IME (3 weeks, parallel with libzako-ui)
    │   ├── Multi-tap character entry
    │   ├── Predictive text (dictionary-based, no ML)
    │   ├── Android IME framework bridge (thin JNI, core in C)
    │   └── Tested: can type a word and submit
    │
    ├── ZAKO Setup Wizard (3 weeks)
    │   ├── Language selection (English, Nyanja, Bemba)
    │   ├── SIM detection and PIN entry
    │   ├── Sovereign key generation (visual: "your identity is being created")
    │   ├── DID display as QR code
    │   └── No network required
    │
    ├── PADS app (4 weeks)
    │   ├── Work Island list view
    │   ├── Task lifecycle (ASSIGN → START → FINISH)
    │   ├── Field signoff with signature
    │   ├── Invoice generation
    │   └── Talks to ledgerd + identd via bus
    │
    ├── Exchange app (4 weeks)
    │   ├── Send/receive flow
    │   ├── Transaction history (scrolling list from ledger)
    │   ├── QR code display for receiving
    │   ├── Conservation proof display
    │   └── Talks to exchange-engine + sharedb via bus
    │
    ├── Sovereignty dashboard (3 weeks)
    │   ├── Island list with chain state
    │   ├── Capability grant/revoke UI
    │   ├── Key/DID browser
    │   └── Ledger integrity verification trigger
    │
    └── telux-coverd (2 weeks)
        ├── ST7789 SPI driver integration
        ├── 240×240 framebuffer writes
        ├── Clock, battery %, current mode
        ├── Caller ID on incoming call
        └── Last transaction summary
```

### 4.5 What Makes This Different from Android Apps

| Aspect | Android APK | ZAKO Native App |
|--------|-------------|-----------------|
| Language | Java/Kotlin on ART VM | C99 |
| Startup time | 500ms–2s (cold) | <200ms |
| RAM usage | 20–50MB per app | <5MB per app |
| IPC | Android Binder (kernel) | Unix domain socket (direct) |
| Rendering | Android View system (XML layouts) | EGL + custom widget toolkit |
| Lifecycle | Android Activity lifecycle (complex) | Simple: start, run, stop |
| Power governance | Subject to Doze (opaque) | Subject to Outstack (explicit, auditable) |
| Process class | Undefined (Android manages) | Declared at install (INTERACTIVE, BACKGROUND) |

The ZAKO apps are peers of the daemons — same language, same IPC, same process governance. The boundary between "daemon" and "app" is just whether it renders pixels.

---

## Section 5: Overall Timeline from Here

| Step | Duration | Blocking On |
|------|----------|-------------|
| Linux env setup + AOSP sync | 1–2 days | Linux workstation |
| Device tree + kernel prep | 2 weeks | AOSP synced |
| Vendor blob extraction | 1 day | Physical device + ADB |
| Cross-compile ZAKO + first build | 1 week | Device tree complete |
| First flash + boot | 1 day | Build successful |
| Daemon bring-up + testing | 1–2 weeks | Device boots |
| Telephony validation | 1 week | Canadian SIM |
| UI research sprint | 2 weeks | Device functional |
| libzako-ui + first app | 6 weeks | Research complete |

**Total to a working prototype phone with native ZAKO UI: ~14–16 weeks from receiving the device in a Linux environment.**

---

## Section 6: Pre-Device Work That Can Happen Now

Even without the device or a Linux AOSP build environment, the following work advances the project:

1. **Write `zako-ctl` CLI tool** — a command-line interface that exercises all daemon APIs via bus. Useful for testing and later for ADB shell debugging on-device.

2. **Implement the git-informed adoptions** from `06-git-adoptions-detailed.md` — prepared statement caching, branch-based Islands, signed record commits. Pure C, testable on host.

3. **Build the integration test harness** — boots all daemons + bus in one process, runs end-to-end exchange scenario.

4. **Prototype libzako-ui with SDL2** — a 480×800 SDL window simulating the device display. Develop layout primitives and T9 navigation before touching the device.

5. **Write the T9 IME logic** — multi-tap + predictive text is pure computation, no hardware needed.

6. **Cover display renderer prototype** — 240×240 framebuffer simulator for telux-coverd UI design.

---

*This document supersedes the Phase 4–7 sections of `00-project-plan.md` for tactical execution planning. The project plan remains the strategic reference; this document is the operational bridge from completed software to running hardware.*
