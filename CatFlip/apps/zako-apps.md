# Ya — ZAKO-Specific Apps

Apps built specifically for ZAKO that surface the Telux, PADS, and Outstack capabilities to the user. These are not available on F-Droid — they are ZAKO distribution apps, built and shipped by Babb.

---

## The App Layer

ZAKO's daemon layer (Outstack, Telux, PADS, SIMBA) runs below the application layer. The user interacts with these systems through apps that surface them. On Ya v1, the primary user-facing ZAKO app is Workpads.

All ZAKO apps are SIMBA-compliant nodes: they have declared manifests, declared process classes, and produce TELUX records via `telux-ledgerd`. They do not maintain parallel data stores.

---

## Workpads

**Purpose:** Field worker work record and exchange app. The primary ZAKO application for Ya.

**What it surfaces:**
- Work Island creation and management
- ASSIGN / START / FINISH work record flows
- EXPENSE and TRAVEL logging
- Invoice generation and transmission
- PADS Tasks View (all work items, status derived from ledger)
- PADS Tallies View (period summaries)
- Money View (financial record history)
- Outbound queue status (records awaiting transmission)

**How it works:**
Workpads is a UI layer on top of PADS. It calls the PADS API (exposed via `pads-workd` and `telux-ledgerd`). It does not store any data of its own — all records are in `telux-ledgerd`. The app is display and input only.

**pads-v1 compatibility:**
Workpads also serves as the pads-v1 client for incoming Workpads URL records from non-ZAKO counterparties. When a Workpads URL arrives via SMS, `telux-sharedb` decodes it and routes it to the PADS API; Workpads displays the inbound record.

**Process class:** `user-active` (foreground app). PADS background work runs in `pads-workd` (communication class).

**Repo:** `github.com/babb-os/workpads-android`
**Build target:** Bundled in Ya system image

---

## Telux Exchange App (Planned — v2)

A user-facing app surfacing the Telux exchange model directly: DID display, People Island management (contacts with sovereign DIDs), outbound record history, newgroup management.

In Ya v1, the People Island (contacts) and Exchange sub-entity are accessible through Workpads for work-related exchange. A dedicated Telux app with full sovereign exchange UI is a v2 priority.

---

## Outstack Status Widget / Notification

A lightweight system UI element showing the current Outstack power mode and battery threshold. Not a standalone app — implemented as a persistent notification or status bar extension.

**Shows:**
- Current mode: Full Power / Standard / Conservation / Critical / Emergency
- Battery % and which threshold is next
- How many processes are currently gated

This is important for the Ya user to understand why some apps are suspended — without this indicator, a user in Conservation mode whose F-Droid background sync is suspended will be confused.

**Implementation:** SystemUI extension or persistent foreground notification from `outstack-governed`.
**Status:** Planned for v1 if time permits; v2 otherwise.

---

## SIMBA Registry App (Future — post-v2)

A settings-level app allowing the Sovereign to review admitted SIMBA Nodes, manage capability grants, and review compliance status. Surfaces `simba-ctl` functionality in UI form.

Not needed in Ya v1 (no SIMBA nodes admitted yet). Required when the first external SIMBA service is admitted.

---

## App Build Integration

Workpads ships as a bundled system app:

```makefile
# In device.mk:
PRODUCT_PACKAGES += \
    Workpads
```

Since Workpads is a distribution-built app (not a generic FOSS app), it ships directly as an APK prebuilt in the device tree:

```
device/cat/S22FLIP/apps/
└── Workpads/
    ├── Workpads.apk
    └── Android.mk
```

```makefile
# device/cat/S22FLIP/apps/Workpads/Android.mk:
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := Workpads
LOCAL_MODULE_CLASS := APPS
LOCAL_SRC_FILES := Workpads.apk
LOCAL_CERTIFICATE := platform     # signed with Ya's platform key
LOCAL_MODULE_PATH := $(TARGET_OUT)/app
include $(BUILD_PREBUILT)
```

**Signing:** Workpads is signed with the Ya platform key (`ya-s22flip-platform`). Using the platform key grants Workpads the `SIGNATURE` level permissions needed to communicate with ZAKO daemons.

---

## ZAKO vs Third-Party App Distinction

| | ZAKO apps (Workpads etc.) | Third-party F-Droid apps |
|---|---|---|
| Source | Built by Babb | Open source, third-party |
| Distribution | Bundled in Ya image | Installed via F-Droid |
| Signing | Ya platform key | F-Droid key |
| SIMBA manifest | Implicit (built-in; no external admission needed) | Required if accessing ZAKO Islands |
| Ledger access | Direct via PADS/Telux API | Via capability grant from Sovereign |
| Update mechanism | Ya OTA | F-Droid |
