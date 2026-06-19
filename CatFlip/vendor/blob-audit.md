# Cat S22 Flip — Vendor Blob Audit

Inventory of proprietary binary blobs required for full hardware function on Ya. All blobs are extracted from the stock Cat S22 Flip firmware dump. This document tracks what each blob does, whether an open-source alternative exists, and its inclusion status.

---

## Extraction Process

```bash
# From a running stock device (ADB required):
adb root
adb pull /vendor/lib/ ./vendor-dump/lib/
adb pull /vendor/lib64/ ./vendor-dump/lib64/
adb pull /vendor/bin/ ./vendor-dump/bin/
adb pull /vendor/firmware/ ./vendor-dump/firmware/
adb pull /vendor/etc/ ./vendor-dump/etc/

# Alternatively, extract from stock OTA zip:
# 1. Download latest stock OTA for Cat S22 Flip
# 2. unzip ota.zip
# 3. sdat2img system.transfer.list system.new.dat.br system.img
# 4. Mount and copy vendor partition
```

The `proprietary-files.txt` in the device tree tracks every blob by path. Keep it current with the actual extracted set.

---

## Blob Categories

### 1. Critical — Device Non-Functional Without These

| Blob | Path | Purpose | Open-Source Alternative |
|---|---|---|---|
| `rild` / `libril.so` | `/vendor/bin/rild`, `/vendor/lib/libril.so` | Radio Interface Layer daemon | No — Qualcomm RIL is proprietary |
| `libqmi_cci.so` et al. | `/vendor/lib/` | QMI modem communication | No |
| RPM firmware | `/vendor/firmware/rpm.mbn` | Resource Power Manager firmware | No — signed Qualcomm binary |
| TrustZone | `/vendor/firmware/tz.mbn` | TEE / QSEE firmware | No — signed Qualcomm binary |
| `wcnss.mbn` et al. | `/vendor/firmware/` | WiFi + BT firmware (WCNSS) | No |
| Modem AMSS | `/vendor/firmware/modem*` | Cellular modem firmware | No — carrier-certified binary |

### 2. HAL Blobs — Required for Hardware Features

| Blob | Path | Purpose | Open-Source Alternative |
|---|---|---|---|
| `android.hardware.keymaster@4.0-service-qti` | `/vendor/bin/hw/` | Keymaster 4.0 (TrustZone-backed) | No — QTI implementation |
| `android.hardware.gatekeeper@1.0-service-qti` | `/vendor/bin/hw/` | Gatekeeper (screen lock) | No |
| Camera HAL (`libmmcamera*`) | `/vendor/lib/` | QCamera3 camera HAL | No — libcamera lacks QM215 support |
| `android.hardware.audio@6.0-impl` | `/vendor/lib/` | Audio HAL | No — QTI audio |
| `android.hardware.sensors@1.0-impl` | `/vendor/lib/` | Sensor HAL | No |
| `android.hardware.gnss@2.0-impl` | `/vendor/lib/` | GPS/GNSS HAL | No |
| Display HAL (`libdisplay*`) | `/vendor/lib/` | Display color management | No — qualcomm display stack |

### 3. GPU Blobs

| Blob | Path | Purpose | Open-Source Alternative |
|---|---|---|---|
| Adreno 308 drivers | `/vendor/lib/egl/libGLES_adreno.so` et al. | OpenGL ES + Vulkan | **Partial** — Mesa freedreno supports Adreno 3xx; testing required on QM215 |

Mesa freedreno is theoretically possible for Adreno 308 (a3xx family), but QM215-specific kernel/firmware support needs validation. Default to Qualcomm Adreno blobs for v1; investigate Mesa as a future improvement.

### 4. Power Management

| Blob | Path | Purpose |
|---|---|---|
| Power HAL (`android.hardware.power@1.3-service-qti`) | `/vendor/bin/hw/` | CPU performance/power hints |
| `libpowerhalwrap.so` | `/vendor/lib/` | Power HAL wrapper |
| Thermal HAL | `/vendor/bin/hw/` | Thermal management |

### 5. DRM / Widevine

| Blob | Path | Purpose | Note |
|---|---|---|---|
| `libwvhidl.so` | `/vendor/lib/` | Widevine L3 | L3 (software) due to unlocked bootloader |
| `android.hardware.drm@1.3-service.widevine` | `/vendor/bin/hw/` | Widevine HAL | |

Widevine L3 is acceptable for Zambia deployment — no streaming DRM content is part of the Ya use case.

### 6. Non-Critical / Can Omit

| Blob | Purpose | Include? |
|---|---|---|
| Google diagnostic blobs | Crash reporting to Google | **No — remove** |
| Carrier customization APKs | Pre-installed carrier apps | **No — remove** |
| Cat-branded apps | CatPhone, CatConnect etc. | **No — remove** |
| `com.qualcomm.qti.perfdump` | Qualcomm performance telemetry | **No — remove** |

---

## Blob Extraction Command Reference

```bash
# Generate proprietary-files.txt from an extracted vendor dump:
find vendor-dump/ -type f | sed 's|vendor-dump/||' | sort > proprietary-files.txt

# Check which blobs a binary depends on (find missing deps):
ldd vendor-dump/lib/libril.so

# For ARM32 blobs on an x86 build machine, use readelf instead:
readelf -d vendor-dump/lib/libril.so | grep NEEDED
```

---

## Blob Build Integration

In the device tree, blobs are declared in `proprietary-files.txt` and extracted by:

```bash
# From device tree root:
./extract-files.sh    # extracts blobs from connected ADB device
# or
./extract-files.sh [firmware-dump-path]  # extracts from an unpacked OTA
```

The `Android.mk` (or `Android.bp`) in `vendor/cat/S22FLIP/` prebuilts blobs into the build. The `device.mk` inherits from `vendor/cat/S22FLIP/S22FLIP-vendor.mk`.

---

## Open Issues

- [ ] Extract full blob list from actual device (current table is based on MSM8937 platform conventions, not confirmed against the Cat S22 Flip firmware)
- [ ] Confirm Keymaster 4.0 service name matches what the device uses (may differ from the standard QTI binary name)
- [ ] Test Mesa freedreno for Adreno 308 on QM215 — potential future path to reduced proprietary GPU dependency
- [ ] Identify and remove any Qualcomm diagnostic/telemetry blobs (`diag`, `qlogd`, `perfdump` etc.)
- [ ] Identify any Cat-branded or Bullitt-specific service APKs in the vendor partition for removal
