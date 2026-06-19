# Cat S22 Flip — Partition Map

MSM8937 / eMMC partition layout. This document covers the expected partition structure based on MSM8937 platform conventions and stock firmware analysis. Verify against the actual device using the commands below.

---

## Extracting the Partition Table

```bash
# Full partition listing by name (most reliable):
adb shell ls -la /dev/block/by-name/

# Raw partition table:
adb shell cat /proc/partitions

# fastboot partition info (requires bootloader unlock):
fastboot getvar all 2>&1 | grep partition

# From EDL (via edl tool, if ADB is unavailable):
edl printgpt
```

---

## Expected Partition Layout (MSM8937 Reference)

Based on Qualcomm MSM8937 platform conventions and Android 11 Go device profiles. Actual sizes must be verified from the device.

### Static Partitions (lower eMMC)

| Partition | Block device | Purpose | Typical size |
|---|---|---|---|
| `sbl1` | mmcblk0p1 | Secondary bootloader (Qualcomm signed) | ~256 KB |
| `sbl1bak` | mmcblk0p2 | SBL1 backup | ~256 KB |
| `rpm` | mmcblk0p3 | Resource Power Manager firmware | ~256 KB |
| `rpmbak` | mmcblk0p4 | RPM backup | ~256 KB |
| `tz` | mmcblk0p5 | TrustZone / QSEE (TEE) | ~2 MB |
| `tzbak` | mmcblk0p6 | TZ backup | ~2 MB |
| `devcfg` | mmcblk0p7 | Device configuration table | ~64 KB |
| `devcfgbak` | mmcblk0p8 | Device config backup | ~64 KB |
| `aboot` | mmcblk0p9 | LK bootloader (Little Kernel) | ~512 KB |
| `abootbak` | mmcblk0p10 | Aboot backup | ~512 KB |
| `boot` | mmcblk0p22 | Android kernel + ramdisk (A/B or single) | ~32 MB |
| `recovery` | mmcblk0p23 | Recovery image | ~32 MB |
| `dtbo` | mmcblk0p24 | Device Tree Blob Overlay | ~4 MB |
| `vbmeta` | mmcblk0p25 | AVB 2.0 verification metadata | ~64 KB |

### Radio / Modem Partitions

| Partition | Purpose |
|---|---|
| `modem` | Modem firmware (AMSS) |
| `modembak` | Modem backup |
| `dsp` | Hexagon DSP firmware |
| `dspbak` | DSP backup |
| `bluetooth` | Bluetooth firmware |
| `wifi` | WiFi firmware (WCNSS) |

### Android System Partitions

| Partition | Purpose | Note |
|---|---|---|
| `system` | AOSP system image | Read-only; verified by dm-verity |
| `vendor` | Vendor HALs and blobs | Read-only; verified by dm-verity |
| `product` | Product-specific apps | Read-only |
| `userdata` | User data (FBE encrypted) | R/W; wiped on factory reset |
| `cache` | Recovery cache / OTA staging | Wiped on factory reset |
| `persist` | Persistent calibration data | Survives factory reset |
| `misc` | BCB (boot control block) for OTA | |
| `frp` | Factory Reset Protection | Cleared only with authorized account removal |

### MSM8937-Specific Partitions

| Partition | Purpose |
|---|---|
| `fsc` | File system cookie |
| `ssd` | Secure software download |
| `keystore` | Keymaster key storage (hardware-backed on TZ) |
| `limits` | Thermal/power limits tables |
| `limits2` | Secondary limits tables |
| `lksecapp` | LK secure app |
| `secdata` | Security data |
| `splash` | Boot splash screen |

---

## A/B Partition Note

MSM8937 / Android 11 Go devices may use A/B (seamless updates) or the traditional single-slot layout. The Cat S22 Flip stock firmware uses a **single-slot** layout (no `_a` / `_b` suffixes). This simplifies flashing but means OTA updates require a full reboot into recovery.

Confirm with:
```bash
adb shell getprop ro.boot.slot_suffix
# If empty or not set → single-slot (A-only)
```

---

## Dynamic Partitions

Android 11 uses dynamic partitions by default (system, vendor, product live inside a `super` partition). On MSM8937 / Go devices, this may or may not be implemented. Check:

```bash
adb shell cat /proc/partitions | grep super
# If super exists → dynamic partition layout
```

If dynamic partitions are used, flashing requires `fastbootd` (userspace fastboot) rather than bootloader fastboot for system/vendor/product:

```bash
fastboot reboot fastboot         # enter fastbootd
fastboot flash system system.img
fastboot flash vendor vendor.img
```

---

## Partition Sizes — Action Required

The sizes in this table are estimates. Extract actual sizes before writing any flash scripts:

```bash
adb shell cat /proc/partitions
# Column 3 is size in 1K blocks
# Multiply by 1024 for bytes

# For a complete named list with sizes:
adb shell ls -la /dev/block/by-name/ | awk '{print $NF}' | while read p; do
  size=$(adb shell blockdev --getsize64 "$p" 2>/dev/null)
  echo "$p: $size bytes"
done
```

Update the table above with verified sizes before any release flash scripts are written.

---

## Flashing Reference

For the complete fastboot flash procedure including partition order, see `CatFlip/project/release-process.md` (inherited from `ZAKO/DISTRIBUTIONS/BASE/project/release-process.md`) and `CatFlip/bootloader/`.

**Critical:** never flash `tz`, `rpm`, `sbl1`, or `modem` partitions with incorrect images. These partitions contain signed Qualcomm firmware. Flashing incorrect images here bricks the device; recovery requires EDL mode.
