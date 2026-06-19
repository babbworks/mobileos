# Ya — Preloaded Apps

Apps included in the Ya system image or documented as first-install recommendations for Zambian users. Divided into bundled (in system image) and guided-install (user installs from F-Droid after first boot).

---

## Philosophy

Ya ships the minimum set of apps needed for the core use case: a functional de-Googled Android for a Zambian field worker with Telux exchange capability. The catalog is intentionally small. Extra apps increase image size (tight on 16GB eMMC), increase attack surface, and reduce the chance that a user gets lost in a cluttered launcher.

Every bundled app must be available via F-Droid for updates. Apps only available on Google Play are excluded by policy.

---

## Bundled (System Image)

These are in `PRODUCT_PACKAGES` in `device.mk` and ship as part of the Ya image.

| App | Package | Role | Privileged |
|---|---|---|---|
| F-Droid | `org.fdroid.fdroid` | App store for all future installs | Yes — priv-app so it installs without "unknown sources" prompt |
| QKSMS | `org.qksms` | SMS/MMS client | No |
| AOSP Phone | `com.android.phone` | Dialer | Yes — system |
| AOSP Contacts | `com.android.contacts` | Contacts | Yes — system |
| Mull | `us.spotco.mulch` | Browser (Firefox-based, hardened) | No |
| ntfy | `io.ntfy` | UnifiedPush distributor / push notifications | No |
| Déjà Vu Location | `org.fitchfamily.android.dejavu` | Network location provider (no Google) | Yes — location provider |

**Note on Mull packaging:** Mull is available on F-Droid main repo. Bundle the APK from F-Droid's signed release rather than a GitHub release, so update verification chain is consistent.

**Note on Déjà Vu:** Must be installed as a privileged app under `/system/priv-app/` for the location provider permission. Include the `privapp-permissions-dejavu.xml` in `system/etc/permissions/`.

---

## Guided Install (First Boot / Setup Wizard)

These are not in the system image but are presented to the user as recommended installs after first boot. The Ya setup wizard (if implemented) can offer one-tap install from F-Droid.

| App | F-Droid | Use |
|---|---|---|
| K-9 Mail | ✓ | Email (IMAP/SMTP) |
| OsmAnd~ | ✓ | Offline maps — prompt to download Zambia map pack over WiFi |
| DAVx5 | ✓ | Calendar + contacts sync to Nextcloud/CalDAV |
| Simple Calendar | ✓ | Calendar UI |
| Material Files | ✓ | File manager |
| VLC | ✓ | Media player |
| Open Camera | ✓ | Camera app (replaces any stock camera if needed) |
| KeePassDX | ✓ | Password manager (local vault; no cloud account) |
| Aegis Authenticator | ✓ | TOTP 2FA |
| Breezy Weather | ✓ | Weather (uses open-source weather APIs) |
| NewPipe | GitHub releases | YouTube (not on F-Droid main due to API scraping; document install source) |

---

## Removed from Stock Firmware

Apps present in the Cat S22 Flip stock firmware that are explicitly excluded from Ya:

| Removed | Reason |
|---|---|
| Google Play Store (`com.android.vending`) | GMS; replaced by F-Droid |
| Google Play Services (`com.google.android.gms`) | GMS; no replacement needed for core function |
| Google Chrome (`com.android.chrome`) | Replaced by Mull |
| Google Maps | Replaced by OsmAnd~ |
| Gmail | Replaced by K-9 Mail |
| Google Messages | Replaced by QKSMS |
| Google Assistant | Disabled entirely (`config_defaultAssistant = ""`) |
| YouTube | Replaced by NewPipe |
| CatPhone / CatConnect | Cat-branded apps from Bullitt |
| Any carrier preloads | Airtel or MTN branded apps from stock firmware |
| Qualcomm diagnostic apps (`qlogd`, `perfdump`, etc.) | Telemetry; remove |

---

## Image Size Budget

16GB eMMC, roughly:
- AOSP system image: ~2.5 GB
- Vendor partition: ~1.0 GB
- Product partition: ~200 MB
- Bundled apps: ~150 MB (F-Droid ~30MB, Mull ~70MB, others ~50MB)
- User data partition: remaining (~12 GB)

Keep the user data partition at least 10 GB. Shipping too many large bundled apps shrinks this. Mull is the heaviest app at ~70 MB; everything else is small.

---

## build Integration

```makefile
# In device.mk:
PRODUCT_PACKAGES += \
    FDroid \
    QKSMS \
    Mull \
    Ntfy \
    DejaVuLocationService

PRODUCT_COPY_FILES += \
    device/cat/S22FLIP/permissions/privapp-permissions-fdroid.xml:\
    system/etc/permissions/privapp-permissions-fdroid.xml \
    device/cat/S22FLIP/permissions/privapp-permissions-dejavu.xml:\
    system/etc/permissions/privapp-permissions-dejavu.xml
```

The F-Droid privileged extension permissions file:

```xml
<!-- privapp-permissions-fdroid.xml -->
<permissions>
  <privapp-permissions package="org.fdroid.fdroid">
    <permission name="android.permission.INSTALL_PACKAGES"/>
    <permission name="android.permission.DELETE_PACKAGES"/>
  </privapp-permissions>
</permissions>
```
