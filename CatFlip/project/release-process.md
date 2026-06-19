# Cat S22 Flip — Release Process

How ZAKO OS builds are signed, tested, versioned, and distributed for the Cat S22 Flip.

---

## Versioning Scheme

```
ZAKO-<major>.<minor>.<patch>-<date>-<variant>
```

| Component | Meaning | Example |
|-----------|---------|---------|
| major | Breaking changes or Android base update | 1 |
| minor | Feature additions | 0 |
| patch | Bug fixes, security patches | 3 |
| date | Build date (YYYYMMDD) | 20260715 |
| variant | Build type | `release`, `beta`, `nightly` |

Example: `ZAKO-1.0.3-20260715-release`

### Release Naming

Releases use Zambian river names alphabetically:

| Version | Codename |
|---------|----------|
| 1.x | Chambeshi |
| 2.x | Dongwe |
| 3.x | Eskoloni |

---

## Build Signing

### Key Hierarchy

| Key | Purpose | Storage |
|-----|---------|---------|
| Platform key (`platform.pk8`) | Signs system apps and framework | Offline, encrypted backup |
| Release key (`releasekey.pk8`) | Signs the OTA package | Offline |
| AVB key (`zako-avb-key.pem`) | Signs vbmeta (dm-verity chain) | Offline |
| Test keys (AOSP default) | userdebug/development only | In-tree (never used for release) |

### Signing a Release Build

```bash
# Build the target-files archive:
m dist -j$(nproc)
# Output: out/dist/zako_S22FLIP-target_files-*.zip

# Sign with release keys:
sign_target_files_apks \
  -o \
  -d vendor/cat/S22FLIP/keys/ \
  out/dist/zako_S22FLIP-target_files-*.zip \
  signed-target_files.zip

# Generate signed images from target-files:
img_from_target_files signed-target_files.zip signed-images.zip
```

---

## OTA Generation

### Full OTA

```bash
ota_from_target_files \
  -k vendor/cat/S22FLIP/keys/releasekey \
  signed-target_files.zip \
  ota_full.zip
```

### Incremental OTA

```bash
ota_from_target_files \
  -k vendor/cat/S22FLIP/keys/releasekey \
  -i previous-signed-target_files.zip \
  signed-target_files.zip \
  ota_incremental.zip
```

### OTA Distribution

- OTA packages are hosted on ZAKO infrastructure (not Google OTA servers)
- Device checks for updates via a configured HTTPS endpoint
- Update mechanism: AOSP `update_engine` (or RecoverySystem for single-slot)
- No GMS dependency for OTA delivery

---

## Testing Checklist

### Pre-Release Gate (all must pass)

#### Boot & Stability

- [ ] Device boots to home screen from cold flash
- [ ] Device boots to home screen from OTA update
- [ ] No crash loops in first 30 minutes of operation
- [ ] `adb shell getprop sys.boot_completed` returns 1 within 90 seconds

#### Telephony — Live SIM Testing

| Test | MTN | Airtel | Zamtel |
|------|-----|--------|--------|
| Voice call (MO + MT) | [ ] | [ ] | [ ] |
| SMS send/receive | [ ] | [ ] | [ ] |
| USSD (*#100#, balance check) | [ ] | [ ] | [ ] |
| STK (SIM Toolkit menu) | [ ] | [ ] | [ ] |
| Mobile Money USSD (*XXX#) | [ ] | [ ] | [ ] |
| Data (LTE attach, browsing) | [ ] | [ ] | [ ] |
| APN auto-configuration | [ ] | [ ] | [ ] |

#### Hardware

- [ ] Lid open/close detected (`SW_LID` events)
- [ ] Cover display shows notifications when lid closed
- [ ] Physical keypad all keys functional
- [ ] Proximity sensor blanks screen during call
- [ ] Accelerometer reports orientation changes
- [ ] WiFi connects and transfers data
- [ ] Bluetooth pairs and streams audio
- [ ] Camera captures photo (rear)
- [ ] Speaker/earpiece/mic audio path

#### Security

- [ ] SELinux enforcing, zero `avc: denied` in dmesg
- [ ] FBE encryption active (`adb shell getprop ro.crypto.state` → `encrypted`)
- [ ] dm-verity active on system and vendor
- [ ] No Google endpoints contacted over 24-hour capture (see [[de-googling]])

#### Power

- [ ] Idle power draw <100mW with SIM registered (measured over 8 hours)
- [ ] Outstack mode transitions at correct thresholds
- [ ] Device survives 24h standby on full charge (target: 48h+)

#### OTA

- [ ] Full OTA installs successfully from previous release
- [ ] Incremental OTA installs successfully
- [ ] OTA failure triggers rollback to previous state (or recovery)
- [ ] User data preserved across OTA

---

## Release Build Procedure

1. Tag the manifest with version: `git tag ZAKO-1.0.3-20260715-release`
2. Sync from tag: `repo init -b ZAKO-1.0.3-20260715-release && repo sync`
3. Build user target: `lunch zako_S22FLIP-user && m dist -j$(nproc)`
4. Sign target-files with release keys
5. Generate full OTA and incremental OTA (from previous release)
6. Run full testing checklist
7. Upload OTA to distribution server
8. Update OTA manifest (version, URL, SHA-256)
9. Tag all component repos with the release version

---

## Distribution Method

- **New devices:** Pre-flashed via fastboot before deployment
- **Existing devices:** OTA update via ZAKO update server (HTTPS)
- **Manual update:** Sideload OTA via `adb sideload ota_full.zip` in recovery
- **Fallback:** Distribute `.img` files for manual fastboot flash

### Update Server Configuration

```
URL: https://updates.zako.[domain]/catflip/
Manifest: /catflip/update.json
```

```json
{
  "version": "ZAKO-1.0.3-20260715-release",
  "url": "https://updates.zako.[domain]/catflip/ota_full.zip",
  "sha256": "<hash>",
  "size": 524288000,
  "incremental_from": "ZAKO-1.0.2-20260701-release",
  "incremental_url": "https://updates.zako.[domain]/catflip/ota_incr_102_103.zip",
  "incremental_sha256": "<hash>"
}
```

---

## Related Documents

- [[contributor-guide]] — Build and flash instructions
- [[02-architecture]] — Build system and signing overview
- [[security-model]] — AVB keys and dm-verity
- [[de-googling]] — Verification of no Google OTA dependency
