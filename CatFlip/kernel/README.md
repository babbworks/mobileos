# Cat S22 Flip — Kernel

Linux kernel build documentation for ZAKO OS on QM215/MSM8937.

---

## Source

| Parameter | Value |
|-----------|-------|
| Base | Linux 4.9 LTS |
| Source | Qualcomm CAF (Code Aurora Forum) |
| Tag | `LA.UM.10.6.2.r1-02500-89xx.0` |
| Architecture | ARM (32-bit, armv8-a in AArch32 mode) |
| Reference device | Xiaomi Redmi Go (tiare) — also MSM8937, Android 11 |
| Compiler | Clang (AOSP prebuilt, version 12+) |

### Source Repository

```bash
git clone https://source.codeaurora.org/quic/la/kernel/msm-4.9 \
  -b LA.UM.10.6.2.r1-02500-89xx.0 \
  kernel/cat/msm8937/
```

---

## Cross-Compilation

```bash
cd kernel/cat/msm8937/

# Environment:
export ARCH=arm
export SUBARCH=arm
export CROSS_COMPILE=arm-linux-androideabi-
export CLANG_TRIPLE=arm-linux-gnueabi-
export CC=clang

# Toolchain (use AOSP prebuilt):
export PATH=$ANDROID_BUILD_TOP/prebuilts/clang/host/linux-x86/clang-r416183b/bin:$PATH
export PATH=$ANDROID_BUILD_TOP/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin:$PATH

# Generate defconfig:
make O=out msm8937-perf_defconfig

# Apply ZAKO-specific config fragment (if used):
scripts/kconfig/merge_config.sh -m out/.config device/cat/S22FLIP/kernel_zako.config

# Build:
make O=out -j$(nproc) \
  CC=clang \
  CLANG_TRIPLE=arm-linux-gnueabi- \
  CROSS_COMPILE=arm-linux-androideabi- \
  zImage-dtb

# Output:
ls out/arch/arm/boot/zImage-dtb
```

---

## Defconfig

Base defconfig: `arch/arm/configs/msm8937-perf_defconfig`

This config targets the MSM8937 platform with performance-oriented settings. ZAKO-specific changes are applied as a config fragment or directly patched.

### Key Config Options for ZAKO

```
# Power management (critical for 1450mAh battery):
CONFIG_CPU_FREQ=y
CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y
CONFIG_CPU_FREQ_GOV_POWERSAVE=y
CONFIG_CPU_FREQ_GOV_PERFORMANCE=y
CONFIG_CPU_IDLE=y
CONFIG_ARM_CPUIDLE=y

# Cgroup support (Outstack process class enforcement):
CONFIG_CGROUPS=y
CONFIG_CGROUP_FREEZER=y
CONFIG_CGROUP_CPUACCT=y
CONFIG_CGROUP_SCHED=y
CONFIG_CGROUP_BPF=y

# Thermal management:
CONFIG_THERMAL=y
CONFIG_THERMAL_WRITABLE_TRIPS=y
CONFIG_THERMAL_GOV_STEP_WISE=y
CONFIG_THERMAL_GOV_LOW_LIMITS=y
CONFIG_QTI_THERMAL_LIMITS_DCVS=y

# Power supply sysfs (battery monitoring for outstack-powerd):
CONFIG_POWER_SUPPLY=y
CONFIG_SMB1351_USB_CHARGER=y  # or SMB1360 depending on Cat S22 Flip hardware
CONFIG_BATTERY_BMS=y

# Security:
CONFIG_SECURITY_SELINUX=y
CONFIG_SECURITY_SELINUX_DEVELOP=y
CONFIG_DM_VERITY=y
CONFIG_DM_VERITY_FEC=y

# Input (keypad + lid sensor):
CONFIG_KEYBOARD_GPIO=y
CONFIG_INPUT_GPIO_KEYS=y

# Display:
CONFIG_DRM_MSM=y
CONFIG_FB_MSM=y

# File-based encryption:
CONFIG_FS_ENCRYPTION=y
CONFIG_EXT4_ENCRYPTION=y
```

---

## Device-Specific Patches

Patches applied on top of the CAF tag for Cat S22 Flip support:

| Patch | Purpose |
|-------|---------|
| `0001-dts-add-cat-s22flip-device-tree.patch` | Device tree source for Cat S22 Flip (display, keypad, sensors) |
| `0002-input-gpio-keys-add-SW_LID-for-flip.patch` | Hall effect lid sensor as `SW_LID` input event |
| `0003-display-add-cover-display-driver.patch` | 1.44" cover display initialization (SPI/DSI) |
| `0004-defconfig-enable-zako-options.patch` | Enable ZAKO-required kernel options |
| `0005-power-supply-tune-bms-for-1450mah.patch` | Battery management system tuning for 1450mAh cell |

### Applying Patches

```bash
cd kernel/cat/msm8937/
git am ../../../device/cat/S22FLIP/kernel-patches/*.patch
```

---

## Reference Device: Xiaomi Redmi Go (tiare)

The Redmi Go shares the MSM8937 SoC and Android 11 base. Its kernel source and device tree serve as the primary reference for:

- DTS structure (CPU, GPU, display, audio)
- MSM8937 clock tree configuration
- Thermal zone definitions
- Power supply driver configuration

Differences from Cat S22 Flip:
- Redmi Go is a touchscreen slab (no keypad, no lid sensor, no cover display)
- Different display panel (different DTS bindings)
- Different battery capacity (3000 mAh vs 1450 mAh — BMS tuning differs)
- No ruggedization sensors

---

## Key Kernel Features for ZAKO

### Cgroup Freezer

Used by Outstack (`outstack-powerd`) to freeze process groups when gating:

```bash
# Freeze a cgroup:
echo FROZEN > /sys/fs/cgroup/freezer/<group>/freezer.state

# Thaw:
echo THAWED > /sys/fs/cgroup/freezer/<group>/freezer.state
```

### CPU Frequency Governors

| Governor | Use |
|----------|-----|
| `schedutil` | Default — EAS-driven, balances performance and power |
| `powersave` | Used in Critical Reserve / Emergency modes |
| `performance` | Testing only; not used in production |

```bash
# Check current governor:
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

# Set (root):
echo powersave > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
```

### Thermal Zones

```bash
# List thermal zones:
ls /sys/class/thermal/thermal_zone*/type

# Read CPU temperature:
cat /sys/class/thermal/thermal_zone0/temp
# Value in millidegrees Celsius (e.g., 42000 = 42.0°C)
```

Thermal configuration is in the DTS. Qualcomm `thermal-engine` (vendor daemon) reads trip points and throttles CPU frequency. Outstack reads the same sysfs nodes for its thermal override logic.

### Power Supply sysfs

```bash
# Battery percentage:
cat /sys/class/power_supply/battery/capacity

# Charging status:
cat /sys/class/power_supply/battery/status
# → Charging, Discharging, Full, Not charging

# Current voltage (µV):
cat /sys/class/power_supply/battery/voltage_now

# Current draw (µA, negative = discharging):
cat /sys/class/power_supply/battery/current_now

# Temperature (tenths of °C):
cat /sys/class/power_supply/battery/temp
```

These nodes are the hardware interface that `outstack-powerd` reads for power mode decisions.

---

## Build Integration with AOSP

### Prebuilt Kernel (Default)

```makefile
# device/cat/S22FLIP/BoardConfig.mk
TARGET_PREBUILT_KERNEL := device/cat/S22FLIP/prebuilt/kernel
BOARD_KERNEL_IMAGE_NAME := zImage-dtb
```

### Inline Kernel Build (Future)

```makefile
# Build kernel from source as part of AOSP build:
TARGET_KERNEL_SOURCE := kernel/cat/msm8937
TARGET_KERNEL_CONFIG := msm8937-perf_defconfig
TARGET_KERNEL_CLANG_COMPILE := true
```

---

## Related Documents

- [[02-architecture]] — Kernel choice rationale
- [[device-profile]] — Hardware requiring kernel support
- [[Outstack-Protocol-v1]] — Power governance (kernel-level interfaces)
- Build docs: `CatFlip/build/README.md`
