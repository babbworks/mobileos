# Contributor Guide — ZAKO Distribution Base

How to set up the build environment, make changes, test them, and submit patches. Written for contributors comfortable with Linux and Android development. Adapt device-specific component names and paths for each distribution.

---

## Prerequisites

**Host machine:**
- Linux (Ubuntu 22.04 LTS or 20.04 LTS recommended)
- 32GB RAM, 300GB free disk (minimum: 16GB RAM, 250GB disk)
- Broadband connection for initial sync

See `build/build-environment.md` for full setup including ADB udev rules, repo tool, and AOSP sync.

---

## Making Changes

### Device Tree Changes

Device tree changes live in `device/[DEVICE_VENDOR]/[DEVICE_CODENAME]/`. Common change types:

```bash
# Add a system property:
echo "ro.[distro].feature.xyz=true" >> device/[DEVICE_VENDOR]/[DEVICE_CODENAME]/system.prop

# Add a package (in device.mk):
PRODUCT_PACKAGES += \
    MyNewApp

# After device tree changes — rebuild:
m systemimage    # for property/package changes
m bootimage      # if BoardConfig.mk or kernel parameters changed
```

### Kernel Changes

Always work in a branch:

```bash
cd kernel/[DEVICE_VENDOR]/[DEVICE_CODENAME]
git checkout -b my-feature

# Build and test-boot without flashing:
cd ~/aosp
m kernel && m bootimage
fastboot boot out/target/product/[DEVICE_CODENAME]/boot.img
```

If the test boot looks good, commit the kernel change. Every non-trivial kernel change must be logged in `kernel-patches.md` — see format below.

### DTS Changes

After modifying a `.dts` or `.dtsi` file:

```bash
# Verify DTS compiles cleanly:
make ARCH=[arm|arm64] dtbs 2>&1 | grep -E "error|warning"

# Rebuild boot image with new DTB:
m bootimage

# Test-boot and verify:
fastboot boot out/target/product/[DEVICE_CODENAME]/boot.img
```

---

## Coding Standards

### General

- Follow the style of the surrounding code
- Prefer minimal changes — do not refactor while fixing
- Do not add code for hypothetical future requirements

### Kernel Code

- Follow Linux kernel coding style (`Documentation/process/coding-style.rst`)
- Use `checkpatch.pl` before submitting:
  ```bash
  ./scripts/checkpatch.pl --no-tree --ignore LONG_LINE 0001-my-patch.patch
  ```
- Tabs, not spaces (kernel uses tabs)

### Android Makefiles

- Use `PRODUCT_PACKAGES +=` (not `=` which overwrites)
- Use `PRODUCT_PROPERTY_OVERRIDES +=` for properties

### DTS

- Follow DTS style of existing Qualcomm or vendor files in `arch/arm[64]/boot/dts/`
- Document GPIO assignments with source (e.g., `/* GPIO 91 from stock DTB */`)

---

## kernel-patches.md Entry Format

Every non-trivial kernel change must be logged:

```markdown
### [PATCH-NNN] Short description

- **Applied to:** [kernel baseline name]
- **Status:** Applied
- **Type:** Bug fix / Feature / Device-specific / Workaround
- **Upstream:** Yes / No / N/A
- **Files:** path/to/changed/file.c
- **Why:** Why this change was needed
- **Notes:** Caveats, related issue numbers
```

---

## Commit Message Format

```
component: Short description (imperative, < 72 chars)

Longer explanation if the title is not self-explanatory. Wrap at 72
characters. Explain what problem existed before and why this change
is the correct fix — not just what the change does.

Fixes: ISSUE-NNN
Tested-on: [DEVICE_NAME] ([boot state], [Android version])
```

Examples:

```
arm: dts: msm8937-[device]: disable secondary display

Secondary display driver is not available in the open kernel.
Without this disable, MDSS panics during init attempting to
configure an unknown display controller.

Fixes: ISSUE-001
Tested-on: [DEVICE_NAME] (bootloader unlocked, Android 11)
```

```
configs: [DEVICE_CODENAME]_defconfig: unset ANDROID_BINDER_IPC_32BIT

TARGET_USES_64_BIT_BINDER requires 64-bit binder ABI even on 32-bit
userspace. IPC_32BIT conflicts and causes binder failures at runtime.

Fixes: PATCH-003
```

---

## Submitting Patches

1. Check `known-issues.md` — is the bug documented? If not, add it first.
2. Create a branch from the distribution's main branch:
   ```bash
   git checkout zako-[android_version]
   git pull
   git checkout -b fix/issue-NNN-short-description
   ```
3. Make changes, commit with correct message format
4. Run tests relevant to the change (from `project/test-plan-template.md`)
5. Update `project/regression-log.md` or `kernel/kernel-patches.md` as appropriate
6. Open a pull request to the main branch

---

## Testing Before Submitting

```bash
# 1. Build the affected component:
m bootimage    # kernel/DTS changes
m systemimage  # device tree/property changes

# 2. Test-boot (reverts on next reboot — safe way to test):
adb reboot bootloader
fastboot boot out/target/product/[DEVICE_CODENAME]/boot.img

# 3. Run relevant tests from test-plan-template.md

# 4. Essential checks after any kernel change:
adb shell uname -r                  # confirm correct kernel version
adb shell getenforce                # confirm Enforcing
adb shell getprop ro.crypto.state   # confirm encrypted

# 5. Document results in regression-log.md
```

---

## Debugging

### Kernel Boot Failures

```bash
# Check for kernel panic immediately after flashing:
adb shell dmesg | tail -100

# If ADB not accessible (early panic), use UART if available.

# Fall back to a known-good kernel to confirm device hardware is OK:
fastboot boot [path-to-prebuilt-kernel]

# Then bisect between working and broken change.
```

### SELinux Denials

```bash
adb shell dmesg | grep "avc: denied"

# Generate candidate allow rules (review before applying — do not blindly ship):
adb shell dmesg | grep "avc: denied" | \
  audit2allow -p out/target/product/[DEVICE_CODENAME]/obj/ETC/sepolicy_intermediates/policy
```

### WiFi

```bash
adb shell dmesg | grep -i wlan
adb shell wpa_cli status
adb shell ip addr show wlan0
```

### Audio

```bash
adb shell dmesg | grep -i audio
adb shell lsmod | grep audio
adb shell tinymix | head -20
adb shell tinyplay /sdcard/test.wav
```
