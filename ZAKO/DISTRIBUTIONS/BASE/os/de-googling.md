# De-Googling — ZAKO Distribution Base

Removing Google Mobile Services (GMS) and replacing with privacy-respecting alternatives. The goal is a fully functional Android device with no data transmitted to Google. This document covers the universal GMS removal process common to all ZAKO distributions.

Device-specific notes (APNs, particular replacement apps, market context) belong in the distribution's own `os/de-googling.md`.

---

## What GMS Is

GMS (Google Mobile Services) is a set of proprietary apps and background services Google requires device manufacturers to ship. On stock Android, GMS provides:

- Google Play Store and Play Services (`com.google.android.gms`)
- Push notifications (Firebase Cloud Messaging — FCM)
- Location services (Google network location provider)
- Account sync (Google account framework)
- SafetyNet / Play Integrity attestation
- Crash reporting (Google Analytics)
- Maps, Search, Assistant, Chrome, Gmail, YouTube

On a clean AOSP build, GMS is **not automatically included** — it must be actively added. The primary task is ensuring GMS does not get pulled in, not removing it from an existing build.

---

## GMS Removal Strategy

```makefile
# Confirm these are NOT in device.mk or any inherited makefile:
# PRODUCT_GMS_CLIENTID_BASE := android-[device]  ← Remove this line if present
# gapps, GmsCore, PlayStore — do not add these to PRODUCT_PACKAGES

# Safe to keep (these are AOSP, not GMS):
# com.android.providers.telephony
# com.android.phone
# com.android.settings
```

If the device tree was derived from a LineageOS or stock OEM tree, audit the product makefiles for any Google client ID line:
```bash
grep -r "GMS_CLIENTID" device/[DEVICE_VENDOR]/[DEVICE_CODENAME]/
```
Remove any match.

---

## Standard Replacements

### App Store: F-Droid (Privileged)

Install F-Droid as a privileged system app so it can install apps without requiring "install unknown sources":

```makefile
# In device.mk:
PRODUCT_PACKAGES += \
    FDroid

PRODUCT_COPY_FILES += \
    device/[DEVICE_VENDOR]/[DEVICE_CODENAME]/permissions/privapp-permissions-fdroid.xml:\
    system/etc/permissions/privapp-permissions-fdroid.xml
```

F-Droid privileged extension allows seamless installation without manual permission grants per app.

### Push Notifications: UnifiedPush via ntfy

FCM (Firebase Cloud Messaging) requires GMS. Replace with UnifiedPush:

- **Distributor:** ntfy (self-hosted) or ntfy.sh
- **Compatible apps:** Tusky (Mastodon), Element (Matrix), Conversations (XMPP), and a growing list

For distributions targeting offline-tolerant users: polling-only is acceptable and simpler. Document the trade-off in the distribution's briefing.

```bash
# Self-hosted ntfy server configuration:
# Users configure ntfy as their UnifiedPush distributor in app settings
# ntfy app available on F-Droid
```

### Network Location: Déjà Vu

Replaces Google's network location provider (WiFi SSID + tower triangulation via Google's database):

```makefile
PRODUCT_PACKAGES += \
    DejaVuLocationService
```

Déjà Vu uses locally-cached Mozilla Location Service data — no data sent to Google. Alternative: GPS-only location (slower TTFF, no network data sharing).

### Browser: Bromite or Mull

```makefile
# Bromite (Chromium-based, privacy patches):
PRODUCT_PACKAGES += Bromite

# Or Mull (Firefox-based, available on F-Droid):
# Direct users to F-Droid for install
```

### Maps: OsmAnd~

OpenStreetMap-based, fully offline capable. Pre-download regional map data over WiFi before deployment:

```makefile
PRODUCT_PACKAGES += OsmAnd
# Users download regional map packs in-app
```

### Email: K-9 Mail / Thunderbird Mobile

Mature FOSS email client supporting IMAP/SMTP. Available on F-Droid.

### SMS/MMS: QKSMS

Full-featured FOSS SMS client. Available on F-Droid. AOSP Messaging also works for basic SMS.

### Dialer and Contacts

AOSP includes its own Dialer and Contacts apps that work fully without GMS. LineageOS-enhanced versions add call recording (where legal) and T9 dialing.

### Calendar: Simple Calendar

Available on F-Droid. Pair with DAVx5 for CalDAV sync to self-hosted Nextcloud or any CalDAV server.

### Assistant

Remove entirely — no replacement required:

```xml
<!-- In overlay/res/values/config.xml: -->
<string name="config_defaultAssistant" translatable="false"></string>
```

Remap or disable any hardware button that triggered the assistant.

---

## MicroG — Decision Point

MicroG is an open-source reimplementation of GMS APIs. It enables GMS-dependent apps to function without real GMS.

**Default ZAKO position:** Do not ship MicroG. Target use cases do not require Google account integration, and MicroG adds maintenance complexity.

If a distribution's market requires MicroG (specific apps with no FOSS alternatives):
- Document this decision in the distribution profile under Notes
- Offer MicroG as a separately installable ZIP, not as a default preload

---

## System Property Replacements

### Captive Portal Check

Android checks connectivity by pinging Google servers. Replace with a distribution-owned endpoint:

```
# In system.prop:
captive_portal_server=[CAPTIVE_PORTAL_HOST]
captive_portal_http_url=http://[CAPTIVE_PORTAL_HOST]/generate_204
captive_portal_https_url=https://[CAPTIVE_PORTAL_HOST]/generate_204
captive_portal_fallback_url=https://[CAPTIVE_PORTAL_HOST]/generate_204
captive_portal_other_fallback_urls=https://[CAPTIVE_PORTAL_HOST]/generate_204
```

Server requirement: a host that returns HTTP 204 on `/generate_204`. Lightweight nginx config:
```nginx
location /generate_204 { return 204; }
```

### NTP (Time Sync)

```
# In system.prop:
persist.sys.ntp_server=[REGIONAL_NTP_POOL]
# e.g., africa.pool.ntp.org, pool.ntp.org, asia.pool.ntp.org
```

### DNS

```
# In system.prop:
net.dns1=9.9.9.9          # Quad9 (DNSSEC validating)
net.dns2=149.112.112.112  # Quad9 secondary
```

Better: enable Private DNS (DNS-over-TLS) in system settings defaults.

### Crash Reporting

```
# In system.prop — clear Google crash receiver:
ro.error.receiver.default=
```

---

## Telemetry Removal Checklist

```
[ ] PRODUCT_GMS_CLIENTID_BASE removed from product makefile
[ ] No GMS packages in PRODUCT_PACKAGES (grep -r "GmsCore\|PlayStore\|gapps" device/)
[ ] captive_portal_server redirected to distribution-owned endpoint
[ ] NTP server changed from Google to regional pool
[ ] DNS changed from 8.8.8.8 to Quad9 or equivalent
[ ] ro.error.receiver.default cleared
[ ] Google location provider not installed
[ ] No diagnostic/analytics APKs in system or vendor partitions

Post-boot verification:
[ ] adb shell pm list packages | grep google  → should return nothing
[ ] Network audit: adb shell tcpdump -i any host google.com  → no traffic on fresh boot
[ ] adb shell settings get global captive_portal_server  → returns your endpoint
```

---

## Standard F-Droid App Set

Recommended for all ZAKO distributions. Adapt per market — swap Maps app, SMS app, etc. as appropriate.

| Category | App | F-Droid |
|---|---|---|
| App store | F-Droid | Built-in |
| Browser | Bromite | ✓ |
| Email | K-9 Mail | ✓ |
| Maps | OsmAnd~ | ✓ |
| SMS | QKSMS | ✓ |
| Push relay | ntfy | ✓ |
| Calendar | Simple Calendar | ✓ |
| Contacts sync | DAVx5 | ✓ |
| Files | Material Files | ✓ |
| PDF | MuPDF | ✓ |
| Video | VLC | ✓ |
| Camera | Open Camera | ✓ |
| Network location | Déjà Vu | ✓ |
| 2FA | Aegis Authenticator | ✓ |
| Password manager | Bitwarden | ✓ |
