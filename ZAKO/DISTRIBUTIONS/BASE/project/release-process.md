# Release Process — ZAKO Distribution Base

Universal procedure for producing, signing, and distributing a ZAKO distribution release. Adapt device-specific paths and commands for each distribution.

---

## Release Types

| Type | Audience | Distribution method | Signing |
|---|---|---|---|
| Dev build | Internal testing | Manual flash via fastboot | AOSP debug keys |
| Beta | Trusted testers | Sideload ZIP | Distribution release keys |
| Production | End users | OTA or sideload ZIP | Distribution release keys |
| Security patch | Existing users | OTA delta | Distribution release keys |

---

## Version Numbering

```
Format: MAJOR.MINOR.PATCH-YYYYMMDD
Example: 1.0.0-20260101

MAJOR: Incompatible change (base Android version bump, major architecture change)
MINOR: Feature release
PATCH: Bug fix or security patch
YYYYMMDD: Build date — always included
```

```makefile
# In product makefile:
PRODUCT_VERSION_MAJOR = 1
PRODUCT_VERSION_MINOR = 0
PRODUCT_VERSION_PATCH = 0

PRODUCT_BUILD_PROP_OVERRIDES += \
    ro.[distro].version=$(PRODUCT_VERSION_MAJOR).$(PRODUCT_VERSION_MINOR).$(PRODUCT_VERSION_PATCH) \
    ro.[distro].build.date=$(shell date +%Y%m%d)
```

---

## Pre-Release Checklist

```
Code:
[ ] All patches from kernel-patches.md applied to kernel
[ ] Kernel defconfig finalized
[ ] Device tree changes correct for this release
[ ] No debug/engineering-only flags in production build
[ ] No androidboot.selinux=permissive in kernel cmdline

Keys:
[ ] Build signed with distribution release keys (not AOSP test keys)
[ ] AVB signing keys used: [PATH_TO_KEYS]/avb_key.pem
[ ] Key fingerprints documented for this release
[ ] Keys backed up in secure location

Testing:
[ ] All Phase 7 tests passed (see distribution-checklist.md)
[ ] regression-log.md updated with release candidate results
[ ] No undocumented Critical or High issues in known-issues.md

Compliance:
[ ] GPL kernel sources published or link to existing published sources
[ ] Proprietary blob list (proprietary-files.txt) current
[ ] Release notes written covering known limitations
```

---

## Step 1: Production Build

```bash
cd ~/aosp
source build/envsetup.sh
lunch [DISTRO_CODENAME]_[DEVICE_CODENAME]-user    # user variant (not userdebug)

# Clean build for release:
m installclean

# Full build:
m -j$(nproc) 2>&1 | tee release-build.log

# Build target-files package (required for signing):
m target-files-package
```

---

## Step 2: Sign the Build

```bash
KEYS=[PATH_TO_KEYS]
TARGET_FILES=out/target/product/[DEVICE_CODENAME]/obj/PACKAGING/target_files_intermediates/[DISTRO_CODENAME]_[DEVICE_CODENAME]-target_files-*.zip

./build/tools/releasetools/sign_target_files_apks \
  -o \
  --key_mapping build/target/product/security/platform=$KEYS/platform \
  --key_mapping build/target/product/security/shared=$KEYS/shared \
  --key_mapping build/target/product/security/media=$KEYS/media \
  --key_mapping build/target/product/security/networkstack=$KEYS/networkstack \
  --default_key_mappings $KEYS/releasekey \
  $TARGET_FILES \
  signed-target-files.zip
```

---

## Step 3: Generate Release Artifacts

### Full Flashable Image Set

```bash
./build/tools/releasetools/img_from_target_files \
  signed-target-files.zip \
  signed-img.zip

unzip signed-img.zip -d release/images/
```

### OTA Package (Full)

```bash
./build/tools/releasetools/ota_from_target_files \
  -k $KEYS/releasekey \
  --block \
  signed-target-files.zip \
  ota-[distro]-[device]-1.0.0-20260101-full.zip
```

### OTA Package (Incremental / Delta)

Deltas are significantly smaller — important for users on metered data:

```bash
./build/tools/releasetools/ota_from_target_files \
  -k $KEYS/releasekey \
  --block \
  -i previous-signed-target-files.zip \
  signed-target-files.zip \
  ota-[distro]-[device]-1.0.0-to-1.0.1-delta.zip
```

---

## Step 4: Generate and Sign Checksums

```bash
cd release/

sha256sum \
  ota-[distro]-[device]-VERSION-DATE-full.zip \
  signed-img.zip \
  > SHA256SUMS.txt

openssl dgst -sha256 -sign $KEYS/releasekey.pem SHA256SUMS.txt > SHA256SUMS.txt.sig
```

Users verify:
```bash
sha256sum -c SHA256SUMS.txt
openssl dgst -sha256 -verify releasekey.x509.pem -signature SHA256SUMS.txt.sig SHA256SUMS.txt
```

---

## Step 5: Flash Script

Create `flash-[device].sh` to accompany the image set. Adapt device-specific fastboot commands:

```bash
#!/bin/bash
# [DISTRO_NAME] — [DEVICE_NAME] Flash Script
set -e

echo "=== [DISTRO_NAME] Flash Script for [DEVICE_NAME] ==="
echo "This will erase all user data."
read -p "Continue? [y/N] " confirm
[[ "$confirm" == "y" ]] || exit 1

IMAGES="$(dirname "$0")"

echo "Entering fastboot..."
adb reboot bootloader 2>/dev/null || true
sleep 5
fastboot devices || { echo "ERROR: No device in fastboot"; exit 1; }

echo "Flashing static partitions..."
fastboot flash boot     "$IMAGES/boot.img"
fastboot flash dtbo     "$IMAGES/dtbo.img"
fastboot flash vbmeta   "$IMAGES/vbmeta.img"
fastboot flash recovery "$IMAGES/recovery.img"

echo "Entering fastbootd for dynamic partitions..."
fastboot reboot fastboot
sleep 5

echo "Flashing system partitions..."
fastboot flash system  "$IMAGES/system.img"
fastboot flash vendor  "$IMAGES/vendor.img"
fastboot flash product "$IMAGES/product.img"

echo "Wiping userdata..."
fastboot -w

echo "Rebooting..."
fastboot reboot

echo "=== Flash complete. First boot may take 2-3 minutes. ==="
```

---

## Step 6: OTA Server

Host update metadata at `[OTA_SERVER]/update.json`:

```json
{
  "version": "1.0.0",
  "date": "20260101",
  "url": "https://[OTA_SERVER]/ota-[distro]-[device]-1.0.0-20260101-full.zip",
  "size": 456789012,
  "sha256": "[SHA256_HASH]",
  "delta_from": "0.9.0",
  "delta_url": "https://[OTA_SERVER]/ota-delta-0.9.0-1.0.0.zip",
  "delta_size": 123456789,
  "delta_sha256": "[DELTA_HASH]",
  "changelog": "https://[OTA_SERVER]/changelog-1.0.0.txt"
}
```

Configure the OTA client in system properties:

```
# system.prop:
ro.ota.update.url=https://[OTA_SERVER]/update.json
```

---

## Step 7: Publish GPL Sources

```bash
# Tag the kernel release:
cd kernel/[DEVICE_VENDOR]/[DEVICE_CODENAME]
git tag [distro]-os-1.0.0-20260101
git push origin [distro]-os-1.0.0-20260101
```

Required to publish (GPL obligation):
- Kernel source with all patches applied, tagged per release
- Device tree (Apache 2.0 — publish anyway)
- Any GPL modifications to AOSP components

Not required:
- Vendor blobs (proprietary)
- Distribution configuration files that are not GPL

---

## Release Archive Structure

```
releases/
└── [DISTRO_NAME]-[DEVICE_CODENAME]-1.0.0-20260101/
    ├── images/
    │   ├── boot.img
    │   ├── dtbo.img
    │   ├── vbmeta.img
    │   ├── recovery.img
    │   ├── system.img
    │   ├── vendor.img
    │   ├── product.img
    │   └── super.img
    ├── ota-[distro]-[device]-1.0.0-20260101-full.zip
    ├── flash-[device].sh
    ├── SHA256SUMS.txt
    ├── SHA256SUMS.txt.sig
    ├── CHANGELOG.md
    ├── KNOWN-ISSUES.md
    └── signed-target-files.zip  ← keep for future delta OTA generation
```

---

## Release Notes Template

```markdown
# [DISTRO_NAME] [VERSION] for [DEVICE_NAME]

**Release date:** YYYY-MM-DD
**Build:** [DISTRO_CODENAME]_[DEVICE_CODENAME]-user-VERSION-YYYYMMDD
**Base:** Android [VERSION] (AOSP [AOSP_BRANCH])
**AVB rollback index:** [N]
**ZAKO standard version:** v[N]

## What's New
- [changelog]

## Known Limitations
- [list all known-issues with user-visible impact]

## Installation
See flash-[device].sh — requires unlocked bootloader. Wipes all user data.

## Verification
SHA-256: (see SHA256SUMS.txt)
Release key fingerprint: [FINGERPRINT]
```
