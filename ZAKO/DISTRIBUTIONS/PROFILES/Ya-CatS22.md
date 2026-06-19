# ZAKO Distribution Profile — Ya on Cat S22 Flip

---

## Identity

| Field | Value |
|---|---|
| Distribution name | Ya |
| Distribution codename | BabbCat |
| ZAKO standard version | v1 |
| Status | Active Development |
| Profile last updated | 2026-06-02 |

---

## Target Device

| Field | Value |
|---|---|
| Device name | Bullitt Cat S22 Flip |
| Device codename | S22FLIP |
| Manufacturer | Bullitt Group / CAT |
| SoC | Qualcomm MSM8937 (Snapdragon 215 / QM215) — Cortex-A53 × 8 |
| Architecture | arm (32-bit userspace, 64-bit capable kernel) |
| RAM | 2GB LPDDR3 |
| Storage | 16GB eMMC |
| Display | 4.0" internal (main) + 1.44" external (cover) |
| Battery | 1450mAh |
| Form factor | Flip (clamshell) |
| Device folder | MobileOS/CatFlip |

---

## Base OS

| Field | Value |
|---|---|
| Base | AOSP |
| Android version | Android 11 |
| AOSP branch/tag | android-11.0.0_r46 |
| Kernel version | Linux 4.9 — CAF tag LA.UM.10.6.2.r1-02500-89xx.0 |
| Kernel source | MobileOS/CatFlip/repos/kernel/redmi-go (primary reference) |
| Go Edition | Yes (Android 11 Go — 2GB/16GB device) |
| Build target | babb_S22FLIP-user |

---

## Target Market

| Field | Value |
|---|---|
| Primary region | Zambia |
| Primary language(s) | English, Nyanja, Bemba, Tonga |
| Carriers | Airtel Zambia, MTN Zambia, Zamtel |
| Mobile money platforms | Airtel Money (`*778#`), MTN MoMo (`*303#`), Zamtel Kwacha |
| Connectivity profile | LTE-dominant in urban Lusaka; 3G/2G fallback in rural areas |
| Climate considerations | High heat and humidity; dusty environments. Device is IP68 rated — leverage this |

---

## Outstack Power Profile

This device has a 1450mAh battery — the smallest battery in the ZAKO device portfolio. Power governance is the primary engineering constraint.

| Mode | Battery threshold | CPU governor | Background process policy | Display timeout |
|---|---|---|---|---|
| Full Power | >80% | schedutil | All processes permitted | 60s |
| Standard | 40–80% | schedutil + core parking | Non-critical deferred | 30s |
| Conservation | 20–40% | powersave | Only telephony + Telux exchange | 20s |
| Critical Reserve | 5–20% | powersave (1 core) | Telephony only | 10s |
| Emergency | <5% | powersave (1 core) | Radio standby only; display off unless active call | 5s |

Battery-specific constraints:
- Battery capacity: 1450mAh
- Target standby: 5+ days (LTE eDRX with up to 327-second paging cycles)
- Target active: 6+ hours mixed voice/data use
- LTE standby target: <100mW
- Deep idle power floor target: <50mW

eDRX configuration (Standard and Conservation modes):
```
# system.prop:
ro.ril.edrx.enable=1
ro.ril.edrx.ptw=2048   # 20.48s paging time window
ro.ril.edrx.cycle=8192 # 327.68s eDRX cycle — longest supported
```

---

## Telux Configuration

| Field | Value |
|---|---|
| Identity model | W3C DID (did:key default, did:web planned) |
| Island configuration | Single personal Island per device by default |
| Carrier layer | SMS (primary), IP (secondary), QR (out-of-band identity exchange) |
| Captive portal endpoint | http://204.babb.tel/generate_204 |
| Push relay | ntfy (self-hosted at babb.tel infrastructure) |
| NTP server | africa.pool.ntp.org |

Carrier configs:

```
Carrier: Airtel Zambia
MCC/MNC: 64504
APN name: airtelweb
APN type: default,mms,supl
VoLTE: Under investigation (see GAP-003 in CatFlip/project/gaps-and-blind-spots.md)
USSD mobile money: *778# (Airtel Money)
RCS: Not supported

Carrier: MTN Zambia
MCC/MNC: 64501
APN name: internet
APN type: default,mms,supl
VoLTE: Under investigation
USSD mobile money: *303# (MTN MoMo)
RCS: Not supported

Carrier: Zamtel
MCC/MNC: 64503
APN name: zamtel
APN type: default
VoLTE: Not supported
USSD mobile money: *338# (Zamtel Kwacha)
RCS: Not supported
```

---

## ZAKO Services

| Service | Status | Notes |
|---|---|---|
| PADS record model | Active | Core use case — field worker exchange records |
| Agreements service | Active — v1 | Basic bilateral agreements |
| Health service | Deferred | Post-v1 |
| Academy service | Deferred | Post-v1 |
| BitPads accounting core | Active | Underlies all Telux exchange |
| Telux identity/exchange | Active | Primary sovereignty layer |
| Outstack power governance | Active | Critical for 1450mAh battery |

---

## Security Configuration

| Field | Value |
|---|---|
| Boot state | ORANGE (unlocked bootloader — no custom key injection on MSM8937/LK) |
| AVB custom key injection | Not supported on this bootloader |
| dm-verity | Disabled in dev (--flags 3); Active in production |
| FBE | Active — hardware ICE (Qualcomm Inline Crypto Engine) |
| SELinux | Enforcing in production; Permissive permitted during bring-up only |
| Widevine level | L3 (consequence of unlocked bootloader — acceptable for Zambia deployment) |
| Keymaster version | 4.0 (android.hardware.keymaster@4.0-service-qti) |
| Key storage path | ~/keys/s22flip/ — stored outside build tree, never committed |

---

## Build Key Slots

| Key | Slot name | Storage location |
|---|---|---|
| AVB root key | ya-s22flip-avb-root | ~/keys/s22flip/avb_key.pem |
| AVB system chain key | ya-s22flip-avb-system | ~/keys/s22flip/avb_system_key.pem |
| Platform signing key | ya-s22flip-platform | ~/keys/s22flip/platform.pk8 |
| Release key | ya-s22flip-release | ~/keys/s22flip/releasekey.pk8 |

---

## OTA and Distribution

| Field | Value |
|---|---|
| OTA server | https://update.babb.tel/babb-s22flip/ |
| Update check interval | 24 hours |
| Full OTA naming | ota-babb-s22flip-VERSION-DATE-full.zip |
| Delta OTA supported | Yes — important for Zambia metered data |
| GPL source publication | github.com/babb-os/android_kernel_cat_s22flip (planned) |
| F-Droid repo | Not maintained separately — direct users to f-droid.org |

---

## Known Gaps and Limitations

Gaps document: `MobileOS/CatFlip/project/gaps-and-blind-spots.md`

| ID | Description | Severity | Status |
|---|---|---|---|
| GAP-001 | STK / USSD not verified — critical for Zambia mobile money | Critical | Open |
| GAP-002 | De-Googled Setup Wizard — stock wizard calls home | High | Open |
| GAP-003 | VoLTE / IMS stack unclear on this CAF kernel | High | Open |
| GAP-004 | AGPS / SUPL server must be replaced | Medium | Open |
| GAP-005 | Cover display (1.44") — proprietary driver, likely non-functional | Medium | Accepted |

---

## Notes

- This is the first ZAKO distribution. Everything proved here establishes the template for subsequent devices.
- The Cat S22 Flip was chosen specifically for its ruggedized build (IP68), flip form factor (physical call button habit familiar to Zambia users), and hardware availability.
- BabbCat is to ZAKO what early Ubuntu was to Linux — the first concrete device proof of the architecture.
- Full implementation plan: `MobileOS/CatFlip/project/BabbCat-Implementation-Plan.md`
