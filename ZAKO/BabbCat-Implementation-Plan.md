# Babb Cat — Implementation Plan
## A Complete Phased Roadmap for Building HOME's First Distribution

*Process Document — Living Reference*
*Derived from: HOME-Architecture-and-Vision.md, TeluxOS-AOSP-Prototype.md,*
*TeluxOS-Bedrock-Purity-Assessment.md, cats22-os/*, workpadskaios/, workpads-standard/*
*May 31, 2026*

---

## What Babb Cat Is

Babb Cat is a HOME distribution. HOME is the operating system — the architecture, the philosophy, the Outstack power doctrine, the Telux exchange layer, the sovereignty model. Babb Cat is the expression of HOME for one specific context: the CAT S22 Flip mobile phone, deployed first in Zambia, designed for field workers, mobile money participants, and any person or organization that needs a sovereign mobile device with a durable exchange record.

The relationship is exact:

> HOME is to Babb Cat what Linux is to Ubuntu.
> Telux and Outstack are to HOME what systemd and the Linux kernel are to Ubuntu.
> The CAT S22 Flip is to Babb Cat what the x86 PC was to early Ubuntu — the device on which everything is first proven.

Babb Cat is built on AOSP. It inherits from the `cats22-os` research corpus — thirty technical documents, multiple kernel repos, firmware analysis, vendor blob inventories, and toolchain configurations representing the existing foundation. It adds the full HOME architecture: Outstack power management, Telux Islands and sovereignty, the exchange ledger, and W3C DID identity.

When complete, Babb Cat is:
- A phone that makes calls, sends SMS, and accesses mobile money on Zambia's three carriers without any configuration
- A device that runs for multiple days between charges because every background process is governed by a power doctrine
- A sovereign exchange node whose every meaningful interaction between members is recorded in a chain-hashed, hardware-signed ledger
- A Workpads-native device: the PADS record model that field workers already use in the Workpads KaiOS application runs natively in the HOME exchange layer, not as an app on top of Android but as a ledger entry type understood by the OS itself
- The first real-world proof that the HOME architecture works — that Outstack's five-mode power system extends battery life measurably, that Telux's sovereignty model is usable by real people, and that a flip phone running de-Googled Android is not a privacy exercise but a better phone

This document is the ordered plan for building it. Every phase has clear entry conditions, clear exit conditions, specific outputs, and specific validation criteria. The phases build on each other; no phase begins before its predecessor is validated.

---

## Repository and Resource Map

Before the phase plan begins, the complete map of repos and resources that Babb Cat draws from:

### Kernel Repos (in `repos/kernel/`)

| Repo | Role |
|------|------|
| `caf-msm-4.9/` | CAF upstream kernel — tag `LA.UM.10.6.2.r1-01200-89xx.0` is the baseline |
| `redmi-go/` | Primary reference kernel — Redmi Go (QM215, Android 11 Go, same SoC) |
| `lineageos-xiaomi/` | Secondary reference — Xiaomi MSM8937 LineageOS tree |
| `mi-msm8937/` | Community-maintained MSM8937 kernel with patches |
| `android-linux-stable/` | Security backports merged against MSM-4.9 |
| `msm89x7-mainline/` | Mainline kernel port for MSM8937 — v7.0.9 at time of writing — long-term tracking |

**Working kernel:** `redmi-go/` as the primary build base, patched against `android-linux-stable/` for security, with device-specific patches for S22 Flip hardware (AW881xx amplifier, cover display, lid sensor).

### Device Tree Repos (in `repos/device-trees/`)

| Repo | Role |
|------|------|
| `cat-s22flip-twrp/` | Auto-generated TWRP device tree — has the 64MB recovery error, corrected to 32MB |
| `lineageos-motorola-msm8937-common/` | Motorola MSM8937 common tree — reference for power configs |
| `lineageos-xiaomi-msm8937-common/` | Xiaomi MSM8937 common — closest hardware reference |
| `qcom-reference-msm8937-32go/` | Qualcomm reference design for 32-bit Go — defconfig reference |

### Firmware (in `repos/firmware-dumps/cat-s22flip/`)

| Content | Status |
|---------|--------|
| `aosp-device-tree/` | Device tree extracted from stock firmware dump |
| Vendor blob inventory | ~2,900 files catalogued; 56 firmware binaries, 27 HAL implementations |
| Partition map | Confirmed: recovery=32MB, super=8.5GB (dynamic: system+vendor+product) |
| DTB | Stock device tree binary — decompilation pending for cover display and lid sensor nodes |

### Tool Repos (in `repos/tools/`)

| Repo | Role |
|------|------|
| `docker-aosp/` | Docker-based AOSP build environment — reproducible builds on any host |
| `edl/` | Emergency Download (EDL) tool — recovery from hard brick |
| `android-unpackbootimg/` | Boot image unpacking for analysis and patching |
| `alsa-ucm-conf/` | ALSA Use Case Manager configs — audio routing for custom kernel |

### Bootloader (in `repos/bootloaders/`)

| Repo | Role |
|------|------|
| `lk2nd-ahisky/` | lk2nd — secondary bootloader that runs on top of OEM LK; potential path for custom boot chain improvements |

### Workpads Repos (in `babb/repos/`)

| Repo | Role |
|------|------|
| `workpadskaios/` | Complete KaiOS Workpads app — PADS model, chain protocol, bitpad-v1 codec, exchange record types |
| `workpads-basicsconform/REFS/workpads-standard/` | Normative spec — codec.md, record-schema.md, chain-protocol.md, financial-block.md |

The Workpads system is the HOME exchange ledger's immediate precedent. The PADS record model (Process / Actions / Details / Story), the chain protocol (chainRef linking, state_commit, ACK, dispute, amendment), and the bitpad-v1 codec (binary TLV, deflate-compressed, base64url) constitute the accounting notation that HOME's architecture has flagged as unresolved. Babb Cat resolves it by adopting and extending the Workpads model as the native HOME record format.

---

## Phase 0: Orientation and Repository Integrity
### *Before the build begins, the foundation must be verified*

**Entry condition:** This document exists and the build team has read it alongside `HOME-Architecture-and-Vision.md` and `briefings/05-full-brief.md`.

**Duration:** 1–2 weeks

### 0.1 — Confirm All Repos Are Present and Current

Walk every repo in the resource map above. For each:
- Confirm the directory exists and is a valid git repository
- Confirm the working tree is clean (no uncommitted modifications)
- Confirm the correct branch or tag is checked out
- Note any repos that are stale (more than 90 days behind their upstream)

For kernel repos specifically: confirm `redmi-go/` is based on CAF tag `LA.UM.10.6.2.r1-01200-89xx.0` or a derivative of it. If not, re-establish from `caf-msm-4.9/` at that tag.

For `android-linux-stable/msm-4.9`: confirm the latest security patch merges have been applied. This repo tracks upstream Linux LTS patches merged onto the CAF MSM-4.9 base — it is the primary source of CVE fixes for the kernel.

### 0.2 — Establish the Working Device

Confirm physical access to at least one CAT S22 Flip with:
- Bootloader unlockable (OEM unlock enabled in developer options)
- USB debugging enabled and ADB connection confirmed from build host
- Device at known firmware state (stock Android 11 Go, factory or latest OTA)
- `fastboot devices` shows device serial

Document the device's exact firmware version (`adb shell getprop ro.build.fingerprint`) as the baseline from which all build comparisons will be made.

### 0.3 — Establish the Build Host

The build host must satisfy AOSP Android 11 build requirements:
- Ubuntu 20.04 LTS (bare metal or Docker via `docker-aosp/`)
- Minimum 16GB RAM (32GB recommended)
- Minimum 400GB disk (AOSP source + build outputs)
- Python 3, OpenJDK 11, `repo` tool, standard AOSP build dependencies

Docker path (`docker-aosp/`): the preferred approach for macOS hosts or shared environments. Produces identical build outputs regardless of host OS. Confirm Docker build container launches and can execute `lunch`.

### 0.4 — Read All Gap Documentation

`gaps-and-blind-spots.md` identifies every known risk and undocumented area. Read it completely. Every Critical gap (GAP-001 through GAP-015) must be acknowledged by the team with an assigned owner before Phase 1 begins. These gaps include:

- **GAP-001:** STK and USSD not tested — blocks mobile money
- **GAP-002:** VoLTE / IMS not documented — medium-term voice reliability
- **GAP-003:** Cover display DTS not extracted — flip UX incomplete
- **GAP-004:** Power baseline numbers not established
- **GAP-005:** MCFG carrier profiles not verified for Zambia carriers

Phase assignment for each gap is part of Phase 0's output.

### 0.5 — Workpads Codec Audit

Read `workpadskaios/README.md` and the normative `workpads-standard/` spec docs completely. Specific items to understand before proceeding:

- **PADS model** (Process / Actions / Details / Story): the four-section structure for field service records
- **bitpad-v1 frame structure**: `[template_byte][flag_byte_1][flag_byte_2][field_blobs...]` — compressed, base64url-encoded, sharable as URL hash
- **Chain protocol**: records share a `chainRef`; `state_commit` closes a chain; `ACK` confirms receipt; `dispute` and `amendment` are correction primitives
- **Record types**: `invoice`, `quote`, `receipt`, `job`, `expense`, `payment`, `state_commit`, `amendment`, `dispute`, `ack`
- **Stone/DID identity**: `generateMarkerUid()` exists but has no UI — this is where HOME's identity layer connects

The bitpad-v1 codec is the HOME accounting notation. Babb Cat will extend it, not replace it. Understanding it thoroughly before any other work begins is mandatory.

**Phase 0 Outputs:**
- Repo integrity report (all repos confirmed or remediated)
- Device baseline documented (firmware fingerprint, partition table, ADB confirmed)
- Build host confirmed operational (`lunch` succeeds in AOSP build environment)
- Gap ownership matrix (each gap in `gaps-and-blind-spots.md` has an owner and a target phase)
- Workpads codec audit complete (team has read all normative spec documents)

**Phase 0 Exit Condition:** All outputs complete; no Critical gap is unowned.

---

## Phase 1: Device Intelligence — Know Every Millimeter
### *Nothing is assumed. Everything is measured.*

**Entry condition:** Phase 0 complete. Physical device available, ADB connected.

**Duration:** 2–3 weeks

The `hardware/device-profile.md`, `hardware/bootchain.md`, and `hardware/telephony.md` documents represent existing research. Phase 1 verifies, extends, and formalizes that research into ground truth that every subsequent phase depends on.

### 1.1 — Partition Table Verification

The existing research establishes:
- Recovery partition: **32MB** (not 64MB as auto-generated TWRP tree incorrectly stated)
- Super partition: **8.5GB** (dynamic, contains system + vendor + product as logical volumes)
- Boot partition: **32MB**
- DTBO partition: **8MB**
- Userdata: remainder

Verify against the actual device using `fastboot getvar all` and by reading the partition table via EDL or ADB:

```bash
adb shell cat /proc/partitions
adb shell ls -la /dev/block/bootdevice/by-name/
```

Cross-reference with the firmware dump in `repos/firmware-dumps/cat-s22flip/`. Every partition size and name must match. Discrepancies are investigated before proceeding.

Produce `hardware/partition-map.md` with verified sizes, filesystem types, and confirmed flash commands for each partition.

### 1.2 — Boot Chain Mapping

The boot chain is documented in `hardware/bootchain.md`. Phase 1 verifies this empirically:

**PBL (Primary Boot Loader):** Cannot be observed directly — it executes before any external interface is available. Its existence is confirmed by the fact that fastboot mode is accessible.

**SBL1:** Confirmed via fastboot partition listing (`fastboot getvar all` shows `sbl1` partition).

**TrustZone (QSEE):** Confirmed via `adb shell getprop ro.boot.secureboot` and Keymaster attestation test.

**RPM firmware:** Confirmed by checking `/vendor/firmware/` for `rpm.mbn` in the stock image.

**aboot (LK):** Confirmed version string via `fastboot getvar version-bootloader`. Document the exact version — this determines what fastboot commands are available and whether any known exploits or capabilities apply.

**Orange state confirmation:** After bootloader unlock, document the exact warning screen text and countdown duration. This is the security disclosure for any deployment — users of Babb Cat must understand what orange state means.

### 1.3 — Vendor Blob Deep Inventory

The existing inventory (`build/vendor-blobs.md`) catalogues approximately 2,900 files. Phase 1 adds a criticality classification to each blob category:

**BOOT-CRITICAL:** Blobs without which the device does not boot past `init`. These are non-negotiable. If no open-source alternative exists, they are carried without comment.

**FUNCTION-CRITICAL:** Blobs required for specific hardware functions (GPU, modem, camera, audio). These are carried for the initial builds and tracked for future open-source replacement.

**ENHANCEMENT-ONLY:** Blobs that improve functionality but have acceptable fallback behavior without them. These are candidates for removal in security-focused builds.

**UNKNOWN:** Blobs whose function has not been established. These are investigated before any release build.

Special attention to:
- `aw881xx_*.bin` — Awinic smart amplifier firmware. Boot-critical for speaker function. Must be identified as function-critical and included in build.
- `WCNSS_qcom_cfg.ini` — WiFi configuration. Must match the `wlan.ko` module version exactly.
- MCFG carrier profiles in `/vendor/firmware/mcfg/` — verify presence of MCC 645 entries for Zambia carriers.

### 1.4 — DTB Decompilation (Cover Display and Lid Sensor)

This is the highest-priority hardware intelligence task not yet completed. The stock device tree binary (DTB) contains the exact configuration for the cover display and lid sensor — hardware that works on the stock device but whose configuration is unknown.

```bash
# Extract DTB from stock boot image
./repos/tools/android-unpackbootimg/unpackbootimg -i stock_boot.img -o dtb_extracted/
# Decompile to DTS
dtc -I dtb -O dts dtb_extracted/stock_dtb.dtb -o stock_dtb.dts
# Find cover display node
grep -n "spi\|secondary\|cover\|panel\|st77\|ili9\|gc9" stock_dtb.dts
# Find lid sensor
grep -n "hall\|lid\|SW_LID\|gpio.*key\|input.*gpio" stock_dtb.dts
```

Expected findings:
- **Cover display:** An SPI-attached panel (ST7789, ST7735, or GC9307 controller most likely based on 1.44" size and resolution). Document: SPI bus number, chip select GPIO, reset GPIO, data/command GPIO, controller model.
- **Lid sensor:** A GPIO input or Hall effect sensor input. Document: GPIO number, active-low/high polarity, debounce configuration.

These findings go into `hardware/cover-display-dts.md` and `hardware/lid-sensor-dts.md`. They are required by Phase 5 (Hardware Bring-Up) and Phase 7 (Outstack Integration).

### 1.5 — Telephony Stack Verification

The existing `hardware/telephony.md` documents the QMI protocol stack and the vendor RIL. Phase 1 extends this with live Zambia SIM testing — the first empirical data on actual carrier behavior.

Testing with a Zambia SIM (Airtel preferred, as the largest carrier):
- Insert SIM, boot stock firmware
- Confirm automatic APN configuration from MCFG profile
- Confirm voice call (outgoing and incoming)
- Confirm SMS
- Confirm USSD: `*778#` (Airtel Money menu must appear and respond correctly)
- Confirm STK: the SIM Toolkit menu for Airtel Money must appear in the phone's menu system automatically on SIM insertion

Document every finding, including any failure. Failures at this stage on stock firmware indicate carrier or SIM issues, not OS issues — but they must be understood before building a custom OS that depends on the same hardware.

**Phase 1 Outputs:**
- `hardware/partition-map.md` — verified partition table
- `hardware/cover-display-dts.md` — controller model and GPIO map from DTB decompilation
- `hardware/lid-sensor-dts.md` — GPIO configuration from DTB decompilation
- `hardware/vendor-blob-criticality.md` — full blob inventory with criticality classification
- `hardware/zambia-carrier-baseline.md` — live SIM test results on stock firmware
- Updated `hardware/bootchain.md` with empirical verification notes

**Phase 1 Exit Condition:** Cover display DTS extracted. Lid sensor GPIO confirmed. Vendor blob criticality classification complete. Zambia carrier baseline documented including USSD and STK results.

---

## Phase 2: Toolchain and Build Environment
### *A build you cannot reproduce is not a build you control*

**Entry condition:** Phase 1 complete. Build host confirmed.

**Duration:** 1–2 weeks

### 2.1 — AOSP Source Checkout

AOSP Android 11 (android-11.0.0_r46 or the latest android-11.0.0 security patch release) is the base.

```bash
mkdir -p ~/babbcat && cd ~/babbcat
repo init -u https://android.googlesource.com/platform/manifest \
  -b android-11.0.0_r46 --depth=1
repo sync -c -j$(nproc) --no-tags
```

The `--depth=1` flag reduces initial checkout size. Do not use shallow checkouts for the kernel or device-specific repos that require full history for patch archaeology.

Establish a Babb Cat manifest overlay — a local manifest that points the `device/babb/cats22flip` and `kernel/babb/msm8937` paths to Babb's own repos rather than AOSP defaults. This is how HOME distributions track their own device trees independently of AOSP's device tree ecosystem.

### 2.2 — Kernel Source Establishment

The working kernel is assembled from three sources:

**Base:** CAF tag `LA.UM.10.6.2.r1-01200-89xx.0` from `caf-msm-4.9/` — this is the exact Android 11 Go / MSM8937 kernel.

**Reference patches from Redmi Go (`redmi-go/`):** The Redmi Go community kernel includes numerous MSM8937-specific fixes. Cherry-pick (do not merge blindly) the patches relevant to:
- Power management (RPM communication, power collapse, eDRX configuration)
- Audio (WCD9335 codec, AW881xx amplifier driver, LPASS offload)
- Display (MDSS, cover display SPI panel if it exists in the Redmi tree)
- WiFi (prima WCNSS configuration)

**Security patches from `android-linux-stable/msm-4.9`:** Merge all current security backports. These are CVE fixes backported from upstream LTS to the MSM-4.9 base. They are not hardware-specific and can be merged cleanly.

The assembled kernel tree is committed to `kernel/babb/msm8937` (Babb's own repo) with a clear commit history showing base, reference patches, and security patches as separate commit groups.

### 2.3 — Device Tree Establishment

The device tree for `device/babb/cats22flip` is assembled from:

**Base:** The AOSP device tree extracted from the firmware dump (`repos/firmware-dumps/cat-s22flip/aosp-device-tree/`).

**DTS hierarchy:**
```
qm215.dtsi          ← Qualcomm provided (from CAF kernel)
  └── qm215-qrd.dtsi  ← Qualcomm reference design
        └── msm8937-babbcat.dts  ← S22 Flip specific (Babb's own)
```

`msm8937-babbcat.dts` contains the device-specific additions: cover display SPI node (from Phase 1.4 DTB decompilation), lid sensor GPIO (from Phase 1.4), AW881xx amplifier I2C address, keypad GPIO assignments.

**defconfig:** `msm8937go_babbcat_defconfig` — the Babb Cat kernel configuration. Built from `msm8937go_defconfig` (the Go edition baseline) with explicit additions and removals documented in `kernel/kernel-patches.md`:

Critical flags (from existing research):
```
CONFIG_ANDROID_BINDER_IPC_32BIT    # MUST be absent (64-bit binder on 32-bit kernel)
CONFIG_ZRAM=y                       # required for 2GB Go device
CONFIG_ZRAM_LZ4_COMPRESS=y         # speed over ratio for RAM decompression
CONFIG_F2FS_FS=y                    # userdata filesystem
CONFIG_F2FS_FS_ENCRYPTION=y        # FBE via Qualcomm ICE
CONFIG_SECURITY_SELINUX=y          # SELinux enforcing
CONFIG_DM_VERITY=y                  # system partition integrity
```

HOME-specific additions to defconfig:
```
CONFIG_CGROUPS=y
CONFIG_CGROUP_CPUACCT=y
CONFIG_CGROUP_MEM_RES_CTLR=y
CONFIG_CGROUP_FREEZER=y            # for Outstack process suspension
CONFIG_IMA=y                        # Integrity Measurement Architecture
CONFIG_IMA_MEASURE_PCR_IDX=10
CONFIG_SECURITY_SELINUX_DEVELOP=n  # enforcing by default, no permissive switch
```

### 2.4 — Docker Build Pipeline

`repos/tools/docker-aosp/` provides a containerized build environment. Configure it for Babb Cat:

- Dockerfile extended to include Babb's signing key import (development key for debug builds; production key managed separately)
- Build script `babbcat-build.sh` that executes: `source build/envsetup.sh && lunch babb_S22FLIP-userdebug && make -j$(nproc)`
- Output: `out/target/product/S22FLIP/` containing `boot.img`, `system.img`, `vendor.img`, `super.img`
- Artifact naming: `babbcat-{version}-{builddate}-{buildtype}.zip`

Reproducibility test: build the same commit twice on the same host and confirm byte-identical output for all partition images. AOSP builds are deterministic when the build timestamp is fixed; the build script must fix `BUILD_DATETIME` to achieve this.

### 2.5 — Flash and Recovery Workflow

Document the complete flash procedure for Babb Cat in `build/flash-procedure.md`:

```bash
# Unlock bootloader (one-time, wipes userdata)
fastboot oem unlock

# Flash all partitions
fastboot flash boot boot.img
fastboot flash dtbo dtbo.img
fastboot flash vbmeta vbmeta.img --disable-verity --disable-verification
fastboot flash super super.img  # for dynamic partition devices

# Flash vendor (via recovery if dynamic)
adb sideload babbcat-release.zip

# Reboot
fastboot reboot
```

Note: the `--disable-verity` flag during development builds is temporary. Production builds use `vbmeta.img` with AVB signatures and verification enabled. The development workflow disables verity to allow system partition modification during testing; no release image ships with verity disabled.

**Phase 2 Outputs:**
- AOSP source synced to `~/babbcat/` with Babb manifest overlay
- `kernel/babb/msm8937` repo established with base + reference + security patches
- `device/babb/cats22flip` device tree repo established
- `msm8937go_babbcat_defconfig` documented and committed
- Docker build pipeline operational — first test build completes without error
- `build/flash-procedure.md` complete

**Phase 2 Exit Condition:** Docker build produces `boot.img`, `system.img`, `vendor.img` without errors. Flash procedure document complete and reviewed.

---

## Phase 3: Bootloader and Recovery
### *Control of the boot chain is control of the device*

**Entry condition:** Phase 2 complete. Device physically available.

**Duration:** 1–2 weeks

### 3.1 — Bootloader Unlock

Execute the bootloader unlock from `bootloader/bootloader-unlock.md`:

```bash
# Enable OEM unlock in Settings → Developer Options first
adb reboot bootloader
fastboot oem unlock
# Device reboots, wipes userdata, shows orange state warning
```

**Orange state documentation:** After unlock, the device displays a warning screen on every boot. Document the exact text, the countdown duration, and the persistent indicator (the "orange padlock" in status bar or boot screen). This documentation is the honest disclosure to every Babb Cat user — they must understand what orange state means.

The `devinfo` partition is written (not an eFuse — reversible). The device can be re-locked with `fastboot oem lock`, which re-wipes userdata. Babb Cat does not re-lock the bootloader because the OEM key embedded in aboot cannot be replaced with a Babb key on this platform generation.

This is a known ceiling, documented in `TeluxOS-Bedrock-Purity-Assessment.md` and acknowledged rather than papered over.

### 3.2 — TWRP Recovery Build

Build TWRP using `repos/device-trees/cat-s22flip-twrp/` as the device tree.

**Critical correction** (from existing research): the auto-generated device tree specifies the recovery partition as 64MB. The actual recovery partition is **32MB**. Confirm this in the device tree:

```
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 33554432  # 32MB in bytes — NOT 67108864
```

Build TWRP from source with this correction applied. A TWRP image built for 64MB that tries to flash into a 32MB partition corrupts the partition. This one number change is critical.

Additional TWRP requirement: Keymaster 4.0 blobs for userdata decryption. The device's userdata partition uses f2fs with FBE; TWRP must decrypt userdata to back it up. Extract the required blobs from the vendor partition and include them in the TWRP ramdisk:
- `/vendor/lib64/hw/android.hardware.keymaster@4.0-impl-qti.so`
- `/vendor/bin/hw/android.hardware.keymaster@4.0-service-qti`

Without these, TWRP sees the userdata partition as 0 bytes.

### 3.3 — AVB Signing Key Generation

Generate the Babb Cat development signing key pair:

```bash
# ed25519 key for AVB signing (not the same as the Telux sovereign key)
openssl genpkey -algorithm ed25519 -out babbcat-avb-dev.pem
openssl pkey -in babbcat-avb-dev.pem -pubout -out babbcat-avb-dev-pub.pem
```

For production: generate on air-gapped hardware, store private key in hardware security module (Nitrokey HSM or equivalent). The production signing key is used for every release image and its compromise means the entire release process is compromised.

For development: the dev key is in the Docker build environment. It signs debug and testing builds only.

Document key management policy in `bootloader/avb-signing.md` (extend the existing document with Babb's specific key hierarchy and rotation plan).

### 3.4 — First Custom Boot

With TWRP installed and the first AOSP build from Phase 2 available:

```bash
# Flash custom recovery
fastboot flash recovery twrp-babbcat.img
# Boot into recovery
fastboot reboot recovery
# In TWRP: sideload first build
adb sideload babbcat-debug-001.zip
# Reboot
adb reboot
```

**Acceptance criteria for first boot:**
- Device boots past Android logo (no bootloop)
- `adb shell` accessible
- `adb shell uname -r` returns expected kernel version
- `adb shell getenforce` returns `Enforcing`
- Device has no GMS packages (`adb shell pm list packages | grep google` returns empty)

This first boot will almost certainly have broken hardware (WiFi won't work, audio won't work, cover display won't work). That is expected. The goal of this phase is a bootable system with working ADB. Hardware bring-up is Phase 5.

**Phase 3 Outputs:**
- Bootloader unlocked, orange state documented
- TWRP built with 32MB recovery partition correction and Keymaster blobs
- AVB signing key hierarchy established (dev key operational, production key plan documented)
- First custom boot achieved — device boots to AOSP launcher via Babb Cat build
- `bootloader/bootloader-unlock.md` and `bootloader/avb-signing.md` updated with empirical results

**Phase 3 Exit Condition:** Device boots to launcher. ADB works. SELinux enforcing. No GMS.

---

## Phase 4: Kernel Engineering
### *The kernel is the boundary between hardware and everything else*

**Entry condition:** Phase 3 complete. First custom boot achieved.

**Duration:** 3–5 weeks (this is the most technically demanding phase)

### 4.1 — Kernel Module Architecture

The MSM8937 audio subsystem requires 25+ loadable kernel modules. These must be built, signed (with the same key that signs the kernel), and loaded in the correct order at boot. The `kernel/kernel-modules.md` document lists all required modules.

Audio module load order (from existing research):
```
1. snd-soc-core.ko
2. q6core.ko, q6.ko (QDSP6 core)
3. wcd9xxx-core.ko (WCD9335 codec core)
4. wcd9335.ko (Tasha codec)
5. wcd-cpe-core.ko (compressed audio offload)
6. wcd9335-mbhc.ko (headset detection)
7. msm-pcm-routing-v2.ko (audio routing)
8. aw881xx.ko (Awinic smart amplifier)
```

The AW881xx module (`aw881xx.ko`) must load after the core audio stack is initialized. Its firmware (`aw881xx_*.bin`) must be present in `/vendor/firmware/` before the module loads. The I2C address of the amplifier chip (obtained from Phase 1.4 DTB decompilation) must match the module's device tree node.

### 4.2 — WiFi Module Alignment

The WCNSS WiFi driver (`wlan.ko`) is from the `prima` subsystem — an older Qualcomm WiFi architecture used in the MSM8937. It is in `drivers/staging/prima/` in the kernel tree. Two alignment requirements:

**WCNSS firmware version:** The `WCNSS_qcom_cfg.ini` file in `/vendor/firmware/wlan/prima/` must exactly match the version expected by `wlan.ko`. Building `wlan.ko` from source and using vendor firmware from the stock image requires verifying version compatibility. A mismatch causes WiFi initialization failure at boot.

**Module signing:** `wlan.ko` must be signed with the same key as the kernel. If the kernel is built with `CONFIG_MODULE_SIG_FORCE=y`, unsigned modules are rejected. Build `wlan.ko` as part of the kernel build, not separately.

### 4.3 — Cover Display Driver Integration

From Phase 1.4: the cover display controller model is now known. Integrate the driver:

For ST7789 (most likely based on 1.44" 128×128 resolution):
- `CONFIG_TINYDRM_ST7789V=y` — ST7789V DRM driver is in mainline kernel since 4.15
- For kernel 4.9: the `spi-gpio` driver and a framebuffer driver (fbtft) can drive SPI panels; `fbtft` is in `drivers/staging/fbtft/`
- Add DTS node for the cover display SPI panel using GPIO assignments from Phase 1.4

```dts
&spi_N {  /* N = SPI bus number from DTB decompilation */
    status = "okay";
    cover_display: st7789@0 {
        compatible = "sitronix,st7789v";
        reg = <0>;
        spi-max-frequency = <32000000>;
        dc-gpios = <&tlmm DC_GPIO_NUM 0>;
        reset-gpios = <&tlmm RST_GPIO_NUM 0>;
        width = <128>;
        height = <128>;
        buswidth = <8>;
    };
};
```

### 4.4 — Lid Sensor Integration

From Phase 1.4: the Hall effect sensor GPIO is now known. The lid sensor generates `SW_LID` input events that Android's `PhoneWindowManager` uses for display sleep/wake on fold.

In the DTS, the lid sensor is an input GPIO key:
```dts
gpio_keys {
    compatible = "gpio-keys";
    lid_switch: lid-switch {
        label = "Lid Switch";
        gpios = <&tlmm LID_GPIO_NUM GPIO_ACTIVE_LOW>;
        linux,input-type = <5>;    /* EV_SW */
        linux,code = <0>;          /* SW_LID */
        gpio-key,wakeup;
        debounce-interval = <15>;
    };
};
```

Verify by running `adb shell getevent -l` and opening/closing the flip while watching for `SW_LID` events.

### 4.5 — Physical Keypad Mapping

The physical T9 keypad generates `EV_KEY` events. Run `adb shell getevent -l` on the stock device and press each physical key to map Linux keycodes. Produce `hardware/keypad-mapping.md` with the complete mapping.

Create the Android keylayout file (`S22FLIP.kl`) mapping Linux keycodes to Android `KEYCODE_*` values:

```
# S22FLIP.kl
key 11   0          # 0 key → KEYCODE_0
key 2    1          # 1 key → KEYCODE_1
...
key 232  CALL       # Send/Call key → KEYCODE_CALL
key 107  ENDCALL    # End key → KEYCODE_ENDCALL
key 158  BACK       # Back key → KEYCODE_BACK
```

The T9 multi-tap input method for text entry is a separate concern addressed in Phase 6 (the Babb OS layer).

### 4.6 — Power Management Kernel Configuration

This phase establishes the kernel's power management foundation that Outstack (Phase 7) builds on.

**CPU power collapse:** Confirm `cpuidle` driver is active and all four sleep states are available:
```bash
adb shell cat /sys/devices/system/cpu/cpu0/cpuidle/state*/name
# Expected: WFI, retention, standalone_pc, pc
```

**RPM power domains:** Confirm `qcom,rpmpd` is initialized:
```bash
adb shell ls /sys/kernel/debug/power/
adb shell cat /sys/kernel/debug/rpm_stats  # if debugfs mounted
```

**PMIC sysfs interface:** Confirm battery state is readable:
```bash
adb shell cat /sys/class/power_supply/battery/capacity
adb shell cat /sys/class/power_supply/battery/status
adb shell cat /sys/class/power_supply/battery/current_now
```

**eDRX configuration preparation:** The modem's eDRX cycle is configured via AT commands through the RIL. Confirm the RIL is initialized and AT command passthrough is possible:
```bash
adb shell service call phone 85 # implementation-specific
# Or via qmicli if available
```

Document baseline power draw in each state (screen on / screen off / deep sleep) using current measurements. This is the pre-Outstack baseline against which Outstack's improvements will be measured.

**Phase 4 Outputs:**
- `kernel/kernel-build.md` updated with complete build procedure and module signing
- All 25+ audio modules loading correctly (confirmed via `adb shell lsmod`)
- WiFi initializing at boot (confirmed via `adb shell ip link show wlan0`)
- Cover display DTS node integrated (driver loads, display node appears in `/sys/class/drm/`)
- Lid sensor generating `SW_LID` events (confirmed via `getevent`)
- Keypad fully mapped (`hardware/keypad-mapping.md` and `S22FLIP.kl` complete)
- Power baseline documented (idle draw in mW for each state)

**Phase 4 Exit Condition:** All hardware subsystems have kernel drivers initialized at boot. No kernel panics or module load failures. Power baseline documented.

---

## Phase 5: Hardware Bring-Up
### *Every subsystem validated in isolation, then together*

**Entry condition:** Phase 4 complete. All kernel drivers initialized.

**Duration:** 2–4 weeks

### 5.1 — Telephony Bring-Up

The telephony stack is the most critical function of the Babb Cat device for its Zambia deployment context. It must be validated completely before any other application-layer work.

**Bring-up sequence:**

1. Confirm modem initialization: `adb shell getprop gsm.sim.state` should return `READY` with a SIM inserted
2. Confirm voice call (outgoing): dial a known number, confirm audio in earpiece
3. Confirm voice call (incoming): call the device, confirm ring, answer, audio both ways
4. Confirm SMS: send and receive
5. Confirm data: LTE data session established, `adb shell ping -c 3 1.1.1.1`
6. Confirm USSD: dial `*778#` (or local equivalent), confirm menu appears and responds
7. Confirm STK: confirm the Airtel SIM's STK menu appears in the phone's menu system on SIM insertion

**Known failure modes from existing research:**
- MCFG carrier profile mismatch → APN not auto-configured → no data. Fix: manually configure APN or add MCFG profile to vendor partition.
- STK app not in system image → `com.android.stk` missing → mobile money menus never appear. Fix: confirm `PRODUCT_PACKAGES` includes `Stk`.
- USSD session timeout → sequence of rapid menu responses fails → mobile money transaction incomplete. Fix: tune `TelephonyManager` USSD session timeout via system property.

### 5.2 — Audio Bring-Up

Audio on the MSM8937 is multi-component. Test each path independently:

- Earpiece (call audio): confirmed in 5.1 voice call test
- Speaker: play audio, confirm AW881xx amplifier firmware loaded and amplifier driving speaker
- Microphone: record audio, confirm capture path working
- Headset: insert headset, confirm MBHC (headset detection) detects insertion, confirm audio routing switches to headset output and headset microphone
- LPASS offload: play audio, confirm via kernel log that LPASS DSP is handling playback (CPU in idle state during playback)

### 5.3 — Display Bring-Up

Primary display (4.0" IPS, MDSS):
- Full color output confirmed
- Touch screen responsive (calibrated by AOSP framework automatically)
- Brightness control working (`/sys/class/backlight/`)
- Display power collapse: screen off, screen on, no ghosting or corruption

Cover display (1.44" SPI panel — if driver integrated in Phase 4):
- Framebuffer appears in `/dev/fb1` or `/dev/dri/card1`
- Static image can be written to framebuffer
- Panel responds to SPI commands
- Failure here is non-blocking for Phase 5: cover display is enhanced functionality, not core

### 5.4 — Camera Bring-Up

Camera bring-up is desirable but not on the critical path for Babb Cat's initial deployment. The Zambia use case does not center on camera functionality. Camera should:
- Initialize without crashing Camera HAL
- Preview functional
- Capture functional

Failures are logged in `project/known-issues.md` and addressed in a maintenance release, not blocking the core release.

### 5.5 — Sensor Bring-Up

- Accelerometer: `adb shell dumpsys sensorservice | grep accelero` — confirm sensor present
- GPS: confirm location lock in outdoor test
- Proximity sensor: confirm screen blanks when proximity sensor triggered (during call simulation)
- Hall effect (lid): confirmed in Phase 4 — generating `SW_LID` events

### 5.6 — Power Measurement Baseline

Establish empirical power measurements for each operational state of the hardware bring-up build. Use a USB power meter (e.g., Otii Arc, Nordic PPK2, or Monsoon) between the charger and the device, or measure the battery's current draw directly.

Target measurements:
```
State                              | Target Current Draw
Screen on, active use (calls+data) | < 400mA
Screen on, idle                    | < 200mA
Screen off, WiFi connected         | < 50mA
Screen off, WiFi off, modem active | < 30mA
Deep sleep (modem eDRX)            | < 15mA
```

These measurements are the Babb Cat hardware baseline. Outstack (Phase 7) is measured against this baseline. Every power optimization claimed in Babb Cat's release notes is relative to a number measured here.

**Phase 5 Outputs:**
- All telephony paths confirmed: voice, SMS, data, USSD, STK
- Audio all paths confirmed: earpiece, speaker, mic, headset, LPASS offload
- Primary display confirmed: color, touch, brightness, power collapse
- Sensors confirmed: accelerometer, GPS, proximity, lid sensor
- `project/known-issues.md` updated with any hardware issues not yet resolved
- Power measurement baseline documented in `os/power-management.md`

**Phase 5 Exit Condition:** Telephony fully functional including USSD and STK. All P0 tests in `project/test-plan.md` passing.

---

## Phase 6: Babb OS — De-Googling and Zambia Configuration
### *The phone must serve its user, not its original manufacturer*

**Entry condition:** Phase 5 complete. Fully functional hardware.

**Duration:** 2–3 weeks

This phase transforms the raw AOSP build into Babb OS: a de-Googled Android configured specifically for the Zambia deployment context. This is the first layer of HOME's expression — sovereignty at the network and service level, before the Telux OS primitives are added.

### 6.1 — GMS Elimination Confirmation

From `os/de-googling.md`, GMS is not automatically included in a clean AOSP build. The primary task is ensuring it does not accidentally enter.

Remove from device tree:
```makefile
# Remove in device.mk or lineage_S22FLIP.mk:
PRODUCT_GMS_CLIENTID_BASE := android-cat  # Remove this line entirely
```

Confirm no GMS packages in final build:
```bash
adb shell pm list packages | grep -E "google|gms|play|firebase"
# Expected: empty output
```

Confirm no GMS network contacts: run a packet capture during a full boot cycle. No traffic should originate from the device to any Google IP range (8.8.0.0/8, 74.125.0.0/8, 142.250.0.0/15, 172.217.0.0/16, etc.) unless the user explicitly opens a browser and navigates to a Google property.

### 6.2 — GMS Function Replacement

Each GMS function requires a replacement or an explicit decision that the function is not needed:

**Push notifications (FCM → UnifiedPush + ntfy):**
- ntfy.sh Android client included in system image as privileged app
- ntfy server endpoint configured to `notify.babb.tel` (Babb-controlled server)
- Applications that support UnifiedPush receive notifications via ntfy
- Applications that don't support UnifiedPush: polling is acceptable for Babb Cat's use case
- No GMS-dependent notification path should be present

**Application distribution (Play Store → F-Droid):**
- F-Droid installed as privileged app in `/system/priv-app/F-Droid/`
- Privileged installation: F-Droid can install and update apps silently without additional user permission grants
- Include `PRODUCT_PACKAGES += F-DroidPrivilegedExtension` in device.mk
- F-Droid repository pre-configured with Babb's own F-Droid repository (for Babb-specific apps) alongside the main F-Droid repository

**Location (Google FLP → AOSP GPS + Network Location):**
- Standard AOSP GPS provider is functional (confirmed in Phase 5)
- Network-based location: use Mozilla Location Service or Beacondb instead of Google NLS
- Pre-install Organic Maps for offline maps (Zambia dataset pre-seeded)

**Authentication (Google Account → Telux DID):**
- Phase 8 adds the Telux identity layer; this phase removes the Google Account framework integration
- No Google Account prompts in setup wizard
- No Google Account required for any pre-installed application

**Captive portal detection (connectivity check → Babb endpoint):**
```bash
# System property to redirect captive portal check
adb shell settings put global captive_portal_http_url http://204.babb.tel
adb shell settings put global captive_portal_https_url https://204.babb.tel
```
The `204.babb.tel` endpoint returns HTTP 204 (No Content) — the correct response for an open internet connection — and generates no user data collection.

### 6.3 — Zambia Carrier Configuration

**APN Pre-configuration:**

```xml
<!-- res/xml/apns-conf.xml additions -->
<apn carrier="Airtel Zambia" mcc="645" mnc="01"
     apn="airtelgprs.com" type="default,supl" />
<apn carrier="Zamtel" mcc="645" mnc="02"
     apn="internet" type="default,supl" />
<apn carrier="MTN Zambia" mcc="645" mnc="03"
     apn="internet" type="default,supl" />
```

**MCFG Carrier Profile Verification:**

Verify `/vendor/firmware/mcfg/` contains `mcc645_mnc01.conf` (Airtel), `mcc645_mnc02.conf` (Zamtel), `mcc645_mnc03.conf` (MTN). If absent: add generic profiles from Qualcomm's MCFG tools, or file a bug with the carrier to request provisioning information.

**eDRX Configuration per carrier:**

```
Airtel Zambia:  eDRX cycle 20.48s (value 0x05) — confirmed LTE eDRX support
Zamtel:         eDRX cycle 10.24s (value 0x04) — conservative pending confirmation
MTN Zambia:     eDRX cycle 10.24s (value 0x04) — conservative pending confirmation
```

These configurations are applied by the Outstack daemon in Phase 7; this phase documents the target values.

**VoLTE Baseline:**

- Baseline commitment: CSFB (Circuit Switched Fall Back) for voice on LTE-connected devices
- CSFB works without any configuration — it is the Android default when VoLTE is not provisioned
- Document VoLTE provisioning requirements for each carrier (whether they provision custom devices)
- VoLTE is tracked in `project/known-issues.md` as a post-release enhancement, not blocking

### 6.4 — First-Run Experience (Babb Setup Wizard)

Replace the AOSP Setup Wizard with a Babb-specific first-run experience. Implemented as a system app (`BabbSetupWizard.apk`) in `/system/priv-app/`.

Wizard steps:
1. Language selection from Zambia-relevant set: English (Zambia), Bemba, Nyanja, Tonga, Lozi
2. Timezone confirmation: Africa/Lusaka
3. SIM detection: display detected carrier (Airtel/Zamtel/MTN), confirm APN
4. PIN/password setup: establish FBE user credential
5. Privacy disclosure: explicit statement of what Babb Cat does not do (no Google, no data collection), what it does (ledger records of exchange within Islands — Phase 8)
6. No account required: the wizard does not prompt for any external account

The wizard must not contact any remote server during execution except: NTP time sync to `time.babb.tel`.

### 6.5 — Local Language Input Methods

Install keyboard input methods for Zambia's major languages. All character sets are Latin-based with minor diacritical additions; T9 multi-tap for the physical keypad must support the extended character set.

For touchscreen input (when the flip is open and users prefer touch):
- GBoard alternative: `AnySoftKeyboard` from F-Droid with English + Bemba keyboard layouts
- Configure default to English (Zambia) locale

For physical T9 keypad:
- The physical keypad generates key events; text entry via T9 multi-tap is handled by the Input Method framework
- Custom T9 IME that supports extended characters: `ñ`, `ŵ`, `ŋ` accessible via long-press on relevant keys
- This is a custom development task; no existing Android T9 IME correctly handles all Zambia language characters

### 6.6 — Mobile Money Integration Testing

Testing against live carrier SIMs — this is the P0 validation for the Zambia deployment context.

**Airtel Money (`*778#`):**
- USSD session opens, menu appears
- Navigate through balance check, send money, buy airtime
- Confirm each menu level responds correctly
- Confirm STK Airtel Money menu accessible from phone menu

**Zamtel Kwacha (`*112#`):** Same procedure

**MTN MoMo (`*303#`):** Same procedure

Every test must be run on a live SIM on each carrier's live network. Emulation or simulation does not validate MCFG profiles, carrier provisioning, or USSD session handling.

Failures are categorized: OS bug (our fix), carrier provisioning issue (carrier relationship required), or network issue (retry test). No release ships without all three carriers confirmed passing.

**Phase 6 Outputs:**
- Zero GMS packages in build, zero GMS network traffic confirmed via packet capture
- All GMS functions replaced with open alternatives
- Zambia carrier APNs pre-configured and confirmed
- eDRX target values documented
- Babb Setup Wizard implemented and functional
- Mobile money USSD and STK confirmed on all three Zambia carriers
- T9 IME with Zambia language support implemented

**Phase 6 Exit Condition:** All P1 tests in `project/test-plan.md` passing. Mobile money confirmed on all three carriers. Zero GMS network traffic in packet capture.

---

## Phase 7: Outstack — Power as First Principle
### *The phone knows how much it has and what matters most*

**Entry condition:** Phase 6 complete. Fully functional Babb OS baseline.

**Duration:** 3–4 weeks

Outstack is HOME's power governance system. It runs as a native daemon before any application-layer service starts and governs what runs, when, in what order, with what resources. This phase implements Outstack on Babb Cat.

### 7.1 — outstack-powerd: The Power Governor

**Implementation:** Native C daemon compiled for armv7-a (ARM 32-bit, matching the device architecture). Started by Android `init` from `/system/etc/init/outstack-powerd.rc` at boot priority class `core` — before any application services.

```rc
service outstack-powerd /system/bin/outstack-powerd
    class core
    user root
    group root system
    seclabel u:r:outstack_powerd:s0
    oneshot
```

The daemon implements the five-mode state machine:

```c
typedef enum {
    OSTK_MODE_FULL        = 0,  // External power or >80% battery
    OSTK_MODE_NORMAL      = 1,  // 60–80%
    OSTK_MODE_CONSERVE    = 2,  // 20–60%
    OSTK_MODE_CRITICAL    = 3,  // 5–20%
    OSTK_MODE_EMERGENCY   = 4,  // <5%
} outstack_mode_t;
```

Hysteresis thresholds (entry lower than exit to prevent oscillation):
```c
static const int MODE_ENTRY_THRESHOLDS[] = {80, 60, 20, 5};
static const int MODE_EXIT_THRESHOLDS[]  = {85, 65, 25, 8};
```

Battery reading: polls `/sys/class/power_supply/battery/capacity` every 60 seconds. Mode transitions trigger immediately; polling is not the wakeup path (the kernel power supply subsystem generates uevents on significant battery changes that the daemon subscribes to via netlink).

### 7.2 — cgroup Hierarchy for Power Classes

The five Outstack process classes are implemented as cgroups v1 sub-hierarchies:

```bash
# Created by outstack-powerd at startup
mkdir -p /sys/fs/cgroup/cpu/outstack/{critical,interactive,background,deferred,opportunistic}
mkdir -p /sys/fs/cgroup/memory/outstack/{critical,interactive,background,deferred,opportunistic}
mkdir -p /sys/fs/cgroup/freezer/outstack/{deferred,opportunistic}
```

CPU share allocation per mode:

| Class | FULL | NORMAL | CONSERVE | CRITICAL | EMERGENCY |
|-------|------|--------|----------|----------|-----------|
| critical | 1024 | 1024 | 1024 | 1024 | 1024 |
| interactive | 512 | 512 | 512 | 256 | 0 (suspend) |
| background | 128 | 128 | 64 | 0 (suspend) | 0 (suspend) |
| deferred | 64 | 64 | 0 (freeze) | 0 (freeze) | 0 (freeze) |
| opportunistic | 32 | 0 (freeze) | 0 (freeze) | 0 (freeze) | 0 (freeze) |

Freeze (via `cgroup/freezer`) means processes are suspended (SIGSTOP), preserving state. They will resume when mode improves. Kill means processes are terminated and must restart. Babb Cat's model is freeze-not-kill wherever possible — no data is lost, no work is redone.

### 7.3 — Process Class Assignment

Process class is stored in each executable's extended attributes:

```bash
setfattr -n user.outstack.power_class -v interactive /system/bin/app_process32
setfattr -n user.outstack.power_class -v critical /system/bin/outstack-powerd
setfattr -n user.outstack.power_class -v critical /system/bin/rild
setfattr -n user.outstack.power_class -v background /system/bin/netd
```

The `outstack-powerd` daemon reads this xattr when assigning new processes to cgroups. Processes without a declared class default to `background`.

**Special cases:**
- Android `system_server`: `critical` (contains Telephony, WindowManager, PowerManager — must always run)
- `rild` (Radio Interface Layer daemon): `critical` (telephony)
- `surfaceflinger`: `interactive` (display compositor)
- `mediaserver`: `background` (media playback paused in CRITICAL mode)
- Workpads app (Phase 10): `interactive` when in foreground, `background` when backgrounded

### 7.4 — Lid-Close Mode Trigger

The lid sensor generates `SW_LID` events. Outstack subscribes to these via the Android `InputManager` or directly via `/dev/input/` poll:

```
Lid CLOSE event → Outstack checks current mode
  If FULL or NORMAL → transition to minimum CONSERVE
  If already CONSERVE, CRITICAL, or EMERGENCY → no change (stay or continue degrading)
  
Lid OPEN event → Outstack checks battery
  Resume normal mode progression based on current battery level
```

This is the physical mode transition that makes the flip form factor semantically meaningful in HOME. Closing the flip is not just "turn off the display." It is an intentional act that communicates reduced power urgency and triggers suspension of background Island activity.

### 7.5 — eDRX Coordination

Outstack coordinates eDRX cycle length with the current power mode. The daemon sends AT commands through the telephony HAL to configure the modem's eDRX behavior:

```
OSTK_MODE_FULL:       eDRX disabled (immediate paging response)
OSTK_MODE_NORMAL:     eDRX cycle 5.12s (0x03)
OSTK_MODE_CONSERVE:   eDRX cycle 10.24s (0x04)
OSTK_MODE_CRITICAL:   eDRX cycle 20.48s (0x05)
OSTK_MODE_EMERGENCY:  eDRX cycle 40.96s (0x06)
```

Carrier must support eDRX for this to be effective. Airtel Zambia LTE is confirmed to support eDRX. MTN and Zamtel support is validated during Phase 6 live SIM testing.

### 7.6 — Power Measurement: Outstack vs. Baseline

Repeat the power measurements from Phase 5.6 with Outstack active. The improvement over baseline is the core measurable claim of Babb Cat's power architecture.

Target improvements over Phase 5 baseline:

| State | Baseline | Outstack Target | Improvement |
|-------|----------|----------------|-------------|
| Screen off, all GMS removed | ~30mA | ~30mA | (baseline established in Phase 6) |
| Screen off, eDRX active | ~30mA | ~18mA | eDRX power saving |
| Lid closed, CONSERVE mode | ~18mA | ~12mA | background suspension |
| Deep sleep, CRITICAL mode | ~12mA | ~8mA | deferred freeze + max eDRX |

At 8mA idle from a 1450mAh battery: 181 hours theoretical standby. Real-world target accounting for wakeup events: 72–108 hours (3–4.5 days).

### 7.7 — Power Anomaly Detection

Outstack monitors Island power budget consumption and flags anomalies. In Phase 7, before Islands exist (Phase 8), the anomaly detection applies to process-level accounting:

- Process draws more than 2× its class allocation for more than 60 seconds → log warning
- System-level draw exceeds expected ceiling for current mode by >20% for >120 seconds → log anomaly event
- Anomaly events are written to `/data/outstack/power-anomalies.log` (CBOR-encoded, ring buffer)

The CBOR encoding of anomaly events is the first usage of the exchange ledger record format in Babb Cat — not a ledger entry yet, but the same encoding, establishing the format in practice before the full ledger is implemented.

**Phase 7 Outputs:**
- `outstack-powerd` binary operational, starting before application services
- Five-mode state machine verified against battery threshold changes
- cgroup hierarchy created and process assignments confirmed
- Lid-close trigger verified (mode transition on physical flip close)
- eDRX coordination confirmed (modem eDRX cycle changes on mode transition)
- Power measurements: Outstack vs. baseline documented in `os/power-management.md`
- Power anomaly logging operational

**Phase 7 Exit Condition:** Measurable power improvement over Phase 5 baseline documented. Lid-close triggers mode transition. eDRX cycles confirmed on carrier network.

---

## Phase 8: Telux Identity and Island Foundation
### *Every entity has a name. Every space has a sovereign.*

**Entry condition:** Phase 7 complete. Outstack operational.

**Duration:** 3–4 weeks

This phase implements the Telux identity layer and Island primitive — the foundation of HOME's exchange architecture. No exchange ledger yet (Phase 9). This phase establishes: who the entities are, what spaces they inhabit, and who governs those spaces.

### 8.1 — telux-identd: The Identity Service

Implements W3C DID identity management backed by Keymaster 4.0.

**DID generation:**
```
did:key:z6Mk... ← ed25519 public key encoded in multibase
```

The device generates its first DID at first boot after Phase 6's setup wizard completes. The private key is generated inside Keymaster 4.0 (TrustZone) and never leaves the secure enclave. The DID is derived from the public key — the DID itself is public information; only the private key is secret.

Implementation in Rust (armv7-a target), accessing Keymaster 4.0 through the Android HAL:

```rust
// telux-identd: DID generation
fn generate_device_did() -> Result<DID> {
    let key_alias = "telux.device.sovereign";
    let keymaster = KeymasterHal::connect()?;
    let public_key = keymaster.generate_key(
        KeyAlgorithm::Ed25519,
        KeyPurpose::Sign | KeyPurpose::Verify,
        key_alias,
    )?;
    Ok(DID::from_ed25519_pubkey(&public_key))
}
```

The device DID is stored in `/data/telux/identity/device.did` (FBE-encrypted under the user's credential). The public key is stored unencrypted (it is public). The private key never appears in this file — it remains in TrustZone.

**DID Document:**
```json
{
  "@context": ["https://www.w3.org/ns/did/v1"],
  "id": "did:key:z6Mk...",
  "verificationMethod": [{
    "id": "did:key:z6Mk...#z6Mk...",
    "type": "Ed25519VerificationKey2020",
    "controller": "did:key:z6Mk...",
    "publicKeyMultibase": "z6Mk..."
  }],
  "authentication": ["did:key:z6Mk...#z6Mk..."]
}
```

**AIDL interface** (`ITeluxIdentityService`):
- `generateDID(alias: String) → DID` — create a new DID backed by a new Keymaster key
- `getDeviceDID() → DID` — return the device's primary sovereign DID
- `sign(did: DID, payload: bytes) → Signature` — sign payload with DID's private key (in TrustZone)
- `verify(did: DID, payload: bytes, sig: Signature) → Boolean` — verify signature against DID's public key
- `createCapabilityToken(grantee: DID, capabilities: List<String>, expiresAt: Long) → CapabilityToken` — create a signed capability delegation

### 8.2 — telux-groupd: Island Registry

The Island registry maintains the live state of all Islands on the device.

**Island data model:**
```rust
struct Island {
    id: IslandId,              // UUID
    name: String,
    sovereign_did: DID,        // The Sovereign's public key
    power_budget_mw: u32,      // Milliwatts allocated by Outstack
    members: Vec<IslandMember>,
    logging_policy: LoggingPolicy,
    succession_rules: SuccessionRules,
    created_at: u64,           // Lamport timestamp
}

struct IslandMember {
    did: DID,
    role: MemberRole,          // Human, AI, Service, Device, Broker
    capabilities: Vec<Capability>,
    joined_at: u64,
    expires_at: Option<u64>,
}
```

**Island storage:** `/data/telux/islands/{island_id}/` (SELinux-labeled `u:object_r:telux_island_data:s0`, FBE-encrypted under the Island's sovereign credential).

**Sovereignty enforcement:** All mutation operations (`createIsland`, `addMember`, `revokeMember`, `dissolveIsland`) require a Keymaster-signed request from the Island's Sovereign DID. The signature is verified by `telux-identd` before `telux-groupd` applies any state change.

**AIDL interface** (`ITeluxGroupService`):
- `createIsland(name: String, sovereignDID: DID, powerBudget: int) → Island`
- `addMember(islandId: IslandId, memberDID: DID, capabilities: List<String>, sovereignSig: Signature) → void`
- `revokeMember(islandId: IslandId, memberDID: DID, sovereignSig: Signature) → void`
- `getIsland(islandId: IslandId) → Island`
- `listIslands() → List<Island>`
- `suspendIsland(islandId: IslandId) → void` — called by Outstack on mode transition

### 8.3 — SELinux Domain Extensions

New SELinux policy types for Telux:

```
# New types
type telux_island, domain;
type telux_sovereign, domain;
type telux_ledger, domain;
type telux_identity, domain;
type outstack_powerd, domain;
type telux_island_data, file_type, data_file_type;
type telux_ledger_data, file_type, data_file_type;

# Core enforcement
neverallow telux_island telux_island:{ file dir } rw_file_perms;
# ^ Islands cannot read each other's data files

allow telux_sovereign telux_island_data:dir { read write search };
# ^ Sovereign can modify any island in its authority

allow telux_ledger telux_ledger_data:file { create append read getattr };
neverallow telux_ledger telux_ledger_data:file write;
# ^ Ledger is append-only — writes but not overwrites
```

These policies are compiled into the device's SELinux policy binary during the AOSP build. `neverallow` rules fail at compile time if violated — they cannot be overridden at runtime.

### 8.4 — The Sovereign Key Ceremony

The sovereign key ceremony is the ritual by which a user establishes their sovereign identity on the device. It happens once, during device setup, after the Babb Setup Wizard completes.

**Ceremony flow:**
1. `telux-identd` generates the device DID (if not already generated)
2. User is presented with: "You are establishing yourself as the Sovereign of this device. Your sovereign key is the authority over all Islands you create. It is backed by this device's hardware security module. If you lose this device, your sovereign key cannot be recovered from a backup — it lives only here. Set a strong PIN to protect it."
3. User sets PIN (the PIN unlocks the TrustZone key for signing operations)
4. `telux-groupd` creates the device's default personal Island with the user's DID as Sovereign
5. The device DID and Island ID are displayed as QR codes that can be photographed for record-keeping (the QR codes contain only public key material — no private key)

This ceremony is the most important user-facing moment in Babb Cat's setup. It is the moment when the user becomes a Sovereign. The UX must communicate the significance without being intimidating.

### 8.5 — Outstack-Island Integration

After Phase 8, Islands exist. Outstack must track power consumption per Island. Integration:

- `telux-groupd` notifies `outstack-powerd` when Islands are created, suspended, or dissolved
- `outstack-powerd` creates per-Island cgroup sub-hierarchies:
  ```
  /sys/fs/cgroup/cpu/outstack/islands/{island_id}/
  /sys/fs/cgroup/memory/outstack/islands/{island_id}/
  ```
- Processes declared as Island members are assigned to Island cgroups by `telux-groupd`
- `outstack-powerd` accounts power draw per Island and reports budget alerts to `telux-groupd`

**Phase 8 Outputs:**
- `telux-identd` operational: device DID generated at first boot, Keymaster-backed
- `telux-groupd` operational: Islands created and listed via AIDL
- Sovereign key ceremony implemented in setup wizard
- Default personal Island created for the user at setup
- SELinux domain extensions compiled into policy and enforced
- Per-Island Outstack cgroup integration confirmed

**Phase 8 Exit Condition:** Device has a sovereign DID. User can create an Island and add a member. SELinux enforces Island data isolation.

---

## Phase 9: The Exchange Ledger — Every Transfer Recorded
### *The record is the commitment. The commitment is the system.*

**Entry condition:** Phase 8 complete. Identity and Islands operational.

**Duration:** 4–5 weeks

This is the most architecturally significant phase. The exchange ledger is what makes HOME more than a mobile OS. It is the Telux exchange layer made concrete — the digital equivalent of the Sumerian clay token.

### 9.1 — The HOME Record Format: Extended bitpad

The Workpads `bitpad-v1` codec is the accounting notation. Babb Cat extends it for the HOME exchange ledger context while maintaining backward compatibility with existing Workpads records.

**Existing bitpad-v1 record types** (from `workpads-standard/`):
- `invoice`, `quote`, `receipt`, `job`, `expense`, `payment` — commercial exchange records
- `state_commit` — closes a chain (work complete, payment confirmed, terms accepted)
- `amendment` — corrects a prior record
- `dispute` — opens a dispute on a chain
- `ack` — recipient confirms receipt

**HOME extensions to the codec:**

New BASE_TEMPLATE values:
- `0x07` — `island_exchange` — generic resource transfer between Island members
- `0x08` — `capability_grant` — sovereignty delegation record
- `0x09` — `power_event` — Outstack power anomaly or mode transition record
- `0x0A` — `entity_join` — entity joins an Island (new member record)
- `0x0B` — `entity_leave` — entity leaves an Island

New header fields for HOME records:
- `island_id` — the Island in which this exchange occurred
- `source_did` — DID of the transferring entity
- `destination_did` — DID of the receiving entity
- `authorization_token` — capability token or sovereign signature that authorized this exchange
- `chain_hash` — hash of previous record in this Island's ledger (HOME extension to chain protocol)
- `record_hash` — hash of this record's CBOR encoding (for signature)
- `sovereign_sig` — ed25519 signature by Island Sovereign over `record_hash`

**Backward compatibility:** Records without HOME extension fields are valid standard bitpad-v1 records and are stored in the ledger with `source_did` defaulting to the device DID and `island_id` to the personal Island.

### 9.2 — telux-ledgerd: The Ledger Daemon

**Database design** (SQLite, one database per Island):

```sql
CREATE TABLE ledger (
    seq           INTEGER PRIMARY KEY AUTOINCREMENT,
    record_hash   BLOB NOT NULL UNIQUE,
    chain_hash    BLOB,           -- NULL for first record in island
    island_id     TEXT NOT NULL,
    source_did    TEXT NOT NULL,
    dest_did      TEXT,
    record_type   TEXT NOT NULL,  -- invoice, ack, island_exchange, etc.
    cbor_payload  BLOB NOT NULL,  -- full CBOR-encoded record
    sovereign_sig BLOB NOT NULL,  -- ed25519 signature
    lamport_ts    INTEGER NOT NULL,
    wall_ts       INTEGER NOT NULL,
    INDEX idx_island (island_id),
    INDEX idx_source (source_did),
    INDEX idx_type (record_type),
    INDEX idx_chain (chain_hash)
);

CREATE TABLE chain_registry (
    chain_ref     TEXT PRIMARY KEY,  -- 3-char base64url chainRef from bitpad-v1
    island_id     TEXT NOT NULL,
    opened_at     INTEGER,
    closed_at     INTEGER,          -- NULL if chain still open
    status        TEXT              -- 'open', 'committed', 'disputed'
);
```

**Write path:**
1. Caller provides CBOR record to `telux-ledgerd` via AIDL
2. Daemon validates: Island membership of caller DID, capability token for this record type
3. Daemon computes `record_hash` = SHA-256(cbor_payload)
4. Daemon fetches `chain_hash` = `record_hash` of the most recent record in this Island
5. Daemon calls `telux-identd` to sign `record_hash` with Island Sovereign key
6. Daemon writes record to SQLite with all fields
7. Daemon returns `record_hash` to caller as confirmation

**Append-only enforcement:**
- SQLite database opened with no UPDATE or DELETE permissions in the connection
- SELinux policy `neverallow telux_ledger telux_ledger_data:file write` prevents file overwrites
- Records can only be corrected via `amendment` records (new records that reference and supersede prior records — same as bitpad-v1 amendment protocol)

**Export path:**
- `exportIslandLedger(islandId, since, format)` — exports records since a given timestamp
- Format options: CBOR blob (for transmission), JSON (for inspection), bitpad-v1 URL batch (for SMS transmission of multiple records)

### 9.3 — Chain Protocol Integration

The Workpads chain protocol is directly adopted as the HOME ledger's chain model:

- Records share a `chainRef` (3-char base64url, from bitpad-v1)
- A chain represents an ongoing exchange relationship (a job, a transaction, a negotiation)
- `state_commit` closes a chain with a final status (complete, confirmed, accepted, disputed)
- `ack` confirms receipt of a specific record
- `dispute` opens a dispute on a chain, triggering notification to all chain members
- `amendment` corrects a record while preserving the original in the ledger

The chain view in the Babb Cat UI (Phase 11) displays records in a chain chronologically, using the connector glyphs from the Workpads chain screen: `◉` first record, `─` intermediate, `◎` state_commit.

### 9.4 — Natural Language Query: Rule-Based Baseline

The primary user interface to the ledger is natural language. Phase 9 implements the rule-based baseline parser — covering the 80% case without any LLM.

**Query patterns and translations:**

```
"what did [entity] send to [entity] [time]?"
→ SELECT * FROM ledger WHERE source_did LIKE '%entity%'
  AND dest_did LIKE '%entity%'
  AND wall_ts > [time_lower] AND wall_ts < [time_upper]
  
"show [me|us] all [record_type] since [time]"
→ SELECT * FROM ledger WHERE record_type = [type]
  AND wall_ts > [time_lower]
  
"who authorized [transfer description]"
→ SELECT authorization_token, source_did FROM ledger
  WHERE cbor_payload LIKE '%description%'
  ORDER BY wall_ts DESC LIMIT 10
  
"what is [entity]'s [metric] this [time period]"
→ aggregation query on financial fields in cbor_payload
```

Time parsing covers: "today", "yesterday", "last [N] days", "this week", "last month", specific dates in `DD/MM/YYYY` and `YYYY-MM-DD` formats.

The parser is implemented as a finite-state machine in Java (for the Android application layer, calling through to `telux-ledgerd`). It does not use regular expressions — it uses a hand-written tokenizer that is fast, deterministic, and produces zero false positives (when uncertain, it returns a structured prompt asking for clarification rather than guessing).

### 9.5 — Ledger Transmission: SMS and Share

Exchange records must be transmissible over any medium — including SMS when data is unavailable. This is the "low-bit" property of HOME's ledger design.

**SMS transmission format:**
- Single record: bitpad-v1 URL encoding (`https://workpads.me/p#1pa/<payload>`) — under 300 characters for typical records
- Multiple records: batch encoding (custom HOME extension) — CBOR array of records, deflate-compressed, base64url — transmitted as multi-part SMS if needed

**Share via Android share sheet:**
- Records can be shared via WhatsApp, SMS, email, or QR code
- The share format is the bitpad-v1 URL — receivable by any Workpads client (KaiOS, browser, Babb Cat)
- QR code generation for in-person record sharing (for contexts where neither party has data)

**Receiving records:**
- Babb Cat registers as a handler for `workpads.me/p#*` URLs
- When a Workpads URL is opened, the record is decoded, stored in the ledger under the personal Island, and displayed for review before acceptance
- Acceptance is explicit: the user (Sovereign of their personal Island) signs an `ack` record confirming receipt

**Phase 9 Outputs:**
- `telux-ledgerd` operational: records written, chain-hashed, Sovereign-signed, queryable
- HOME bitpad extension (templates 0x07–0x0B) implemented in codec
- Full bitpad-v1 backward compatibility confirmed (existing Workpads records importable)
- Rule-based NL query parser: 10 canonical query patterns covered
- SMS and share export: records sharable as bitpad-v1 URL
- Receiving Workpads records from external sources (KaiOS, browser) functional

**Phase 9 Exit Condition:** A complete exchange scenario works end-to-end: two entities exchange a Workpads job record, the record is stored in the Island ledger, the `ack` is generated and transmitted, the chain is visible in the chain view, and the ledger query "what did X send to Y last week" returns the correct record.

---

## Phase 10: Workpads Native — The First HOME Application
### *The exchange record is not a feature. It is infrastructure.*

**Entry condition:** Phase 9 complete. Exchange ledger operational.

**Duration:** 3–4 weeks

Workpads is not an application that runs on Babb Cat. Workpads is the use of Babb Cat's exchange layer. The distinction matters: the existing `workpadskaios/` app wraps the PADS model in a KaiOS application container. On Babb Cat, the PADS record format is a native HOME ledger record type. The Workpads application on Babb Cat is a UI for creating and viewing records whose storage and sharing is entirely managed by `telux-ledgerd`.

### 10.1 — Workpads Android Native

Port the Workpads KaiOS application to Android, rewritten for the Android environment but architecturally identical:

**The PADS wizard:**
- Step 0 — Process: `job` (required), `customer`, `date`
- Step 1 — Actions: `actions[]` — add/list step items
- Step 2 — Details: `worker`, `location`, `customer_phone`, `start_time`, `end_time`, `meeting_time`
- Step 3 — Story: `story`, `details`

Navigation adapted for the physical T9 keypad:
- D-pad Up/Down: navigate fields within a step
- Enter/CSK: advance to next step
- RSK: Save
- LSK: Back/Cancel
- `*` key: Quick Note capture (same as KaiOS version)

**Financial overlay:**
- After PADS wizard completion, optional Financial step: `amount`, `currency`, `record_type` (invoice/quote/receipt)
- `FinancialModel.summarize()` implemented in Java, same logic as KaiOS codec formula
- Finance overview screen: per-currency aggregation, time window filter

**Chain view:**
- Identical to KaiOS chain screen: records sorted by timestamp, connector glyphs
- `state_commit` option accessible from record Options menu
- `ACK` generation from record view — sends `ack` record to Island ledger, shares via bitpad-v1 URL

### 10.2 — Keypad-Optimized UX

The physical T9 keypad is the primary input method for field workers. Every Workpads screen must be fully navigable without touching the touchscreen.

**T9 text entry in wizard fields:**
- Multi-tap: press `2` once = 'a', twice = 'b', three times = 'c', wait 1.2s = confirm character
- Long-press `0` = space
- Long-press `1` = extended character set (for Zambia language characters)
- `*` key = shift (switch between lowercase/uppercase)
- `#` key = number input mode (for amount fields)
- Backspace = delete last character

The custom T9 IME from Phase 6.5 is fully integrated here.

### 10.3 — STK and Mobile Money in Exchange Context

When a user completes a mobile money transaction (via STK or USSD), Babb Cat offers to record the transaction in the Workpads ledger.

Implementation: `com.android.stk` generates intents when STK sessions complete with a result. Babb Cat registers an intent listener that, when a completed USSD/STK session is detected with patterns matching mobile money operators, presents a prompt: "Record this Airtel Money transaction in your ledger? [Amount detected: ZMW 50.00]"

If the user accepts:
- A `payment` record is created in `telux-ledgerd` under the personal Island
- Fields: `amount`, `currency` (ZMW), `record_type: payment`, `customer` (extracted from USSD menu if parseable), `worker` (device DID)
- Record shared via Workpads share if recipient's contact is in the Block Registry

This is not automatic. Every ledger entry requires deliberate user acknowledgment. The sovereign is always in control.

### 10.4 — Block Registry on HOME

The Workpads Block Registry (contact store keyed by customer name) is implemented as a HOME Identity Registry — contacts stored in `telux-identd` as known DIDs with human-readable names.

When a Workpads record is received from an external sender, their `source_did` is added to the Identity Registry with their sender name. Future records from the same DID are recognized and labeled.

This is the beginning of the HOME social graph: not a centralized contacts database, but a local registry of entities you have exchanged records with, each identified by their DID.

**Phase 10 Outputs:**
- Workpads Android Native app: PADS wizard, chain view, financial overview, share/receive
- T9 keypad input fully functional for all text fields
- STK/mobile money ledger recording integration
- Block Registry as Identity Registry in `telux-identd`
- Cross-platform record exchange confirmed: Babb Cat → KaiOS, Babb Cat → browser, KaiOS → Babb Cat

**Phase 10 Exit Condition:** A complete field service workflow works end-to-end: field worker creates Workpads job record on physical T9 keypad, sends to customer via SMS link, customer opens on KaiOS or browser, customer ACKs, ACK appears in Babb Cat Island ledger chain view.

---

## Phase 11: HOME Launcher and Visible Layer
### *The interface expresses the architecture*

**Entry condition:** Phase 10 complete.

**Duration:** 3–4 weeks

The HOME launcher is the visible expression of the Island model. It replaces the AOSP launcher with a purpose-built interface that presents Islands as the primary organizational metaphor.

### 11.1 — HOME Launcher

The launcher is built as an Android System App — a replacement default launcher with the same priority as the AOSP launcher.

**Home screen layout:**

```
┌─────────────────────────────────────┐
│  [Island Name]        [mode icon]   │  ← Active Island header
│  Sovereign: You       [battery %]   │
├─────────────────────────────────────┤
│                                     │
│  ◉ Ledger: 3 new records today      │  ← Exchange summary
│  ─ Last: "Invoice to Chanda, ZMW 80"│
│  ─ Status: 1 chain awaiting ACK     │
│                                     │
├─────────────────────────────────────┤
│  [New Record]  [Ledger]  [Members]  │  ← Primary actions
├─────────────────────────────────────┤
│  Phone    Messages   Maps  Workpads │  ← App shortcuts
└─────────────────────────────────────┘
```

**Island switching:** Long-press the Island name header → Island list. Navigate with D-pad. Select Island to switch context.

**Power mode indicator:** Small icon in header corner shows current Outstack mode (🔋 FULL / ⚡ NORMAL / ⚠ CONSERVE / 🔴 CRITICAL / 🆘 EMERGENCY). Tapping the icon shows: battery %, current draw (if available from hardware), mode duration, and which Islands/services are suspended.

### 11.2 — Cover Display Integration: telux-coverd

`telux-coverd` is a small native process that renders to the cover display framebuffer. It subscribes to:
- Outstack power mode events
- `telux-groupd` Island state events (new members, new records, pending ACKs)
- Battery state changes

Cover display content (128×128 or 64×128 depending on hardware):
```
┌──────────────────┐
│ 14:32            │  ← Time
│ Wed 31 May       │  ← Date
│ ▪▪▪▪░░░░ 42%     │  ← Battery bar
│ NORMAL           │  ← Outstack mode
│ ─────────────    │
│ 2 new records    │  ← Ledger summary
│ 1 ACK pending    │
└──────────────────┘
```

When a call arrives: cover display shows caller ID if in Block Registry, or number only if not.

### 11.3 — Sovereign Dashboard

A dedicated screen (accessible from the launcher's Island header or from Settings) showing:
- All Islands on the device: name, member count, power budget, last activity
- Sovereign key information: DID (public), last signing operation, capability grants active
- Ledger summary: record count, chain count, last chain status
- Power governance: current mode, process class assignments, anomaly log

The sovereign dashboard is the control surface for the HOME architecture. It is where an administrator or the user themself can review the complete state of their sovereign exchange context.

**Phase 11 Outputs:**
- HOME launcher installed and default
- Island-centric home screen functional: active Island shown, ledger summary, power mode
- Island switching via long-press
- Cover display rendering Island status when lid closed
- Sovereign dashboard accessible and showing complete system state

**Phase 11 Exit Condition:** HOME launcher is the default launcher. Cover display shows Island status when lid closed. Sovereign dashboard shows accurate Island and ledger state.

---

## Phase 12: Security Hardening and Release Preparation
### *A sovereign device earns trust by being auditable*

**Entry condition:** Phase 11 complete. All features functional.

**Duration:** 3–4 weeks

### 12.1 — Kernel Security Hardening

Apply full KSPP (Kernel Self-Protection Project) recommendations to `msm8937go_babbcat_defconfig`:

```
# Memory safety
CONFIG_FORTIFY_SOURCE=y
CONFIG_STACKPROTECTOR_STRONG=y
CONFIG_HARDENED_USERCOPY=y
CONFIG_INIT_STACK_ALL_ZERO=y
CONFIG_INIT_ON_ALLOC_DEFAULT_ON=y
CONFIG_INIT_ON_FREE_DEFAULT_ON=y

# Randomization
CONFIG_RANDOMIZE_BASE=y
CONFIG_RANDOMIZE_MEMORY=y

# Access restrictions
CONFIG_STRICT_DEVMEM=y
CONFIG_IO_STRICT_DEVMEM=y
CONFIG_SECURITY_DMESG_RESTRICT=y

# Module security
CONFIG_MODULE_SIG=y
CONFIG_MODULE_SIG_FORCE=y
CONFIG_MODULE_SIG_SHA256=y
```

### 12.2 — SELinux Audit

Run a full SELinux audit on a release build:
```bash
adb shell audit2allow -i /dev/kmsg  # should produce zero output in release build
adb shell cat /proc/kmsg | grep "avc: denied"  # should be empty
```

Every SELinux denial in a release build indicates either a policy gap (fix the policy) or an access attempt that should be denied (confirm and leave denied). No permissive domains in any release image.

### 12.3 — Outstack-SEC: Minimal Execution Gating LSM

Implement the minimal kernel LSM for execution gating. This is ~300 lines of C code:

```c
// outstack_sec.c — Minimal LSM for Outstack execution gating
#include <linux/lsm_hooks.h>
#include <linux/xattr.h>

static int outstack_bprm_check_security(struct linux_binprm *bprm)
{
    char power_class[32] = {0};
    int ret;
    int current_mode;
    
    // Read power class from executable xattr
    ret = vfs_getxattr(bprm->file->f_path.dentry,
                       "user.outstack.power_class",
                       power_class, sizeof(power_class));
    if (ret < 0)
        return 0;  // No xattr = no restriction
    
    // Read current Outstack mode from kernel state
    current_mode = outstack_get_current_mode();  // reads from atomic kernel variable
    
    // DEFERRED class cannot exec in CRITICAL or EMERGENCY
    if (strcmp(power_class, "deferred") == 0 && 
        current_mode >= OSTK_MODE_CRITICAL)
        return -EACCES;
    
    // OPPORTUNISTIC class cannot exec outside FULL mode
    if (strcmp(power_class, "opportunistic") == 0 &&
        current_mode > OSTK_MODE_FULL)
        return -EACCES;
    
    return 0;
}

static struct security_hook_list outstack_hooks[] = {
    LSM_HOOK_INIT(bprm_check_security, outstack_bprm_check_security),
};

static int __init outstack_sec_init(void)
{
    security_add_hooks(outstack_hooks, ARRAY_SIZE(outstack_hooks), "outstack");
    return 0;
}
security_initcall(outstack_sec_init);
```

The `outstack_get_current_mode()` function reads from a kernel atomic variable that `outstack-powerd` writes to via a `/proc/outstack/mode` interface (written on mode transitions, protected by `CAP_SYS_ADMIN`).

This LSM elevates execution gating from application layer (`telux-exec` wrapper) to kernel layer. From this point forward, execution gating is enforced at every `execve()` call, not only for processes that invoke the wrapper.

### 12.4 — Production Signing Key Ceremony

The production signing key ceremony establishes the key hierarchy for all release builds:

**Key hierarchy:**
- Root signing key: ed25519, generated on air-gapped hardware, stored in HSM
- Release signing key: derived from root, used in signing pipeline, stored in HSM
- Debug signing key: used for development builds only, never appears in production images

The ceremony is performed with two or more team members present. The key generation, public key recording, and HSM import are all witnessed. The ceremony document is signed by all witnesses and stored in `project/key-ceremony-record.md` (private, not in the public repo).

### 12.5 — OTA Update Framework

Babb Cat uses AOSP's Recovery OTA mechanism (not A/B seamless updates — the device is A-only). Update packages are:
- Signed with the production release key
- Verified by TWRP before installation
- Delta updates where possible (full updates for major version changes)
- Available via `update.babb.tel` — a Babb-controlled update server

The OTA check is NOT automatic. Babb Cat does not background-poll for updates. The user checks for updates manually (Settings → System → Babb Cat Updates). This is consistent with the sovereignty doctrine: the device does not initiate network connections the user has not requested.

### 12.6 — Full Test Suite Execution

Execute the complete `project/test-plan.md` test suite:
- All P0 tests: boot, display, encryption, SELinux, low-RAM mode
- All P1 tests: telephony, audio, WiFi, GPS, storage, camera
- All P2 tests: power management, long-term stability, edge cases

Additionally, execute the Telux/Outstack test cases (not yet in `test-plan.md` — add them as part of this phase):
- Island creation, member addition, member revocation
- Ledger write, read, chain view
- Sovereign dashboard accuracy
- Power mode transitions at battery thresholds
- eDRX activation confirmation on carrier network
- Outstack-SEC LSM: confirm DEFERRED class process denied exec in CRITICAL mode

**Phase 12 Outputs:**
- Full KSPP hardening applied to kernel defconfig
- Zero SELinux denials in release build
- Outstack-SEC LSM integrated and execution gating confirmed at kernel level
- Production signing key ceremony completed and documented
- OTA update framework operational
- All P0, P1, P2 tests passing
- Telux/Outstack test cases added to test plan and passing

**Phase 12 Exit Condition:** All tests passing. Zero SELinux denials. Execution gating confirmed at kernel level. Production signing key in HSM.

---

## Phase 13: Field Validation — Zambia Deployment
### *Truth lives in the field, not in the lab*

**Entry condition:** Phase 12 complete. Signed release build available.

**Duration:** 4–8 weeks (field deployment cycles)

No amount of lab testing substitutes for deployment in the actual context. Phase 13 takes Babb Cat to Zambia — or to a sufficiently representative proxy — and runs it against real networks, real users, and real exchange workflows.

### 13.1 — Pilot Device Preparation

Prepare 5–10 pilot devices:
- All devices flashed with signed Babb Cat release build
- Each device goes through the sovereign key ceremony (each user is their own sovereign)
- Users are briefed on: what Babb Cat does differently from stock Android, how the ledger works, how to use Workpads for recording field exchanges, how to query the ledger
- Users do NOT need to understand the Telux architecture — they need to understand the user-facing functions

### 13.2 — Carrier Validation in Zambia

On live Zambia networks, on each of the three carriers, confirm:
- Voice calls: incoming and outgoing
- SMS
- LTE data: APN auto-configured, data session established
- USSD: all three mobile money access codes respond correctly
- STK: all three carrier money menus present and navigable
- VoLTE: document whether carrier provisions custom devices (if not, CSFB baseline confirmed)
- eDRX: confirm modem enters eDRX on each carrier (requires carrier confirmation or RF test equipment)

### 13.3 — Power Measurement in Field Conditions

Field conditions differ from lab conditions: inconsistent LTE signal (causing modem to use more power for signal quality maintenance), more frequent screen-on events, varied temperatures affecting battery capacity.

Measurement protocol:
- 5 devices, 5 days each
- Normal field use: 15-30 minutes screen-on per day, 5-10 phone calls, 20-30 SMS/data events
- Measure: start battery %, end battery %, inferred daily consumption
- Calculate: days per charge at field usage pattern

Compare to: stock Android 11 Go on same device (one device kept on stock for comparison), and to the Phase 7 lab measurements.

### 13.4 — Workpads Field Trial

The primary use case: field workers create job records during work, share via SMS to customers, customers ACK. After one week:
- How many records were created?
- Were any chains left open (no state_commit)?
- Were any records disputed?
- Was the T9 keypad input usable or frustrating?
- Did users understand the ledger concept or ignore it?
- Were there any UX flows that caused confusion?

Findings feed directly into Phase 14 UX refinement.

### 13.5 — Security Incident Simulation

Test Outstack's anomaly detection by simulating scenarios that should trigger alerts:
- Run a CPU-intensive background task to simulate anomalous power consumption → confirm alert logged
- Simulate mode transition during active phone call → confirm call is not interrupted (CRITICAL class)
- Simulate EMERGENCY mode → confirm device remains reachable by phone

**Phase 13 Outputs:**
- Field validation report: carrier confirmation, power measurement results, user feedback
- Workpads field trial report: usage statistics, UX findings
- `project/known-issues.md` updated with field-discovered issues
- `project/regression-log.md` updated
- Security incident simulation results documented

**Phase 13 Exit Condition:** Mobile money confirmed functional on all three Zambia carriers in live deployment. Power measurements confirm 3+ days standby in field conditions. No P0 issues discovered in field. User feedback incorporated into Phase 14 plan.

---

## Phase 14: HOME Platform — Documentation, Upstreaming, and Distribution Foundation
### *A platform is a system other people build on*

**Entry condition:** Phase 13 complete. Field-validated release published.

**Duration:** Ongoing — this phase has no end condition

Phase 14 is not a build phase. It is the phase in which Babb Cat stops being a project and becomes a platform. The work is documentation, upstream contribution, ecosystem development, and the establishment of the foundation for other HOME distributions.

### 14.1 — Complete Technical Documentation

Every document in the cats22-os research corpus was a draft. Phase 14 produces the final, publication-quality versions:

- `hardware/device-profile.md` — updated with all empirically confirmed data from Phases 1–5
- `kernel/kernel-build.md` — complete, reproducible kernel build instructions
- `os/power-management.md` — complete Outstack implementation and measurement documentation
- `os/security-model.md` — complete Telux-SEC, SELinux, and dm-verity documentation
- New: `outstack/architecture.md` — definitive Outstack architecture reference
- New: `telux/island-protocol.md` — definitive Island and sovereignty protocol reference
- New: `telux/ledger-spec.md` — definitive exchange ledger record format and chain protocol reference (extending bitpad-v1 with HOME additions)
- New: `home/distribution-guide.md` — how to create a HOME distribution for a different hardware platform or deployment context

### 14.2 — Upstream Contribution Plan

Every modification to upstream projects is reviewed. For each:

**Kernel patches:**
- KSPP hardening configuration additions that are device-independent → submit to the KSPP project
- Outstack-SEC LSM → submit to LKML as a new LSM with documentation and test suite
- Cover display DTS node → submit to the Device Tree mailing list
- MSM8937 power domain improvements (if any discovered during field validation) → submit to Qualcomm-specific kernel maintainers

**AOSP modifications:**
- Babb Setup Wizard → open source as a standalone project; encourage adoption by other privacy-focused Android distributions
- T9 IME for Zambia languages → submit to AOSP keyboard project and/or AnySoftKeyboard project

**Workpads extensions:**
- HOME bitpad-v1 extensions (templates 0x07–0x0B) → submit to `workpads-standard/` as a ratified HOME extension
- Android Workpads Native app → publish as open source, separate repository

**Timeline:** Upstream submissions begin in month 1 of Phase 14. Track each submission in `project/upstream-contributions.md` with status (submitted, in review, merged, rejected with reason).

### 14.3 — HOME Certification Process

Establish the HOME device certification criteria so that future hardware can be evaluated for HOME compatibility:

**Certification tiers:**

**HOME Compatible:** Device can run a HOME distribution with the specified features functional. No additional certification testing required — it boots, telephony works, SELinux enforcing.

**HOME Certified:** Device passes the full test suite. All P0, P1, P2 tests. Outstack power measurements confirmed. All Telux primitives operational. Ledger and exchange layer functional.

**HOME Sovereign:** HOME Certified + bootloader can be re-locked with a HOME-controlled key (no orange state). This requires UEFI ABL bootloader (Snapdragon 4-series and above, post-2019 chipsets). Babb Cat is HOME Certified, not HOME Sovereign, due to LK bootloader limitation.

Publish the certification criteria as `home/certification.md`. Certification testing is performed by Babb and the results are published publicly for each certified device.

### 14.4 — The Accounting Notation as Open Standard

The HOME extension to bitpad-v1 is the accounting notation. Phase 14 formalizes it:

Submit to a relevant standards body (IETF as an Informational RFC, or W3C as a Community Group Note) a specification document covering:
- HOME record format (CBOR encoding, field definitions, compression)
- Chain protocol (chainRef, state_commit, ACK, amendment, dispute)
- DID integration (source_did, destination_did, sovereign_sig fields)
- HOME template types (0x07–0x0B)
- Implementation guidance

The accounting notation is the most enduring artifact of HOME's architecture. It is the digital clay token. Making it an open standard ensures that it can outlive any single implementation.

### 14.5 — Distribution Guide for Future HOME Expressions

`home/distribution-guide.md` tells the story of how to create a HOME distribution — not Babb Cat specifically, but any HOME distribution for any context:

- How to fork AOSP and establish a HOME manifest overlay
- Which components are mandatory (outstack-powerd, telux-identd, telux-groupd, telux-ledgerd, telux-coverd) and which are optional (HOME launcher, Workpads Native, specific carrier configs)
- How to configure Outstack's power modes for different power source types (battery, mains, RTG, solar)
- How to establish a sovereign key ceremony appropriate for the deployment context
- How to adapt the setup wizard for a different deployment context (different languages, different carriers, different use case)
- How to submit a new device tree and defconfig to the HOME hardware certification process

---

## Appendix A: Phase Summary and Timeline

| Phase | Name | Duration | Entry Condition | Key Output |
|-------|------|----------|----------------|------------|
| 0 | Orientation | 1–2 weeks | This doc read | Repo integrity, gap ownership |
| 1 | Device Intelligence | 2–3 weeks | Phase 0 | DTB decompiled, carrier baseline |
| 2 | Toolchain | 1–2 weeks | Phase 1 | Docker build operational |
| 3 | Bootloader & Recovery | 1–2 weeks | Phase 2 | First custom boot |
| 4 | Kernel Engineering | 3–5 weeks | Phase 3 | All hardware drivers init |
| 5 | Hardware Bring-Up | 2–4 weeks | Phase 4 | Telephony + audio + display |
| 6 | Babb OS Layer | 2–3 weeks | Phase 5 | GMS free, mobile money working |
| 7 | Outstack | 3–4 weeks | Phase 6 | Five-mode system, power improvement |
| 8 | Telux Identity | 3–4 weeks | Phase 7 | DIDs, Islands, sovereign ceremony |
| 9 | Exchange Ledger | 4–5 weeks | Phase 8 | Records, chains, NL query |
| 10 | Workpads Native | 3–4 weeks | Phase 9 | Full Workpads on HOME |
| 11 | HOME Launcher | 3–4 weeks | Phase 10 | Island-centric UX, cover display |
| 12 | Security Hardening | 3–4 weeks | Phase 11 | LSM, full test suite passing |
| 13 | Field Validation | 4–8 weeks | Phase 12 | Zambia confirmed, 3+ day standby |
| 14 | Platform | Ongoing | Phase 13 | Docs, upstream, certification |

**Minimum calendar estimate (no parallelization):** ~40–52 weeks from Phase 0 start to field validation complete.

**With appropriate parallelization** (kernel engineering can overlap with build environment; documentation runs in parallel with all technical phases; carrier testing can begin as soon as Phase 5 is complete): **28–36 weeks** to Phase 13 complete.

---

## Appendix B: Critical Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Bullitt never releases kernel source (GPL violation) | High | Medium | Use CAF + Redmi Go reference — functional equivalent |
| Zambia carrier does not provision VoLTE for custom devices | High | Low | CSFB baseline confirmed; VoLTE enhancement only |
| Cover display controller model incorrect after DTB decompilation | Medium | Medium | fbtft driver supports all likely candidates; fallback: disable for v1 |
| Outstack-SEC LSM rejected by Android kernel maintainers on upstream submission | Medium | Low | Ship as out-of-tree module with clear upstream plan |
| Workpads bitpad extension rejected by workpads-standard maintainers | Low | Low | Fork under HOME namespace; submit pull request |
| Production signing key compromise | Very Low | Critical | Air-gapped ceremony, HSM storage, immediate key rotation plan |
| eDRX not supported on Zambia carriers | Medium | Medium | Fall back to standard DRX; document eDRX as network-dependent |
| Hardware power gating of WiFi causes system instability | Medium | Medium | Gate only in EMERGENCY mode; test thoroughly in Phase 12 |

---

## Appendix C: The Workpads-HOME Ledger Mapping

For developers implementing the exchange layer, this is the translation table between Workpads KaiOS concepts and HOME ledger concepts:

| Workpads KaiOS | HOME Ledger |
|----------------|-------------|
| Record | Ledger entry |
| chainRef | Chain identifier (Island-scoped) |
| state_commit | Chain closure record |
| ACK | Recipient confirmation record |
| Activity Profile (sender) | Device DID (Keymaster-backed) |
| Block Registry contact | Identity Registry entry (DID + name) |
| localStorage | telux-ledgerd SQLite (FBE-encrypted) |
| bitpad-v1 URL | HOME ledger record export format |
| `*778#` USSD result | `payment` record in personal Island |
| Workpads Panel (ArrowLeft) | HOME launcher Island ledger view |
| Personal Panel (ArrowRight) | Quick Note in personal Island |
| Stone/DID identity (unimplemented in KaiOS) | telux-identd (fully implemented in HOME) |
| `generateMarkerUid()` | `telux-identd.sign(deviceDID, payload)` |

The missing piece in the KaiOS implementation — Stone/DID identity, `generateMarkerUid()` with no UI — is exactly what HOME's identity layer provides. Babb Cat completes the Workpads protocol where the KaiOS version left it open.

---

## Appendix D: The Exchange in Practice — A Day in the Life of Babb Cat

**6:00 AM:** Mwila wakes. Opens the flip. HOME launcher shows: Personal Island, NORMAL mode (battery 78%), no new ledger events overnight. Cover display showed battery percentage while phone was closed.

**7:30 AM:** Mwila's supervisor assigns a job via Workpads share link (received as SMS). Babb Cat decodes the bitpad-v1 URL, stores the record in the ledger under the Personal Island, shows: "New record received from [Supervisor DID]. Job: Electrical inspection at Site 4. View chain?"

**8:00 AM:** Mwila opens the flip at the job site. Creates a Workpads record for the inspection — T9 keypad, PADS wizard, 3 minutes. Story field: "Completed visual inspection. Panel board requires replacement." State: Job in progress.

**12:00 PM:** Battery at 35%, Outstack transitions to CONSERVE. Background apps suspended. Mwila doesn't notice — calls still work, Workpads still works, mobile money still works.

**2:00 PM:** Job complete. Mwila generates state_commit from the chain view. Type: Job complete. Shares via SMS to supervisor. Record: `state_commit`, chainRef=`a7K`, island_id=Personal, source_did=Mwila_DID, sovereign_sig=Keymaster_signature.

**3:00 PM:** Customer pays via Airtel Money `*778#`. Babb Cat detects completed USSD session: "Record this ZMW 150 Airtel Money payment in your ledger?" Mwila accepts. Payment record added to ledger. Chain now has: `job` → `state_commit` → `payment`.

**6:00 PM:** Mwila queries the ledger: "Show me all completed jobs this week." Rule-based parser translates to SQL. Response: 3 jobs, total ZMW 390 earned, all chains state_committed, 2 ACKs received. The ledger is the record of the day's work, signed and chain-hashed, stored on a device no external party can read.

**10:00 PM:** Mwila closes the flip. Outstack transitions to CONSERVE. eDRX cycle extends to 20 seconds. Cover display shows time, battery 52%, CONSERVE mode. By morning: battery 44%. The phone lasts 3 days between charges.

*This is what Babb Cat is. A phone that works. A ledger that belongs to its user. A sovereign device.*

---

*This document is the master process reference for the Babb Cat implementation. Every phase produces outputs that are verified before the next phase begins. Every phase has a named owner. Every risk has a mitigation. The work is ordered, the foundation is clear, and the architecture is understood.*

*Build Babb Cat. Prove HOME. Build the clay token.*
