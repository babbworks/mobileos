# Briefing 04 — ZAKO Integration on Ya

How the ZAKO service layer (Outstack, Telux, PADS, SIMBA) is integrated into the Ya distribution on the Cat S22 Flip.

---

## What "ZAKO Integration" Means

ZAKO is not a separate OS layer added after the fact. It is the reason Ya exists. The AOSP base provides the hardware abstraction, telephony stack, and application framework. ZAKO provides the exchange layer, the power doctrine, the identity model, and the record format. Together they are Ya.

In practice, ZAKO integration on Ya means:

1. **Four package repos wired into the AOSP manifest** — Outstack, Telux, PADS, SIMBA
2. **Device tree config files** in `device/cat/S22FLIP/zako/` that set distribution-specific values for each package
3. **SELinux policy additions** in `device/cat/S22FLIP/sepolicy/` for any device-specific process entries
4. **system.prop entries** for captive portal, NTP, DNS, and eDRX radio config

---

## The Four Packages

### Outstack (`zako-outstack`)

Power governance daemon. Enforces the five-mode state machine, parks cores, controls CPU governor, writes to the power ledger, and gates background processes.

Ya's Outstack configuration is the reference implementation of ZAKO power doctrine on a 1450mAh battery. Every threshold and governor choice in `outstack.conf` is tuned for the 1450mAh constraint and the Zambia deployment context (high heat, intermittent charging access).

Config docs: `CatFlip/zako/outstack/`
Package ref: `ZAKO/PACKAGES/zako-outstack.md`

### Telux (`zako-telux`)

The exchange stack. Three daemons — `telux-ledgerd` (ledger), `telux-identd` (identity), `telux-sharedb` (sharing) — provide the sovereign exchange infrastructure.

Ya's Telux configuration wires up three Zambian carriers (Airtel ZM, MTN ZM, Zamtel) as transport layers, configures the captive portal endpoint at `204.babb.tel`, sets NTP to `africa.pool.ntp.org`, and configures the ntfy push relay at `babb.tel`.

Config docs: `CatFlip/zako/telux/`
Package ref: `ZAKO/PACKAGES/zako-telux.md`

### PADS (`zako-pads`)

Work Record Service. Manages Work Islands and the work record lifecycle (ASSIGN → FINISH → INVOICE → PAY). Primary use case for Ya is field worker exchange records — daily work assignment and payment for informal sector workers in Zambia.

Ya's PADS configuration sets `base_currency = ZMW` (Zambian Kwacha) and enables pads-v1 inbound decoding for compatibility with Workpads clients.

Config docs: `CatFlip/zako/pads/`
Package ref: `ZAKO/PACKAGES/zako-pads.md`

### SIMBA (`zako-simba`)

Service integration framework. Governs any external service admitted to the Ya distribution. At Ya v1 launch, no external SIMBA services are pre-admitted — the registry ships empty. Distribution approval workflow is documented in `ZAKO/PROTOCOLS/SIMBA-Standard-v1.md`.

Package ref: `ZAKO/PACKAGES/zako-simba.md`

---

## Local Manifest Wiring

In the local AOSP manifest (`.repo/local_manifests/babb-zako.xml`):

```xml
<manifest>
  <remote name="github" fetch="https://github.com/" review="https://github.com/" />

  <!-- ZAKO service packages -->
  <project name="babb-os/zako-outstack"
           path="packages/services/ZakoOutstack"
           remote="github"
           revision="main" />

  <project name="babb-os/zako-telux"
           path="packages/services/ZakoTelux"
           remote="github"
           revision="main" />

  <project name="babb-os/zako-pads"
           path="packages/services/ZakoPADS"
           remote="github"
           revision="main" />

  <project name="babb-os/zako-simba"
           path="packages/services/ZakoSIMBA"
           remote="github"
           revision="main" />

  <!-- Ya device tree (CatFlip) -->
  <project name="babb-os/android_device_cat_S22FLIP"
           path="device/cat/S22FLIP"
           remote="github"
           revision="main" />

  <!-- Kernel -->
  <project name="babb-os/android_kernel_cat_s22flip"
           path="kernel/cat/s22flip"
           remote="github"
           revision="babb-4.9-android11" />
</manifest>
```

---

## Device Tree Structure for ZAKO Config

```
device/cat/S22FLIP/
├── device.mk                    ← PRODUCT_PACKAGES += ZakoOutstack ZakoTelux ZakoPADS ZakoSIMBA
├── system.prop                  ← captive portal, NTP, DNS, eDRX props
├── zako/
│   ├── outstack/
│   │   └── outstack.conf        ← Ya-specific power mode thresholds and governors
│   │   └── outstack-policy.xml  ← Process class assignments for Ya daemons
│   ├── telux/
│   │   └── telux.conf           ← Identity, ledger, carrier, sharedb config
│   │   └── carriers/
│   │       ├── airtel-zm.conf
│   │       ├── mtn-zm.conf
│   │       └── zamtel.conf
│   ├── pads/
│   │   └── pads.conf            ← ZMW currency, pads-v1 settings
│   └── simba/
│       └── simba.conf           ← Distribution DID, admission policy
│       └── registry/            ← Empty at v1 launch; add manifest files as services are admitted
```

---

## SELinux Integration

Each ZAKO package provides its own base SELinux policy (`outstack.te`, `telux.te`, etc.). Ya adds device-specific rules in:

```
device/cat/S22FLIP/sepolicy/
├── outstack_device.te   ← Any Ya-specific process class additions
├── telux_device.te      ← Ya-specific carrier or daemon entries
└── file_contexts        ← Updated with /data/zako/* paths
```

The Journal sub-entity SELinux protection (no SIMBA Node may access sub_entity=3) is in the base `telux.te` and cannot be overridden by device tree policy.

---

## system.prop Entries

```
# Captive portal — redirect from Google to babb.tel
captive_portal_server=204.babb.tel
captive_portal_http_url=http://204.babb.tel/generate_204
captive_portal_https_url=https://204.babb.tel/generate_204
captive_portal_fallback_url=https://204.babb.tel/generate_204
captive_portal_other_fallback_urls=https://204.babb.tel/generate_204

# NTP — Africa pool, not Google
persist.sys.ntp_server=africa.pool.ntp.org

# DNS — Quad9
net.dns1=9.9.9.9
net.dns2=149.112.112.112

# eDRX (Outstack also manages these at mode entry; these are boot defaults)
ro.ril.edrx.enable=1
ro.ril.edrx.ptw=2048
ro.ril.edrx.cycle=8192

# Clear Google crash receiver
ro.error.receiver.default=
```

---

## Integration Verification

After first boot, confirm ZAKO services are running:

```bash
# Confirm all ZAKO daemons are up:
adb shell ps | grep -E "outstack|telux|pads|simba"

# Confirm Outstack reports correct mode:
adb shell outstack-ctl status

# Confirm Telux identity is provisioned:
adb shell telux-ctl identity

# Confirm captive portal is not pointing to Google:
adb shell settings get global captive_portal_server
# Expected: 204.babb.tel

# Confirm no GMS packages:
adb shell pm list packages | grep google
# Expected: no output
```

---

## Known Integration Gaps

See `CatFlip/project/gaps-and-blind-spots.md` for the full list. ZAKO-relevant gaps:

- **GAP-001**: STK/USSD not verified — Telux transport layer for mobile money (Airtel Money `*778#`, MTN MoMo `*303#`) depends on USSD working correctly
- **GAP-003**: VoLTE/IMS stack unclear — affects whether `communication` class processes can rely on IP voice during Conservation and Critical modes
