# Cat S22 Flip — Contributor Guide

How to set up the build environment, build ZAKO OS, flash a device, and submit changes.

---

## Prerequisites

| Requirement | Minimum |
|-------------|---------|
| OS | Ubuntu 20.04+ (or equivalent; WSL2 works for builds) |
| RAM | 16 GB (32 GB recommended) |
| Disk | 200 GB free (AOSP source + out/ artifacts) |
| CPU | 8+ cores recommended |
| Java | OpenJDK 11 |
| Python | 3.8+ |
| Device | Cat S22 Flip with unlocked bootloader |

---

## Environment Setup

```bash
# Install build dependencies (Ubuntu/Debian):
sudo apt-get install -y git-core gnupg flex bison build-essential \
  zip curl zlib1g-dev libc6-dev-i386 libncurses5 x11proto-core-dev \
  libx11-dev lib32z1-dev libgl1-mesa-dev libxml2-utils xsltproc \
  unzip fontconfig python3 python3-pip openjdk-11-jdk repo

# Install repo tool (if not in package manager):
mkdir -p ~/bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
export PATH=~/bin:$PATH

# Configure git identity:
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
```

---

## Repo Manifest Structure

```bash
# Initialize the workspace:
mkdir ~/zako && cd ~/zako
repo init -u https://[manifest-repo-url]/zako-manifest.git -b main

# Sync all sources:
repo sync -c -j$(nproc) --no-tags
```

### Manifest Repositories

| Repository | Path in Tree | Content |
|------------|-------------|---------|
| `platform/manifest` | `.repo/manifests/` | Repo manifest (project list) |
| `device/cat/S22FLIP` | `device/cat/S22FLIP/` | Device tree (BoardConfig, overlays, sepolicy) |
| `vendor/cat/S22FLIP` | `vendor/cat/S22FLIP/` | Proprietary blobs + makefiles |
| `kernel/cat/msm8937` | `kernel/cat/msm8937/` | Prebuilt kernel (or source tree) |
| `packages/apps/zako` | `packages/apps/zako/` | ZAKO-specific apps and configuration |

---

## Building

```bash
cd ~/zako

# Set up environment:
source build/envsetup.sh

# Select target:
lunch zako_S22FLIP-userdebug
# (or zako_S22FLIP-user for release builds)

# Build:
m -j$(nproc)

# Output artifacts:
ls out/target/product/S22FLIP/
# → boot.img, system.img, vendor.img, vbmeta.img, recovery.img
```

### Build Targets

| Target | Use |
|--------|-----|
| `zako_S22FLIP-userdebug` | Development (ADB root, SELinux permissive available) |
| `zako_S22FLIP-user` | Release (SELinux enforcing, no root, signed) |
| `bootimage` | Build only boot.img (kernel + ramdisk) |
| `systemimage` | Build only system.img |
| `vendorimage` | Build only vendor.img |

---

## Flashing

### Prerequisites

- Bootloader unlocked (`fastboot oem unlock`)
- USB cable connected, device in fastboot mode (`adb reboot bootloader`)

### Flash Procedure

```bash
cd out/target/product/S22FLIP/

# Flash all images:
fastboot flash boot boot.img
fastboot flash system system.img
fastboot flash vendor vendor.img
fastboot flash vbmeta vbmeta.img
fastboot flash dtbo dtbo.img

# Wipe userdata (required on first flash or major version change):
fastboot -w

# Reboot:
fastboot reboot
```

If the device uses dynamic partitions (`super`), use fastbootd:

```bash
fastboot reboot fastboot
fastboot flash system system.img
fastboot flash vendor vendor.img
fastboot reboot
```

---

## Submitting Changes

### Branch Naming

```
feature/<short-description>
bugfix/<short-description>
```

### Commit Message Format

```
<component>: <summary under 72 chars>

<body: what and why, not how>

Signed-off-by: Your Name <you@example.com>
```

Examples:

```
device/S22FLIP: enable Hall sensor wakeup source
telephony: fix USSD encoding for Zamtel shortcodes
sepolicy: add context for ntfy background service
```

### Code Review

1. Push branch to the project Gerrit/GitLab instance
2. Assign reviewer from the device maintainer list
3. All changes require at least one approval
4. CI must pass (build + basic boot test) before merge

---

## Coding Standards

- **C/C++ (kernel, HAL):** Linux kernel coding style (`scripts/checkpatch.pl`)
- **Java (framework):** AOSP code style (4-space indent, Google Java Style)
- **Makefiles:** AOSP conventions (Android.mk / Android.bp)
- **Shell scripts:** POSIX sh where possible; bash only when necessary; `shellcheck` clean
- **Markdown (docs):** Obsidian-compatible, no HTML, link with `[[wikilinks]]`

---

## Testing Requirements

Before submitting any change:

1. **Build succeeds** — `m -j$(nproc)` completes without errors
2. **Boot test** — device reaches home screen on userdebug build
3. **Affected subsystem test:**
   - Telephony changes → verify voice call + SMS + USSD on at least one SIM
   - Display/sensor changes → verify lid open/close behaviour
   - Power changes → verify Outstack mode transitions (battery simulation or real drain)
4. **SELinux** — no new `avc: denied` in `dmesg` (userdebug build, enforcing mode)

```bash
# Check for SELinux denials:
adb shell dmesg | grep "avc: denied"

# Check boot completion:
adb shell getprop sys.boot_completed  # should return 1
```

---

## Useful Commands

```bash
# Rebuild just the device tree after a change:
mmm device/cat/S22FLIP/

# Rebuild a single module:
mm -j$(nproc)    # (run from module directory)

# Generate OTA package:
m otapackage -j$(nproc)

# Extract vendor blobs from a connected device:
cd device/cat/S22FLIP/
./extract-files.sh
```

---

## Related Documents

- [[02-architecture]] — Build system overview
- [[release-process]] — Signing and release procedure
- [[blob-audit]] — Vendor blob extraction
