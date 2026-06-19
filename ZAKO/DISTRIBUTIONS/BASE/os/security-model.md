# Security Model — ZAKO Distribution Base

The universal security baseline for all ZAKO distributions. Covers the standard trust chain model, AVB approach, SELinux policy, encryption, and hardening decisions.

Distribution-specific deviations (bootloader lock state, hardware-specific Keymaster version, device-specific threat model) belong in the distribution's own `os/security-model.md`.

---

## Trust Chain Architecture

The general ZAKO trust chain follows the standard Android Verified Boot 2.0 model:

```
Hardware Root of Trust (OEM-burned, permanent)
└── Secure Bootloader (OEM-signed — cannot be replaced)
    └── TrustZone / TEE (OEM-signed trustlets)
        └── Bootloader (aboot / LK / UEFI)
            └── AVB 2.0 verification
                ├── boot.img       (signed with distribution AVB key)
                ├── dtbo.img       (signed)
                └── vbmeta.img     (root — signed)
                    └── dm-verity
                        ├── system partition  (hashtree-verified at runtime)
                        ├── vendor partition  (hashtree-verified)
                        └── product partition (hashtree-verified)
```

The weakest point varies by device. Document the specific limitation in each distribution's security-model.md — in particular whether AVB custom key injection is supported.

---

## Boot States

| State | Color | Meaning | Action required |
|---|---|---|---|
| GREEN | Green | Locked, OEM-verified | — |
| YELLOW | Yellow | Locked, custom key verified | Achievable if device supports custom key injection |
| ORANGE | Orange | Bootloader unlocked | Default for most custom OS builds |
| RED | Red | Verification failed | Investigate immediately — do not ship |

**Target for production:** YELLOW where the bootloader supports custom key injection. ORANGE is acceptable on hardware where YELLOW is not achievable — document this explicitly in the distribution's security notes.

All other security layers (dm-verity, FBE, SELinux, Keymaster) remain fully functional in ORANGE state.

---

## AVB Configuration

### Development Configuration

```makefile
# BoardConfig.mk — development mode:
BOARD_AVB_ENABLE                  := true
BOARD_AVB_MAKE_VBMETA_IMAGE_ARGS  += --flags 3
# flags=3: disable-verity + disable-verification
# Iterate quickly without re-signing every build
```

### Production Configuration

```makefile
# BoardConfig.mk — production:
BOARD_AVB_ENABLE                  := true
BOARD_AVB_ALGORITHM               := SHA256_RSA4096
BOARD_AVB_KEY_PATH                := [PATH_TO_KEYS]/avb_key.pem

# Chained vbmeta for system/vendor/product:
BOARD_AVB_VBMETA_SYSTEM           := system vendor product
BOARD_AVB_VBMETA_SYSTEM_KEY_PATH  := [PATH_TO_KEYS]/avb_system_key.pem
BOARD_AVB_VBMETA_SYSTEM_ALGORITHM := SHA256_RSA4096
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX_LOCATION := 1
```

### AVB Key Set (per distribution)

```
[distro]-keys/
├── avb_key.pem           — Root AVB key (RSA-4096)
├── avb_key.x509.pem      — Root AVB certificate
├── avb_system_key.pem    — System chain key
├── avb_boot_key.pem      — Boot chain key
├── platform.pk8          — APK signing: platform key
├── shared.pk8            — APK signing: shared key
├── media.pk8             — APK signing: media key
├── networkstack.pk8      — APK signing: network stack key
└── releasekey.pk8        — Default release key
```

**Critical:** Keys must be stored outside the build tree, never committed to git. Back up in at least two physical locations.

### Rollback Index Policy

```makefile
# Development builds:
BOARD_AVB_ROLLBACK_INDEX := 0

# First production release:
BOARD_AVB_ROLLBACK_INDEX := 1

# Increment with each production release.
# Once index N ships, downgrade to N-1 is permanently blocked.
# Document index in release notes. Never decrement.
```

---

## dm-verity

dm-verity verifies every 4KB block of the system/vendor/product partitions at runtime using a hash tree stored in `vbmeta.img`. Any modification to a verified partition causes an I/O error.

Enable FEC (Forward Error Correction) for production to protect against storage corruption:

```bash
avbtool add_hashtree_footer \
  --image system.img \
  --partition_name system \
  --partition_size $((SYSTEM_PARTITION_SIZE_BYTES)) \
  --key [PATH_TO_KEYS]/avb_system_key.pem \
  --algorithm SHA256_RSA4096 \
  --generate_fec \
  --fec_num_roots 2
```

---

## File-Based Encryption (FBE)

Userdata partition must be encrypted with FBE. Use hardware ICE (Inline Crypto Engine) if the SoC supports it:

```
# fstab entry:
userdata  /data  f2fs  fileencryption=ice  ...
# or for software encryption:
userdata  /data  ext4  fileencryption=aes-256-xts  ...
```

Key hierarchy:
```
Hardware-backed master key (in TEE, never exposed to Android)
    ↓ (Keymaster)
Per-credential key (PIN/password/pattern derived)
    ↓
Per-user directory encryption key
    ↓
Per-file key (AES-256-XTS content, AES-256-CTS filenames)
```

Verify FBE is active after first boot:
```bash
adb shell getprop ro.crypto.state   # should return: encrypted
adb shell getprop ro.crypto.type    # should return: file
```

---

## SELinux Policy

All production builds ship with SELinux in **enforcing** mode. This is non-negotiable.

```bash
# Verify on running device:
adb shell getenforce     # must return: Enforcing
```

### Development vs Production

| Mode | Config | When |
|---|---|---|
| Permissive | `androidboot.selinux=permissive` in kernel cmdline | Early bring-up debugging only |
| Enforcing | Default — no override | All production builds |

Never ship a build with `androidboot.selinux=permissive` in the kernel cmdline.

### Handling Denials

When a custom feature triggers SELinux denials during bring-up:

```bash
# View denials:
adb shell dmesg | grep "avc: denied"

# Generate candidate policy rules (for review — do not blindly apply):
adb shell dmesg | grep "avc: denied" | \
  audit2allow -p out/target/product/[DEVICE_CODENAME]/obj/ETC/sepolicy_intermediates/policy

# Add reviewed rules to device sepolicy:
# device/[DEVICE_VENDOR]/[DEVICE_CODENAME]/sepolicy/*.te
```

Review generated rules carefully — `audit2allow` generates permissive rules, not secure ones.

---

## Network Security

### TLS

Set minimum TLS version to 1.2 system-wide:

```
# system.prop:
security.tls.minimum_version=2    # 0=1.0, 1=1.1, 2=1.2, 3=1.3
```

### Certificate Store

The default Android CA store contains 130+ certificates. For most deployments, the default store is acceptable. A hardening pass can remove government-operated CAs or CAs with known compromise history — document this decision in the distribution's own security notes.

### Private DNS (DNS-over-TLS)

Configure Private DNS in system default settings. Point to a self-hosted or privacy-preserving resolver. See `os/de-googling.md` for DNS configuration.

---

## Keymaster and Hardware-Backed Keys

If the device has Keymaster 3.0 or later with a TEE (TrustZone or equivalent), keys generated with hardware backing are stored in the TEE and never exposed to the Android kernel.

This provides:
- Secure storage for FBE master keys
- Hardware-backed Android Keystore (used by apps for credential storage)
- Attestation (weakened in ORANGE boot state — attestation will reflect unlocked status)

---

## Widevine DRM

With an unlocked bootloader, Widevine downgrades from L1 to L3:
- L1: hardware-protected video decode (HD/4K streaming)
- L3: software-only (SD streaming)

This is an acceptable trade-off in almost all ZAKO target markets. If L1 is required, the only path is bootloader re-lock with OEM images, which removes the custom OS.

---

## Google Services Security Implications

See `os/de-googling.md` for the full GMS removal procedure. Security-relevant effects:

- **Google Play Protect:** Not present. Use F-Droid (curated FOSS only) as the app source. Optionally add Hypatia (open-source malware scanner) as a privileged system app.
- **SafetyNet / Play Integrity:** Fails with unlocked bootloader. Banking apps requiring attestation will not function. Document this in release notes.
- **Google Account:** Not available. Local identity and Telux DIDs replace it.

---

## Security Hardening Checklist (Per Release)

```
Kernel:
[ ] SELinux enforcing confirmed (getenforce = Enforcing)
[ ] CONFIG_SECURITY_SELINUX=y in defconfig
[ ] No androidboot.selinux=permissive in kernel cmdline

AVB:
[ ] All partition images AVB-signed with distribution keys (not AOSP test keys)
[ ] vbmeta.img built with --flags 0 for production
[ ] Rollback index incremented from previous production release
[ ] dm-verity active on system/vendor/product

Encryption:
[ ] FBE active: ro.crypto.state = encrypted
[ ] Hardware ICE active (if device supports it)

Network:
[ ] Private DNS configured
[ ] TLS 1.2 minimum enforced
[ ] No Google telemetry endpoints reachable (tcpdump audit)

Apps:
[ ] No GMS packages installed
[ ] Only F-Droid or verified sources for preloads

Keys:
[ ] Build signed with distribution release keys (not AOSP test keys)
[ ] AVB signing keys stored outside build tree
[ ] Key fingerprints documented for release verification
```
