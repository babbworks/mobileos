# Cat S22 Flip — Security Model

Security architecture for ZAKO OS on the Cat S22 Flip. Covers boot chain trust, verified boot in orange state, encryption, SELinux, hardware key storage, and the Telux-SEC concept.

---

## Boot Chain Trust Boundaries

```
PBL (ROM) → SBL1 (signed) → RPM (signed) → aboot/LK (signed*) → kernel → Android init
```

| Stage | Signed By | Replaceable | Trust |
|-------|-----------|-------------|-------|
| PBL (Primary Boot Loader) | Qualcomm OTP fuses | No | Hardware root of trust |
| SBL1 (Secondary Boot Loader) | Qualcomm | No — signed, fuse-locked | Immutable |
| RPM firmware | Qualcomm | No — signed | Immutable |
| TrustZone (tz.mbn) | Qualcomm | No — signed | Immutable |
| aboot (LK) | OEM (Bullitt/Cat) | No — OEM-signed, cannot re-sign | Immutable in practice |
| boot (kernel + ramdisk) | AVB key (ZAKO project key) | **Yes** — custom images allowed in orange state | Mutable, verified by AVB |
| system / vendor | AVB key | **Yes** — custom images | Mutable, verified |
| userdata | N/A (encrypted, not verified) | Yes | FBE-protected |

**Trust boundary:** Everything below aboot is Qualcomm/OEM-locked and cannot be replaced. From the kernel upward, we control the images. The aboot (LK) verifies the kernel using AVB 2.0 but allows unsigned/differently-signed images in orange state (with a warning).

---

## AVB Orange State

### What It Means

`fastboot oem unlock` sets the device to AVB "orange state":

- Bootloader displays an orange warning screen on every boot (~5 seconds)
- AVB verification of boot/system/vendor still runs but **does not abort on failure**
- dm-verity continues to verify block-level integrity of system/vendor
- The device boots custom-signed images without requiring the OEM key

### Implications for ZAKO

- Orange state is the **production posture** — re-locking with custom keys is not supported on QM215/MSM8937
- The boot warning is cosmetic; it cannot be suppressed without modifying aboot (which is OEM-signed and immutable)
- AVB metadata (`vbmeta` partition) is still populated with ZAKO signing keys to enable dm-verity
- Rollback protection indexes are not enforced in orange state (no anti-rollback)

### Signing Keys

```bash
# Generate AVB signing key (RSA-4096):
openssl genrsa -out zako-avb-key.pem 4096

# Sign vbmeta:
avbtool make_vbmeta_image \
  --algorithm SHA256_RSA4096 \
  --key zako-avb-key.pem \
  --include_descriptors_from_image boot.img \
  --include_descriptors_from_image system.img \
  --include_descriptors_from_image vendor.img \
  --output vbmeta.img
```

---

## dm-verity

dm-verity provides block-level integrity verification for read-only partitions (system, vendor, product).

### Behaviour in Orange State

- dm-verity **remains active** — corruption of system/vendor is detected at read time
- On corruption detection: I/O error returned to the process (EIO), not device wipe
- This protects against storage bit-rot and post-flash tampering
- Does NOT protect against a malicious reflash (attacker with fastboot access can reflash and re-sign)

### Configuration

dm-verity hash trees are embedded in the partition images at build time:

```bash
# Build system.img with verity:
# (handled automatically by AOSP build system when BOARD_AVB_ENABLE := true)
```

Relevant device tree flags:
```makefile
# device/cat/S22FLIP/BoardConfig.mk
BOARD_AVB_ENABLE := true
BOARD_AVB_MAKE_VBMETA_IMAGE_ARGS += --flags 2  # flags=2: hashtree verification in orange state
```

---

## File-Based Encryption (FBE)

ZAKO uses FBE (not Full-Disk Encryption) consistent with Android 11 requirements:

| Property | Value |
|----------|-------|
| Encryption | AES-256-XTS (file contents) + AES-256-CTS (file names) |
| Key derivation | Hardware-backed via Keymaster 4.0 |
| CE (Credential Encrypted) | Unlocked after first user unlock (PIN/pattern) |
| DE (Device Encrypted) | Available at boot (Direct Boot) |

### What's in DE vs CE

| Storage | Encryption Class | Available |
|---------|-----------------|-----------|
| `/data/user_de/` | DE | At boot (before unlock) |
| `/data/user/` | CE | After first unlock |
| `/data/misc/` | DE | At boot |
| App CE storage | CE | After first unlock |

Critical services (telephony, incoming calls) operate in DE mode. User data (messages, contacts, files) is CE-protected.

### Configuration

```makefile
# device/cat/S22FLIP/fstab.qcom
/dev/block/by-name/userdata  /data  ext4  ...,fileencryption=aes-256-xts:aes-256-cts:v2
```

---

## SELinux

ZAKO OS runs SELinux in **enforcing mode** on production builds.

| Build Type | SELinux Mode |
|------------|-------------|
| userdebug | Enforcing (permissive available via `setenforce 0` for debugging) |
| user (release) | Enforcing (cannot be disabled without reflash) |

### Policy Sources

- Base policy: AOSP `system/sepolicy/`
- Vendor policy: `device/cat/S22FLIP/sepolicy/` (vendor-specific HAL contexts)
- ZAKO additions: custom contexts for ZAKO-specific services (ntfy, F-Droid background updater)

### Key Policy Rules

- No `untrusted_app` domain can access raw block devices
- Vendor HAL processes are confined to `hal_*` domains
- ADB shell in userdebug is `shell` domain (limited, no direct vendor access)
- ZAKO custom services run in dedicated domains (not `init` or `system_server`)

---

## Keymaster 4.0 — Hardware Key Storage

The QM215 provides hardware-backed key storage via Keymaster 4.0 running inside TrustZone (QSEE):

| Capability | Status |
|------------|--------|
| RSA key generation (2048, 4096) | Hardware-backed |
| ECDSA (P-256, P-384) | Hardware-backed |
| AES (128, 256) | Hardware-backed |
| HMAC | Hardware-backed |
| Key attestation | Available (Google root; limited utility without GMS) |
| Rollback resistance | Supported |
| Strongbox | Not available (no discrete secure element) |

### Orange State Impact on Keymaster

- Hardware keys remain functional in orange state
- Key attestation chain is rooted in Google's key (not useful for ZAKO; not relied upon)
- Keys marked with `UNLOCKED_DEVICE_REQUIRED` are still accessible (attestation reports unlocked state)
- FBE keys are derived through Keymaster regardless of boot state

### Relevant Blobs

```
/vendor/bin/hw/android.hardware.keymaster@4.0-service-qti
/vendor/lib/libkeymasterdeviceutils.so
/vendor/lib/libkeymasterprovision.so
```

---

## Telux-SEC LSM (Concept)

Telux-SEC is a planned Linux Security Module for ZAKO that enforces HOME-protocol-level access controls at the kernel layer.

### Design Intent

- Enforce capability-based access from the HOME identity system (`telux-identd`) at the kernel level
- Gate file access to ledger storage (`/data/telux/ledger/`) to processes with valid HOME capability tokens
- Provide kernel-level enforcement of Outstack power governance (complement to userspace `outstack-powerd`)
- Hook into `security_file_open`, `security_task_kill`, `security_socket_connect`

### Current Status

- **Not implemented in v1** — SELinux provides adequate mandatory access control for initial release
- Design phase: determining whether LSM stacking (SELinux + Telux-SEC) is feasible on Linux 4.9
- Alternative: implement Telux-SEC controls entirely in userspace via `telux-identd` capability checks

### Prerequisites for Implementation

- LSM stacking support (available in Linux 4.9 via `security_hook_heads` but limited)
- Formal specification of HOME capability tokens that the LSM can verify
- Performance characterization on QM215 (LSM hooks on every syscall add overhead)

---

## Threat Model Summary

| Threat | Mitigation | Residual Risk |
|--------|-----------|---------------|
| Remote code execution | SELinux enforcing, vendor process confinement | Kernel vulnerabilities (4.9 LTS, patching limited) |
| Physical access (fastboot) | FBE protects user data; attacker can reflash OS but not read CE data without PIN | Orange state means device can be reflashed |
| Supply chain (pre-installed malware) | Build from source, reproducible builds, blob audit | Vendor blobs are opaque |
| Network surveillance | No Google endpoints, no telemetry | Carrier can observe unencrypted SMS/USSD |
| Stolen device | FBE + PIN, no bypass without TrustZone exploit | No remote wipe in v1 (no GMS) |

---

## Related Documents

- [[02-architecture]] — Boot chain and verified boot overview
- [[partition-map]] — vbmeta partition location
- [[blob-audit]] — Keymaster and security-related blobs
- [[Outstack-Protocol-v1]] — Power governance (Telux-SEC enforcement target)
