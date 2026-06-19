# Ya — F-Droid Setup

F-Droid configuration for Ya: privileged install, default repositories, and the first-boot app delivery workflow.

---

## Why F-Droid as Privileged System App

Installing F-Droid as a privileged system app (under `/system/priv-app/`) means it holds the `INSTALL_PACKAGES` and `DELETE_PACKAGES` permissions at the system level. The user doesn't need to manually enable "Install unknown apps" per app — F-Droid can install apps seamlessly.

Without privileged status, every F-Droid app install requires the user to navigate to Settings → Security → Install unknown apps. This is a significant friction point for less technical users in Zambia.

---

## Build Integration

```makefile
# In device.mk:
PRODUCT_PACKAGES += FDroid

PRODUCT_COPY_FILES += \
    device/cat/S22FLIP/permissions/privapp-permissions-fdroid.xml:\
    system/etc/permissions/privapp-permissions-fdroid.xml
```

```xml
<!-- device/cat/S22FLIP/permissions/privapp-permissions-fdroid.xml -->
<permissions>
  <privapp-permissions package="org.fdroid.fdroid">
    <permission name="android.permission.INSTALL_PACKAGES"/>
    <permission name="android.permission.DELETE_PACKAGES"/>
    <permission name="android.permission.ACCESS_NETWORK_STATE"/>
  </privapp-permissions>
</permissions>
```

The F-Droid APK must be placed in the build as a prebuilt. Use the F-Droid reproducible build APK from `f-droid.org/F-Droid.apk` — verify the SHA-256 against the F-Droid published checksum before bundling.

---

## Default Repository Configuration

F-Droid ships with the main F-Droid repository pre-configured. Ya-specific additions:

```xml
<!-- F-Droid repository list (embedded in app config or added at first run) -->

<!-- F-Droid main (default; already included in F-Droid APK) -->
<!-- https://f-droid.org/repo -->

<!-- IzzyOnDroid — larger selection, maintained by a single trusted curator -->
<!-- https://apt.izzysoft.de/fdroid/repo -->
<!-- Fingerprint: 3BF0D6ABFEAE2F401707B6D966BE743BF0EEE49C -->
```

The IzzyOnDroid repo contains apps not in the main F-Droid repo (including some apps that are FOSS but have build-time dependencies that F-Droid doesn't compile). Pre-configuring it reduces the number of steps a user needs to find common apps.

To add IzzyOnDroid as a default repo, include it in the F-Droid config file bundled with the app:

```xml
<!-- To be placed in the F-Droid priv-app data or included in APK via build-time config -->
<!-- This approach depends on the F-Droid version — verify against current F-Droid source -->
```

**Simpler alternative for v1:** Document the IzzyOnDroid repo addition in the user onboarding guide. Don't over-engineer first-boot for v1.

---

## F-Droid Privileged Extension

F-Droid offers a "privileged extension" as a separate package that provides the INSTALL/DELETE permissions without putting the full F-Droid app in priv-app. This is the recommended approach for distributions that want the cleanest separation:

```
packages/
└── FDroidPrivilegedExtension   ← priv-app; small; only holds the permissions
└── FDroid                      ← regular system app; full UI
```

The privileged extension approach avoids bundling the full F-Droid UI code in `/system/priv-app/`, which minimizes the priv-app attack surface.

Whether to use the extension approach or simply bundle F-Droid itself as a priv-app is a distribution implementation choice. Either works. For Ya v1, bundling F-Droid directly as a priv-app is simpler.

---

## Zambia Connectivity Considerations

F-Droid repository sync requires a data connection. On Zambian metered LTE:

- Default F-Droid behavior: syncs repository index on opening the app or on a background timer
- Repository index size: ~5–10 MB for the main repo
- App downloads: variable (50 MB–200 MB for larger apps like Mull)

Guidance for users:
1. Do repository sync and large app downloads over WiFi
2. Individual small app installs (K-9 Mail, QKSMS, Aegis) can be done over LTE (~2–10 MB each)
3. OsmAnd~ map packs should always be downloaded over WiFi (~100–300 MB for Zambia)

No configuration changes are needed in F-Droid itself to enforce WiFi-only downloads — the user controls this in F-Droid settings. Consider documenting this in the Ya onboarding guide.

---

## Updates

F-Droid handles its own updates. Since it's installed as a system app, F-Droid updates itself via the privileged extension mechanism. The first update after install will require the user to confirm it.

ZAKO system (OS) updates are handled separately via the Ya OTA mechanism, not via F-Droid. F-Droid manages only third-party user-space app updates.

---

## App Signing Verification

F-Droid verifies the signature of every APK it installs. All apps in the main F-Droid repo are signed by F-Droid's own key. When an app update is fetched, F-Droid checks that the new APK is signed by the same key as the installed version.

This is the trust model: F-Droid (not the developer) signs the APKs built from source. Apps that don't match are rejected. This is stronger than Google Play's model (developer-signed) for reproducibility but weaker for apps where the developer's own signature is important (e.g., apps that use developer-signed certificates for specific features).

For Ya, this is acceptable — none of the bundled app set relies on developer-signed certificate features.
