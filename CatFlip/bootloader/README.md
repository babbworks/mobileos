# Cat S22 Flip — Bootloader

LK (Little Kernel) bootloader on the QM215/MSM8937 platform. Covers the boot chain, unlock procedure, orange state behaviour, and what's replaceable.

---

## Boot Chain

```
PBL → SBL1 → RPM → TZ → aboot (LK) → kernel → Android init
```

| Stage | Image | Partition | Source | Replaceable |
|-------|-------|-----------|--------|-------------|
| PBL | Mask ROM | — (silicon) | Qualcomm | No |
| SBL1 | `sbl1.mbn` | `sbl1` | Qualcomm (signed, fuse-bound) | No |
| RPM | `rpm.mbn` | `rpm` | Qualcomm (signed) | No |
| TrustZone | `tz.mbn` | `tz` | Qualcomm (signed) | No |
| Aboot (LK) | `emmc_appsboot.mbn` | `aboot` | OEM-signed (Bullitt/Cat) | No — cannot re-sign |
| Boot image | `boot.img` | `boot` | ZAKO project | **Yes** |
| Recovery | `recovery.img` | `recovery` | ZAKO project | **Yes** |
| vbmeta | `vbmeta.img` | `vbmeta` | ZAKO project | **Yes** |

**Key insight:** Everything from PBL through aboot is immutable. We cannot modify or replace the bootloader itself. We control everything from the kernel upward.

---

## LK (Little Kernel) on QM215

LK is Qualcomm's bootloader for MSM8937-class devices. On the Cat S22 Flip it:

1. Initializes display (shows splash screen from `splash` partition)
2. Reads `vbmeta` partition for AVB 2.0 metadata
3. Verifies `boot.img` against AVB chain
4. In orange state: logs verification failure but proceeds to boot
5. Loads kernel + ramdisk from `boot` partition into RAM
6. Passes device tree and kernel command line
7. Jumps to kernel entry point

LK also handles:
- Fastboot protocol (USB)
- Recovery mode selection (key combo or BCB in `misc` partition)
- Charging animation (off-mode charging)

---

## Unlock Procedure

### Prerequisites

- USB cable and host with `fastboot` installed
- Data backup (unlock wipes userdata)
- ADB access to running device

### Steps

```bash
# 1. Enable OEM unlocking in Developer Options:
#    Settings → About Phone → tap Build Number 7 times
#    Settings → Developer Options → OEM Unlocking → Enable

# 2. Reboot to bootloader:
adb reboot bootloader

# 3. Unlock:
fastboot oem unlock

# 4. Confirm on device (use volume keys to select, power to confirm)
# Device wipes userdata and reboots

# 5. Verify unlock state:
fastboot getvar unlocked
# Expected: unlocked: yes
```

### Post-Unlock State

- AVB state changes to **orange**
- Boot shows orange warning: "Your device has been unlocked and cannot be trusted"
- Warning displays for ~5 seconds, then boot continues normally
- `ro.boot.verifiedbootstate=orange` is set in kernel cmdline
- FBE and dm-verity continue to function

### Re-Lock Warning

**Do not re-lock the bootloader after flashing custom images.** Re-locking with non-OEM-signed images in the boot/system partitions will **brick the device** (aboot will refuse to boot unverified images in locked/green state). QM215 does not support custom AVB key enrollment for re-locking.

---

## Orange State — Detailed Behaviour

### What Works

| Feature | Status in Orange State |
|---------|----------------------|
| dm-verity (system/vendor integrity) | Active — verifies against ZAKO AVB key in vbmeta |
| FBE (file-based encryption) | Active — Keymaster 4.0 functions normally |
| Keymaster 4.0 | Functional — hardware keys work |
| SELinux | Enforcing (set by kernel/ramdisk, not bootloader) |
| Rollback protection | **Disabled** — rollback index not enforced |
| Key attestation | Reports `verifiedBootState=orange` (not `green`) |

### What Doesn't Work

| Feature | Status |
|---------|--------|
| Verified boot abort on failure | Disabled (boot proceeds regardless of verification result) |
| Anti-rollback index enforcement | Not checked; any older build can be flashed |
| Green/yellow boot state | Cannot be achieved with custom images |
| Hiding unlock status from apps | Not possible (`ro.boot.verifiedbootstate` is readable) |

### Security Implications

An attacker with physical access and a USB cable can:
- Flash any boot/system image (device is in orange state)
- Boot into any custom recovery
- **Cannot** read FBE-encrypted userdata without the user's PIN/pattern

This is acceptable for the ZAKO threat model. FBE protects data at rest.

---

## dm-verity in Orange State

dm-verity is configured via the `boot.img` fstab and `vbmeta.img`:

```
# In fstab.qcom (inside ramdisk):
/dev/block/by-name/system  /  ext4  ro,barrier=1  wait,verify
/dev/block/by-name/vendor  /vendor  ext4  ro,barrier=1  wait,verify
```

The AVB footer in each partition image contains the hash tree. `vbmeta.img` (signed with our key) chains these together. Because we sign `vbmeta` with the ZAKO AVB key and the bootloader is in orange state, it accepts our signature without checking against an OEM root.

Corruption detection:
- If a block on system/vendor is corrupted → EIO returned to the reading process
- System does not reboot or wipe; the corrupted file simply fails to read
- This catches storage bit-rot, not deliberate reflash attacks

---

## Fastboot Reference

```bash
# Enter fastboot mode:
adb reboot bootloader
# Or: hold Volume Down + Power during boot

# Key commands:
fastboot getvar all                  # dump device info
fastboot getvar unlocked             # check lock state
fastboot flash boot boot.img         # flash kernel
fastboot flash system system.img     # flash system (non-dynamic)
fastboot flash vbmeta vbmeta.img     # flash AVB metadata
fastboot erase userdata              # wipe user data
fastboot reboot                      # reboot to system
fastboot reboot fastboot             # reboot to fastbootd (dynamic partitions)

# Dangerous — do NOT flash these with custom images:
# fastboot flash sbl1 ...   ← BRICK RISK
# fastboot flash tz ...     ← BRICK RISK
# fastboot flash rpm ...    ← BRICK RISK
# fastboot flash aboot ...  ← BRICK RISK
```

---

## EDL (Emergency Download) Recovery

If the device is bricked (bad aboot, corrupted SBL1), EDL mode is the last resort:

```bash
# Enter EDL: hold Volume Up + Volume Down + insert USB cable
# (or short test points on PCB if buttons non-functional)

# Use Qualcomm EDL tool (requires device-specific loader):
edl --loader=prog_emmc_firehose_8937_ddr.mbn
edl printgpt                         # verify partition table
edl w [partition] [image]            # write partition
```

EDL requires a firehose programmer binary specific to the MSM8937 platform. This binary is OEM-restricted but available through firmware archives.

---

## Related Documents

- [[partition-map]] — Full partition layout including boot-chain partitions
- [[security-model]] — AVB, dm-verity, and trust boundaries
- [[02-architecture]] — Boot chain overview
