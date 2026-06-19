# ZAKO Distribution Profile — Template

*Copy this file to ZAKO/DISTRIBUTIONS/PROFILES/[DISTRO_NAME]-[DEVICE_CODENAME].md and fill in all sections.*
*Fields marked [REQUIRED] must be completed before any build begins. Fields marked [PRE-RELEASE] must be completed before the first public release.*

---

## Identity

| Field | Value |
|---|---|
| Distribution name | [DISTRO_NAME] |
| Distribution codename | [DISTRO_CODENAME] |
| ZAKO standard version | [e.g., v1] |
| Status | Draft / Active Development / Beta / Released |
| Profile last updated | [DATE] |

---

## Target Device [REQUIRED]

| Field | Value |
|---|---|
| Device name | [DEVICE_NAME] |
| Device codename | [DEVICE_CODENAME] |
| Manufacturer | [MANUFACTURER] |
| SoC | [SOC — e.g., Qualcomm MSM8937] |
| Architecture | [e.g., arm, arm64] |
| RAM | [GB] |
| Storage | [GB] |
| Display | [dimensions, type] |
| Battery | [mAh] |
| Form factor | [bar / flip / slider / other] |
| Device folder | [relative path to distribution folder, e.g., MobileOS/CatFlip] |

---

## Base OS [REQUIRED]

| Field | Value |
|---|---|
| Base | [AOSP / LineageOS / other] |
| Android version | [e.g., Android 11] |
| AOSP branch/tag | [e.g., android-11.0.0_r46] |
| Kernel version | [e.g., Linux 4.9 CAF] |
| Kernel source | [URL or internal path] |
| Go Edition | [Yes / No] |
| Build target | [e.g., [DISTRO_CODENAME]_[DEVICE_CODENAME]-user] |

---

## Target Market [REQUIRED]

| Field | Value |
|---|---|
| Primary region | [country/region] |
| Primary language(s) | [list] |
| Carriers | [list of carrier names] |
| Mobile money platforms | [list, if applicable] |
| Connectivity profile | [e.g., LTE-dominant, 3G fallback, WiFi-heavy] |
| Climate considerations | [e.g., high heat+humidity, dusty environments] |

---

## Outstack Power Profile [REQUIRED]

Document how this distribution configures Outstack's five power modes for this specific device. Reference `ZAKO/PROTOCOLS/Outstack-Protocol-v1.md`.

| Mode | Battery threshold | CPU governor | Background process policy | Display timeout |
|---|---|---|---|---|
| Full Power | [>N%] | [governor] | [policy] | [seconds] |
| Standard | [N–M%] | [governor] | [policy] | [seconds] |
| Conservation | [N–M%] | [governor] | [policy] | [seconds] |
| Critical Reserve | [N–M%] | [governor] | [policy] | [seconds] |
| Emergency | [<N%] | [governor] | [policy] | [seconds] |

Battery-specific constraints:
- Battery capacity: [mAh]
- Target standby: [days] / [hours LTE standby]
- Target active: [hours voice call]
- Deep idle power floor: [mW target]

---

## Telux Configuration [REQUIRED]

Reference `ZAKO/PROTOCOLS/Telux-Protocol-v1.md`.

| Field | Value |
|---|---|
| Identity model | [W3C DID / local / hybrid] |
| Island configuration | [default island name/type] |
| Carrier layer | [SMS / BLE / QR / IP — which are active] |
| Captive portal endpoint | [URL or "shared babb.tel infrastructure"] |
| Push relay | [ntfy self-hosted / ntfy.sh / Gotify / none] |
| NTP server | [pool.ntp.org / regional pool / internal] |

Carrier-specific Telux configs (complete one block per carrier):

```
Carrier: [CARRIER_NAME]
MCC/MNC: [XXXYY]
APN name: [apn name]
APN type: [default,mms,supl]
VoLTE: [Yes / No / Planned]
USSD mobile money: [*XXX# — service name]
RCS: [supported / not supported]
```

---

## ZAKO Services [PRE-RELEASE]

| Service | Status | Notes |
|---|---|---|
| PADS record model | [Active / Disabled / Deferred] | |
| Agreements service | [Active / Disabled / Deferred] | |
| Health service | [Active / Disabled / Deferred] | |
| Academy service | [Active / Disabled / Deferred] | |
| BitPads accounting core | [Active / Disabled / Deferred] | |
| Telux identity/exchange | [Active / Disabled / Deferred] | |
| Outstack power governance | [Active / Disabled / Deferred] | |

---

## Security Configuration [PRE-RELEASE]

| Field | Value |
|---|---|
| Boot state | [GREEN / YELLOW / ORANGE] |
| AVB custom key injection | [Supported / Not supported on this bootloader] |
| dm-verity | [Active / Disabled in dev / Active in prod] |
| FBE | [Active — hardware ICE / Active — software / Not active] |
| SELinux | [Enforcing / Permissive during dev] |
| Widevine level | [L1 / L2 / L3] |
| Keymaster version | [4.0 / 3.0 / software] |
| Key storage path | [path outside build tree, never committed] |

---

## Build Key Slots [PRE-RELEASE]

*Do not record key material here — record only the slot names and where keys are stored.*

| Key | Slot name | Storage location |
|---|---|---|
| AVB root key | [distro]-avb-root | [offline hardware / secrets vault / path] |
| AVB system chain key | [distro]-avb-system | |
| Platform signing key | [distro]-platform | |
| Release key | [distro]-release | |

---

## OTA and Distribution [PRE-RELEASE]

| Field | Value |
|---|---|
| OTA server | [URL] |
| Update check interval | [hours] |
| Full OTA naming convention | [ota-[distro]-[device]-VERSION-DATE-full.zip] |
| Delta OTA supported | [Yes / No] |
| GPL source publication | [URL] |
| F-Droid repo (if maintained) | [URL or N/A] |

---

## Known Gaps and Limitations

*Link to the distribution's gaps document and summarize critical open items.*

Gaps document: [path to gaps-and-blind-spots.md in distribution folder]

| ID | Description | Severity | Status |
|---|---|---|---|
| GAP-001 | [description] | Critical / High / Medium / Low | Open / In progress / Resolved |

---

## Notes

*Anything non-obvious about this distribution that is not captured in the structured fields above.*
