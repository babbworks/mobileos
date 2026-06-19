# Cat S22 Flip — Build System

AOSP build environment setup and build procedures for ZAKO OS targeting the Cat S22 Flip (QM215/MSM8937).

---

## Build Environment

### Host Requirements

| Requirement | Minimum |
|-------------|---------|
| OS | Ubuntu 20.04 LTS (x86_64) |
| RAM | 16 GB |
| Disk | 200 GB free |
| CPU | 8 cores recommended |
| Java | OpenJDK 11 |
| Python | 3.8+ |

### Host Setup

```bash
sudo apt-get install -y git-core gnupg flex bison build-essential \
  zip curl zlib1g-dev libc6-dev-i386 libncurses5 x11proto-core-dev \
  libx11-dev lib32z1-dev libgl1-mesa-dev libxml2-utils xsltproc \
  unzip fontconfig python3 openjdk-11-jdk ccache

# Enable ccache (speeds up incremental builds significantly):
export USE_CCACHE=1
export CCACHE_DIR=~/.ccache
ccache -M 50G
```

---

## Source Tree Layout

```
~/zako/
├── .repo/                      # repo tool metadata
├── build/                      # AOSP build system
├── device/cat/S22FLIP/         # Device tree
├── vendor/cat/S22FLIP/         # Proprietary blobs
├── kernel/cat/msm8937/         # Kernel (prebuilt or source)
├── packages/apps/zako/         # ZAKO-specific apps
├── frameworks/                 # AOSP frameworks
├── system/                     # Core system components
└── out/                        # Build output
```

---

## Device Tree: `device/cat/S22FLIP/`

```
device/cat/S22FLIP/
├── AndroidProducts.mk          # Declares lunch targets
├── zako_S22FLIP.mk             # Product makefile (inherits AOSP Go + vendor)
├── BoardConfig.mk              # Board-level config (partitions, kernel, AVB)
├── device.mk                   # Device-level packages and properties
├── fstab.qcom                  # Partition mount table
├── init.qcom.rc                # Device-specific init scripts
├── overlay/                    # Resource overlays (carrier config, display)
├── sepolicy/                   # Device-specific SELinux policy
├── proprietary-files.txt       # Blob inventory list
├── extract-files.sh            # Blob extraction script
├── keys/                       # Signing keys (release only; not in source tree for userdebug)
└── prebuilt/
    └── kernel                  # Prebuilt kernel image (when not building from source)
```

### Key BoardConfig.mk Settings

```makefile
# device/cat/S22FLIP/BoardConfig.mk

TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_VARIANT := cortex-a53

TARGET_BOARD_PLATFORM := msm8937
TARGET_BOOTLOADER_BOARD_NAME := QM215

# Kernel
TARGET_PREBUILT_KERNEL := device/cat/S22FLIP/prebuilt/kernel
BOARD_KERNEL_CMDLINE := console=ttyMSM0,115200,n8 androidboot.console=ttyMSM0 \
  androidboot.hardware=qcom androidboot.memcg=1 lpm_levels.sleep_disabled=1 \
  earlycon=msm_geni_serial,0x4a90000 androidboot.selinux=enforcing

# Partitions
BOARD_BOOTIMAGE_PARTITION_SIZE := 33554432
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 2147483648
BOARD_VENDORIMAGE_PARTITION_SIZE := 536870912
BOARD_USERDATAIMAGE_PARTITION_SIZE := 10737418240

# AVB
BOARD_AVB_ENABLE := true
BOARD_AVB_KEY_PATH := vendor/cat/S22FLIP/keys/zako-avb-key.pem
BOARD_AVB_ALGORITHM := SHA256_RSA4096

# Recovery
TARGET_RECOVERY_FSTAB := device/cat/S22FLIP/fstab.qcom
```

---

## Vendor Blob Extraction

### From a Connected Device (ADB)

```bash
cd device/cat/S22FLIP/
./extract-files.sh
# Pulls blobs listed in proprietary-files.txt from the device
# Places them in vendor/cat/S22FLIP/
```

### From a Firmware Dump

```bash
./extract-files.sh ~/firmware-dump/
# Reads from a local directory instead of ADB
```

The resulting `vendor/cat/S22FLIP/` directory contains:
- `Android.bp` / `Android.mk` — prebuilt declarations
- `S22FLIP-vendor.mk` — included by `device.mk`
- `proprietary/` — the actual blob files

---

## Kernel Compilation

### Prebuilt Approach (Default for v1)

A prebuilt `kernel` binary is stored at `device/cat/S22FLIP/prebuilt/kernel`. This is compiled externally and dropped into the device tree. The AOSP build system packages it into `boot.img` with the generated ramdisk.

### Source Compilation

```bash
cd kernel/cat/msm8937/

# Set up cross-compilation:
export ARCH=arm
export CROSS_COMPILE=arm-linux-androideabi-
export CLANG_TRIPLE=arm-linux-gnueabi-
export CC=clang

# Generate defconfig:
make O=out msm8937-perf_defconfig

# Build:
make O=out -j$(nproc) \
  CC=clang \
  CLANG_TRIPLE=arm-linux-gnueabi- \
  CROSS_COMPILE=arm-linux-androideabi-

# Output:
ls out/arch/arm/boot/zImage-dtb
# Copy to device tree prebuilt location:
cp out/arch/arm/boot/zImage-dtb ../../../device/cat/S22FLIP/prebuilt/kernel
```

See [[kernel README|kernel/README]] for full kernel documentation.

---

## Build Commands Reference

```bash
# Set up environment (required once per terminal session):
source build/envsetup.sh
lunch zako_S22FLIP-userdebug

# Full build:
m -j$(nproc)

# Specific images:
m bootimage -j$(nproc)
m systemimage -j$(nproc)
m vendorimage -j$(nproc)

# OTA package:
m otapackage -j$(nproc)

# Distribution archive (for signing):
m dist -j$(nproc)

# Clean build:
m clean                  # remove out/ entirely
m installclean           # remove out/ for current target only (faster)

# Single module rebuild:
mmm device/cat/S22FLIP/  # rebuild device tree modules
mm -j$(nproc)            # rebuild current directory module
```

---

## Output Artifacts

After a successful build, key artifacts are in `out/target/product/S22FLIP/`:

| File | Purpose |
|------|---------|
| `boot.img` | Kernel + ramdisk |
| `system.img` | Android system partition |
| `vendor.img` | Vendor HAL blobs |
| `vbmeta.img` | AVB verification metadata |
| `dtbo.img` | Device tree blob overlays |
| `recovery.img` | Recovery mode image |
| `userdata.img` | Empty encrypted userdata (for factory flash) |
| `zako_S22FLIP-ota-*.zip` | Full OTA package |
| `zako_S22FLIP-target_files-*.zip` | Target-files archive (for signing) |

---

## Common Issues

| Issue | Solution |
|-------|----------|
| Jack server out of memory | `export JACK_SERVER_VM_ARGUMENTS="-Xmx4g"` |
| Missing vendor blobs | Re-run `extract-files.sh`; check `proprietary-files.txt` |
| Kernel mismatch (boot loop) | Verify kernel defconfig matches Android version; check DTB |
| SELinux denials on boot | Check `device/cat/S22FLIP/sepolicy/` for missing contexts |
| Out of disk space | `m installclean`; increase disk; use ccache to reduce rebuilds |

---

## Related Documents

- [[contributor-guide]] — Full development workflow
- [[02-architecture]] — Software stack overview
- [[blob-audit]] — What blobs are needed and why
- Kernel docs: `CatFlip/kernel/README.md`
