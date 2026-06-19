# TeluxOS × Babb OS: AOSP Prototype Track
## Fusing Telux Architecture with the CAT S22 Flip

*Speculation and Research Document — Track One*
*Prepared: May 31, 2026*

---

## Prologue: Two Projects Discovering They Were One

Babb OS and Telux were conceived in different registers. Babb OS began from a practical question about a locked phone and grew into a production operating system discipline: de-Googled Android for a specific hardware device in a specific deployment context, with careful power management and a commitment to mobile money infrastructure. Telux began from a civilizational question — what does a system for binding commitments across any entity, any distance, any timeframe look like at the OS level — and grew into an architectural doctrine with its own vocabulary: Islands, newgroups, sovereignty, Outstack, the exchange layer.

These are not two projects. They are two resolutions of the same photograph.

Babb OS answers: how do we build a phone that works for its users rather than for its manufacturer's data interests? Telux answers: how do we build an operating system that treats exchange — between humans, machines, and AI — as a native primitive with the same status as process scheduling and memory allocation?

When you hold both questions simultaneously, the CAT S22 Flip stops being a delivery device for a better Android and becomes a prototype for something genuinely novel: a phone that is also a sovereign exchange node, running OS-layer primitives that have no equivalent in any current mobile system.

This document is a speculation and research track describing how to actually build that thing. It is not a design document. It is a map of the territory, with honest assessments of where the paths are clear, where they require engineering invention, and where they require hardware constraints to be respected rather than wished away.

---

## Part I: What Is Being Built

### The Fusion Target

TeluxOS on the CAT S22 Flip is:

**From Babb OS:**
- De-Googled Android (AOSP, no GMS)
- QM215/MSM8937 hardware support (kernel 4.9 CAF, vendor blobs, Zambia carrier configs)
- Power-first design (eDRX, Doze without impediment, GMS elimination dividend)
- Mobile money infrastructure (STK, USSD, carrier APN pre-configuration)
- Reproducible signed build process
- Rugged flip phone form factor awareness

**From Telux:**
- The newgroup primitive (dynamic coalitions of humans, AI entities, services, IoT)
- Islands as sovereign containers (power domain + security namespace + communication space + ledger)
- Outstack power management (five system modes, five process classes, execution gating)
- Exchange ledger (every resource transfer recorded, signed, queryable)
- W3C DID identity for non-human entities
- Natural language query interface to ledger records
- Sovereignty model (hierarchical, grantable, succession-aware)

**From Outstack (as Alpine Linux doctrine applied to Android):**
- Power as a security primitive (not just resource management)
- Hardware power gating as access revocation (unforgeable)
- Power budget per Island (not just per process)
- Execution gating at exec() time based on power + sovereignty state
- Five-mode system (FULL → EMERGENCY) mapped to Android's power stack

The result is a phone whose OS has a coherent doctrine about what runs, when, with what authority, recorded how, queryable by whom — enforced not just by policy but by hardware power state.

### Why the CAT S22 Flip Is the Right Prototype Device

The CAT S22 Flip has properties that make it unusually suited to this fusion:

**Its hardware is well understood.** The QM215 (MSM8937) SoC has a decade of community documentation. The boot chain is fully mapped. The vendor blob inventory is complete. The kernel source (CAF, with Redmi Go reference device) is available. There are no mysteries remaining in the hardware.

**Its form factor is semantically meaningful.** The flip hinge creates a natural physical boundary between active and passive states. Lid closed = Island suspended. Lid open = Island active. The Hall effect sensor generating `SW_LID` kernel events is already there. This gives TeluxOS a hardware-mediated mode transition that maps directly to Outstack's operational modes — not metaphorically, but actually.

**Its power constraints force good architecture.** The 1450mAh battery is small. The QM215's power characteristics are modest. Every watt-hour counts. Telux's power-first doctrine, applied to this hardware, produces measurable improvements. The CAT S22 Flip on stock Android Go with GMS is already better than most comparably-priced phones. TeluxOS on the CAT S22 Flip, eliminating GMS and adding Outstack's intelligent power management, targets genuinely transformative standby life.

**Its target deployment context is appropriate.** The Zambia context for which Babb OS was designed involves irregular charging infrastructure, limited data bandwidth, and field deployment scenarios. These conditions are exactly the ones Telux's Outstack system was designed for in its "Near-term industrial field deployment" vision: field teams, sensor services, limited connectivity, records that must survive transmission across degraded links.

**Its existing work is not discarded.** The cats22-os repository already contains thirty technical documents, a build infrastructure, a vendor blob inventory, a kernel configuration, and a detailed power management analysis. TeluxOS on the CAT S22 Flip does not start over. It inherits everything from Babb OS and adds the Telux layer above it.

---

## Part II: Hardware Baseline

### The QM215 (MSM8937) in Detail

Understanding the hardware constraints is prerequisite to understanding what Telux integration actually looks like on this platform.

**Processing:**
- 4× ARM Cortex-A53 @ 1.4GHz (32-bit instruction set, though A53 supports AArch64 — the device runs 32-bit Android Go)
- 28nm process (2015-era manufacturing node)
- Adreno 308 GPU (low-end, no compute shaders, Vulkan 1.0 only with driver limitations)
- Hexagon 536 DSP (accessible via FastRPC; can run signal processing offloaded from ARM)

**Memory and Storage:**
- 2GB LPDDR3 RAM (tight for any LLM; sufficient for Android Go plus modest native daemons)
- 16GB eMMC 5.1 storage (dynamic partitions: system + vendor + product in 8.5GB super partition)
- f2fs userdata with FBE via Qualcomm ICE (hardware inline crypto engine)

**Power Architecture (this is the critical section for Outstack):**
- RPM (Resource Power Manager): a dedicated microcontroller on the QM215 that manages power rails, clocks, and sleep states *independently of the main ARM cores*. The RPM firmware (`rpm.mbn`) is loaded during boot chain before Linux. It runs independently, always. This is why the modem can receive calls while all ARM cores are in Power Collapse.
- PMIC: Qualcomm PM8937 (Power Management IC). Manages battery charging, power rails for each subsystem, DCDC converters, LDOs. Accessible from kernel via Qualcomm SPMI (System Power Management Interface) bus.
- Qualcomm LPASS (Low Power Audio SubSystem): handles audio offload from ARM, allowing ARM core power collapse during playback.
- TrustZone: ARM TrustZone implementation running QSEE (Qualcomm Secure Execution Environment), housing Keymaster 4.0 for cryptographic operations. This is the hardware root of trust.
- Power Collapse: all four Cortex-A53 cores powered off, only retention RAM active. RPM handles interrupts and modem keep-alive. CPU wakes only on actual events (calls, alarms, kernel timers).

**Modem:**
- MDM9x07 LTE Category 4 modem (integrated in QM215 package)
- Qualcomm QMI (Qualcomm MSM Interface) protocol over shared memory (`/dev/smd*`)
- eDRX capable (negotiated with carrier)
- PSM capable (device-initiated de-registration for IoT use cases)
- Carrier: Airtel/Zamtel/MTN Zambia, with MCFG profiles

**Connectivity:**
- WCNSS subsystem: WiFi (Qualcomm Prima 802.11n), Bluetooth 4.1
- Both controlled by separate firmware blobs; WiFi uses prima (staging driver)

**Security:**
- Keymaster 4.0 in TrustZone (AES-256 FBE, asymmetric operations)
- dm-verity on system/vendor/product partitions (active in orange state)
- SELinux enforcing (AOSP policies + device-specific policies)
- Bootloader: LK (Little Kernel), unlocked to "orange state" — no custom key re-locking possible on this platform generation. This is a known characteristic, not a fixable limitation.

**The 32-Bit Constraint:**
The QM215 runs a 32-bit kernel on 32-bit ARM. AArch64 exists in the processor but is not used (Android Go configuration choice). This has implications:
- Maximum addressable memory per process: 4GB virtual (shared between kernel and userspace)
- No 64-bit binder in default AOSP Go configuration (though `TARGET_USES_64_BIT_BINDER := true` is set — see Babb OS documentation for the CONFIG_ANDROID_BINDER_IPC_32BIT flag discussion)
- Some native libraries compiled for AArch32 only
- Certain modern software toolchains assume 64-bit and require adaptation

For TeluxOS, this means: native daemons must be compiled for armv7-a. Rust and C work fine. C++ works fine. Go works (32-bit ARM is supported). Python works. The 32-bit constraint is a memory overhead inconvenience, not a fundamental blocker.

---

## Part III: Integration Architecture

### Layer Model

TeluxOS on the CAT S22 Flip has five integration layers:

```
┌─────────────────────────────────────────────────────┐
│  LAYER 5: TELUX APPLICATION LAYER                   │
│  newgroup UI, Island management, exchange ledger     │
│  query (natural language), sovereign dashboard       │
├─────────────────────────────────────────────────────┤
│  LAYER 4: TELUX SYSTEM SERVICES                     │
│  outstack-powerd (power management daemon)           │
│  telux-groupd (newgroup/Island manager)              │
│  telux-ledgerd (exchange record service)             │
│  telux-identd (W3C DID identity service)             │
├─────────────────────────────────────────────────────┤
│  LAYER 3: ANDROID FRAMEWORK EXTENSIONS              │
│  SELinux policies for Telux domains                  │
│  Android AccountManager DID integration              │
│  cgroup v1 extensions for power classes              │
│  Custom power management policy hooks                │
├─────────────────────────────────────────────────────┤
│  LAYER 2: BABB OS (DE-GOOGLED ANDROID GO)           │
│  AOSP Android 11, no GMS, Doze unimpeded            │
│  Zambia carrier configs, mobile money, STK/USSD      │
│  dm-verity, FBE, SELinux enforcing                   │
├─────────────────────────────────────────────────────┤
│  LAYER 1: KERNEL + HARDWARE                         │
│  Linux 4.9 CAF (MSM8937), vendor blobs               │
│  RPM firmware, TrustZone/Keymaster 4.0               │
│  QM215 power domains (RPM-managed)                   │
└─────────────────────────────────────────────────────┘
```

Each layer is separately buildable and independently deployable. TeluxOS Phase 1 can ship without Layer 4 or 5. Layer 3 extensions can be added incrementally.

### Layer 3: Android Framework Extensions

**SELinux for Telux Domains:**
Android's SELinux enforcement is the closest available analog to Telux-SEC (the custom Linux Security Module in the full Telux vision). Rather than writing a new LSM — which on kernel 4.9 is difficult and carries high maintenance cost — TeluxOS extends the SELinux policy to create Telux-specific domains.

The approach:
- A new `u:r:telux_island:s0` domain for processes running inside a named Island
- A new `u:r:telux_sovereign:s0` domain for the sovereignty enforcement daemon
- `u:r:telux_ledger:s0` for the exchange ledger service
- New file labels: `u:object_r:telux_island_data:s0` for Island-scoped data directories
- Type transitions: processes in `telux_island` can only read files labeled for their Island and access services in their Island's allow-list

This is not as powerful as a dedicated LSM (SELinux cannot do power-gating; it cannot enforce time-limited access tokens natively), but it provides genuine kernel-enforced boundary control that survives process compromise. A compromised process in `u:r:telux_island:s0` cannot reach files or services outside its Island's SELinux domain, regardless of what the process code says.

The sovereignty enforcement daemon runs in `u:r:telux_sovereign:s0`, which has transition authority to change Island domain assignments. It is the one process that can move another process between Island domains, analogous to how the Sovereign controls group membership in the Telux model.

**cgroup v1 Power Classes (kernel 4.9 constraint):**
The full Outstack design targets cgroups v2, which provides unified hierarchy and better accounting. Linux 4.9 has cgroups v1, which is less elegant but functional. TeluxOS uses cgroups v1 as follows:

- `/sys/fs/cgroup/cpu/telux/` hierarchy for CPU scheduling
- `/sys/fs/cgroup/memory/telux/` hierarchy for memory limits
- Five sub-groups mapping to Outstack process classes: `critical/`, `interactive/`, `background/`, `deferred/`, `opportunistic/`
- The `outstack-powerd` daemon maintains process-to-class assignments and adjusts cgroup membership as system power mode changes
- In EMERGENCY mode: only `critical/` group processes receive CPU time (cpuset 1 core, cpu.shares 1024 vs. 64 for all others)
- In CONSERVE mode: `deferred/` and `opportunistic/` groups have their CPU quota suspended entirely

This is less elegant than the custom scheduler in the full Outstack vision, but it is kernel-supported, stable on 4.9, and achieves the same practical effect: certain process classes are starved of CPU when the system needs to conserve power.

**Android Power Management Integration:**
Android already has a layered power management system: Doze, App Standby Buckets, and the JobScheduler/WorkManager constraint system. TeluxOS maps Outstack modes to Android's existing system rather than replacing it:

| Outstack Mode | Android Doze State | cgroup Change | Modem eDRX |
|---------------|-------------------|---------------|------------|
| FULL | Interactive | All classes active | Disabled |
| NORMAL | Light Doze permitted | All classes active | 5.12s cycle |
| CONSERVE | Doze active | Deferred/opportunistic suspended | 10.24s cycle |
| CRITICAL | Deep Doze enforced | Only critical + interactive | 20.48s cycle |
| EMERGENCY | All background killed | Critical only, 1 core | 40.96s cycle |

The `outstack-powerd` daemon reads battery level from `/sys/class/power_supply/battery/capacity` (or the BMS HAL), current power mode from its own state machine, and writes to cgroup hierarchies and calls the Android `PowerManager` API via JNI or AIDL to align Doze state with Outstack mode.

### Layer 4: Telux System Services

All four Layer 4 services are implemented as privileged native daemons plus Android AIDL interfaces. They are built into the system image, run as separate processes with dedicated UIDs, and communicate with Android framework and applications via Binder (AIDL).

**outstack-powerd:**
The power management daemon. Replaces the conceptual "Outstack power governor" from the Outstack architecture with an Android-native implementation.

Architecture:
- Native daemon written in C or Rust, started by Android `init` from `/system/etc/init/outstack-powerd.rc`
- Reads: `/sys/class/power_supply/battery/capacity`, SPMI-exposed PMIC registers (via sysfs), cgroup current usage, Island power budget allocations from `/data/outstack/budgets/`
- Writes: cgroup cpu.shares and cpu.cfs_quota_us, cgroup memory limits, Android PowerManager (via socket IPC with a Java-side system service wrapper)
- State machine: implements the five-mode model with hysteresis (avoids mode flapping at boundaries)
- Power budget enforcement: each Island has an allocated budget in milliwatts. The daemon tracks actual draw per Island using cgroup accounting and triggers budget violation events when Islands exceed their allocation.
- BABB integration: if the Babb hardware dongle (USB-C security key referenced in Outstack documentation) is connected, it elevates the connected Island to FULL power mode and authenticates the sovereign.

**telux-groupd:**
The newgroup and Island management daemon.

Architecture:
- Native daemon (Rust preferred for memory safety, armv7-a target)
- Manages the live registry of Islands and their member groups
- Island data stored in `/data/telux/islands/` (SELinux-labeled, FBE-encrypted)
- For each Island: sovereign key (ed25519 public key), member list (W3C DIDs), power budget allocation, group message bus endpoint, logging policy
- AIDL interface: `ITeluxGroupService` providing `createIsland()`, `joinGroup()`, `grantMembership()`, `revokeGroup()`, `getIslandStatus()`, `setSovereign()`
- Sovereignty enforcement: `grantMembership()` and related mutation operations check that the caller holds the sovereign key for the target Island (verified via TrustZone-backed signature verification through Keymaster 4.0)
- Android User integration: optionally maps Islands to Android Users (for the strongest process isolation available on Android), or uses SELinux domain isolation only for lighter-weight Islands

**telux-ledgerd:**
The exchange ledger service. Records every resource transfer between Island members.

Architecture:
- Native daemon + SQLite database at `/data/telux/ledger/ledger.db` (f2fs + FBE encrypted at rest)
- Record format: CBOR-encoded (as specified in Telux documentation: 128-512 bit range for normal records, TLV binary for low-bit transmission)
- Each record: timestamp, source entity DID, destination entity DID, resource type, quantity, authorization proof (sovereign signature or delegated capability token), record hash, chain hash (linking to previous record from same Island)
- Records are write-once: no UPDATE or DELETE operations on the ledger table. Corrections are new records that reference and supersede prior records.
- ed25519 signing: record hash signed by the Sovereign key, stored in Keymaster 4.0. The private key never leaves TrustZone.
- AIDL interface: `ITeluxLedgerService` providing `recordTransfer()`, `queryRecords()`, `exportIslandLedger()`, `verifyRecord()`
- The `queryRecords()` method accepts structured queries (not natural language; that layer is above). Returns only records the calling DID is authorized to see, enforced by Island visibility policy stored in `telux-groupd`.
- Transmission: ledger records can be exported as CBOR blobs signed by Island sovereign key, suitable for transmission over SMS (long USSD messages), data link, or QR code exchange. This is the "degraded link" transmission capability described in Telux documentation.

**telux-identd:**
The W3C DID identity service.

Architecture:
- Implements `did:key` method as the baseline (self-describing, no network required, generates DID from ed25519 public key)
- Optionally supports `did:web` method when network available (anchors DID document at a user-controlled domain)
- DID documents stored in `/data/telux/identity/` with AIDL access via `ITeluxIdentityService`
- Android AccountManager integration: Telux DIDs are registered as AccountManager accounts with a custom account type (`tel.babb.telux.did`). This allows Android's account picker UI to present Telux DID identities alongside (or instead of) Google accounts.
- Device DID: the device itself has a DID, generated from its TrustZone-backed device key, bound to the Keymaster attestation certificate chain. This is the device's identity in any Island it joins.
- Key operations: all key material for DIDs ultimately backed by Keymaster 4.0 in TrustZone. Signing operations go through the HAL. Private key material never exposed in userspace.

### The newgroup Primitive on Android

In the full Telux architecture, newgroup is a kernel primitive. On AOSP Android with kernel 4.9, it must be implemented in the system service layer rather than the kernel. This is a meaningful architectural compromise, but it is not a fatal one.

A newgroup on TeluxOS Android is:
1. A named group registered in `telux-groupd`
2. A set of member DIDs with capability tokens (what each member can do in the group)
3. A shared message bus (an Android `LocalSocket` endpoint or a D-Bus style multiplexed channel provided by `telux-groupd`)
4. A ledger scope (all exchanges within the group recorded by `telux-ledgerd`)
5. A power budget allocation (managed by `outstack-powerd`)
6. A sovereign (the DID that controls membership)

Member types map to Android entities:
- **Human members**: Android user + Telux DID. Authenticated via device PIN (which unlocks the TrustZone-backed DID private key via Keymaster).
- **AI entities**: Android service with a DID, or a remote inference endpoint represented by a local proxy process with its own DID.
- **System services**: privileged APKs or native daemons with assigned DIDs.
- **Physical devices**: external hardware authenticated via Babb USB-C hardware token, or via Bluetooth LE + FIDO2.
- **Commercial APIs**: represented by broker processes (local proxy with outbound capability token, expires after transaction).

The sovereignty check happens in `telux-groupd`: mutation operations on a group require a Keymaster-signed request from the sovereign DID. This means the sovereign never sends their private key over any channel — they send a signed capability grant or membership change request, and `telux-groupd` verifies the signature via `telux-identd`.

### Islands on Android

Islands map most closely to Android Users (separate process space, separate storage, separate application data), but Android Users are a coarse primitive and each device supports at most ~8 users with significant overhead.

TeluxOS provides two Island implementation modes:

**Mode 1: Android User Islands** (high isolation, ≤4 Islands per device)
- Each Island is a separate Android User
- Full process isolation: Island A cannot call Island B's services through normal Android mechanisms
- Separate encrypted userdata per Island
- Overhead: ~200MB RAM per Island (separate Android runtime)
- Best for: Islands with fundamentally different trust domains (personal Island vs. work Island vs. shared family Island)

**Mode 2: SELinux Domain Islands** (lightweight, many Islands per device)
- Each Island is a SELinux domain (`u:r:telux_island_N:s0`)
- Process isolation enforced by SELinux type enforcement
- Shared Android runtime
- Overhead: ~5MB per Island (just the `telux-groupd` registry entry)
- Best for: functional Islands (a project Island, a transaction Island, an AI collaboration Island)

In practice, a device deployment likely uses one Android User Island as the "primary work context" and multiple SELinux Domain Islands for individual projects and transactions.

---

## Part IV: Kernel Layer

### What Kernel 4.9 Permits

The MSM8937 runs Linux 4.9.227 (Qualcomm CAF). This is a 2017-era kernel on Long-Term Support. Its capabilities for TeluxOS integration:

**Available:** cgroups v1 (cpu, memory, blkio, freezer), SELinux (full policy enforcement), namespaces (network, mount, PID — not user namespaces in 4.9 Go configs), IMA (Integrity Measurement Architecture, for extending the boot-time measurement to runtime), Keymaster 4.0 integration, Qualcomm RPM interface, SPMI for PMIC access.

**Not available:** cgroups v2 (added in 4.5, not enabled in Go configs), user namespaces (security concerns in Android Go), io_uring (5.1+), BPF LSM (5.7+), Landlock (5.13+), the exec-time credential hook used in full Outstack execution gating.

**Exec-time execution gating:**
The full Outstack vision intercepts `execve()` at kernel level to check power budget before allowing a process to start. On kernel 4.9, this requires either an LSM hook or a ptrace mechanism. The ptrace approach is too slow and fragile. Writing a custom LSM for kernel 4.9 is possible but carries significant ongoing maintenance burden.

**Practical approach for exec gating on 4.9:** Use the `outstack-powerd` daemon as a pre-exec broker. Applications and services in the Telux stack are launched through a wrapper (`telux-exec`) rather than directly. `telux-exec` is a small native binary that: reads the target executable's power class from `/data/telux/exec_policy/`, queries `outstack-powerd` via socket for go/no-go, and either `exec()`s the target or returns `-EPOWER` (a synthetic error that `telux-groupd` translates into an "execution deferred" event visible in the Island status).

For system-critical processes and vendor HAL services (which are started directly by Android `init`), execution gating is not practical. The TeluxOS model acknowledges this: hardware-layer services are outside the Telux Island model just as hardware drivers are outside the Telux exchange layer. Only applications and Telux-aware services participate in execution gating.

### SELinux Extensions

The device-specific SELinux policy (in `device/babb/cats22flip/sepolicy/`) is extended with:

```
# Telux-SEC policy additions
type telux_island, domain;
type telux_sovereign, domain;
type telux_ledger, domain;
type telux_identity, domain;
type telux_island_data, file_type, data_file_type;
type telux_ledger_data, file_type, data_file_type;

# Sovereign can modify island membership
allow telux_sovereign telux_island:process { transition };
allow telux_sovereign telux_island_data:file { rw_file_perms };

# Islands are isolated from each other
neverallow telux_island telux_island:{ file dir } { read write };

# Ledger is append-only from Island members
allow telux_island telux_ledger:unix_stream_socket { connectto };
allow telux_ledger telux_ledger_data:file { create append read };
neverallow telux_ledger telux_ledger_data:file { write };  # write but not overwrite
```

The `neverallow` rules are compiled into the policy and enforced at compile time by Android's SELinux build tools. Any policy that tries to grant cross-Island file access will fail to compile. This is a meaningful guarantee.

---

## Part V: Build Architecture

### Build Layers

TeluxOS is assembled as a set of additions on top of Babb OS, which is itself built on AOSP. The build system (Android's Soong/Blueprint) accommodates this through device tree overlays and explicit package lists.

**Additional packages in device.mk:**
```makefile
# Telux system services
PRODUCT_PACKAGES += \
    outstack-powerd \
    telux-groupd \
    telux-ledgerd \
    telux-identd \
    telux-exec \
    TeluxIslandManager \
    TeluxLedgerApp

# Telux system configs
PRODUCT_COPY_FILES += \
    device/babb/cats22flip/telux/outstack-powerd.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/outstack-powerd.rc \
    device/babb/cats22flip/telux/power_budgets.conf:$(TARGET_COPY_OUT_DATA)/telux/power_budgets.conf.default \
    device/babb/cats22flip/telux/island_policies.conf:$(TARGET_COPY_OUT_DATA)/telux/island_policies.conf.default
```

**Native daemon build (outstack-powerd/Android.bp):**
```soong
cc_binary {
    name: "outstack-powerd",
    srcs: ["src/*.c"],
    shared_libs: ["libbinder", "libhidlbase", "libutils"],
    static_libs: ["libcutils"],
    init_rc: ["outstack-powerd.rc"],
    cflags: ["-DANDROID", "-march=armv7-a"],
}
```

### Signing and Trust

TeluxOS inherits Babb OS's signing model: Android Verified Boot with a Babb-controlled key pair, dm-verity on all system partitions. Because the QM215 platform cannot re-lock the bootloader with a custom key, this remains "orange state" AVB — dm-verity active, FBE active, but no hardware-attested boot guarantee to third parties.

The Telux signing layer adds: all Island sovereign operations are signed with ed25519 keys backed by Keymaster 4.0. These are not related to the Android AVB signing. They provide application-layer cryptographic guarantees on top of the existing hardware security baseline.

---

## Part VI: The Flip Form Factor as Telux Interface

The CAT S22 Flip's physical hinge is more than a convenience feature. In TeluxOS, it becomes a hardware-mediated mode interface.

**Lid State → Outstack Mode Transition:**

```
Lid OPEN  → FULL or NORMAL mode (as battery permits)
            Active Island(s) receive UI focus
            External display blank
            
Lid CLOSED → CONSERVE mode minimum
             Active groups suspended (not killed — suspended)
             External display shows Island status summary
             Modem enters 20.48s eDRX cycle
             CPU cores 2-3 parked
```

The Hall effect sensor generating `SW_LID` events in the kernel input subsystem is already wired to `PhoneWindowManager`. TeluxOS adds a policy hook: `outstack-powerd` subscribes to lid state change events via the Android `SensorManager` or directly via `getevent` on the input device, and triggers mode transitions accordingly.

**Physical Keypad as Sovereign Interface:**

The T9 physical keypad has a D-pad and call/end keys. TeluxOS assigns the call key a secondary Telux function: long-press call key with lid closed = sovereign authentication prompt on external display (if external display is functional) or on primary display at next lid open. This is the physical analog of a sovereign key ceremony.

**External Display as Island Status:**

The 1.44" SPI-attached secondary display — currently deferred in the cats22-os bring-up plan pending DTS decompilation — is ideal as an Island status surface. When TeluxOS is active and the lid is closed, the external display shows:

- Active Island name and sovereign status
- Current Outstack power mode (icon)
- Battery state
- Pending ledger events requiring acknowledgment
- Number of active group members

Implementing this requires completing the secondary display bring-up work (decompile stock DTB → identify SPI controller → enable driver → configure DTS node). The TeluxOS implementation adds a small native process `telux-coverd` that renders to the secondary display framebuffer, subscribing to Island state events from `telux-groupd` and power events from `outstack-powerd`.

---

## Part VII: Natural Language Query

The exchange ledger accumulates records. The primary user interface for querying them is natural language.

**The 2GB RAM Constraint:**

Running a large language model locally on 2GB RAM is not feasible. The smallest capable open models (Gemma 2B 4-bit quantized) require approximately 1.5GB in practice — leaving insufficient space for the Android runtime and system processes. Three approaches:

**Approach A: Rule-based NL query parser (on-device, no LLM)**
A domain-specific parser that recognizes a constrained vocabulary of queries:
- "What did [entity] send to [entity] [time period]?"
- "Show all transfers from [entity] last [time unit]"
- "Who authorized the [resource type] transfer?"
- "What is [entity]'s power budget consumption this [time unit]?"

This handles 80% of the practical query surface with zero RAM overhead beyond the parser itself (~2MB). It is the recommended baseline implementation.

**Approach B: On-device quantized tiny model (on-device, <512MB)**
Phi-2 or Phi-3-mini in 4-bit quantization can run in 512MB-700MB on ARM. This is plausible on the CAT S22 Flip if Babb OS's memory optimization is pushed further (reduced heap limits, aggressive background process limits). Query quality significantly better than rule-based parser. Boot-time load of model adds ~5 seconds to first query. Power cost of inference is ~400mW for 2-3 seconds per query.

**Approach C: Cloud-assisted query with on-device privacy filter (hybrid)**
The query is sent to a Babb-controlled server running a full model. The on-device component redacts personally identifying information from the query before transmission, interprets the response, and enforces that the response only contains data the caller's Island scope permits. This gives full LLM quality with on-device privacy guarantees. Requires data connection.

**Recommended implementation path:** Start with Approach A. Add Approach B as an optional "rich query" mode that the user or Island administrator enables. Approach C for organizations that operate a Babb server.

---

## Part VIII: Honest Limitations

This section documents what TeluxOS on AOSP Android does not achieve versus the full Telux vision, because honesty about limitations is prerequisite to using this as a prototype productively.

**Kernel-level execution gating:** Not achievable on kernel 4.9 without a custom LSM. The `telux-exec` wrapper provides application-layer enforcement only. A process that bypasses `telux-exec` (a vendor process, a GMS remnant if one leaked through, a malicious APK) can bypass execution gating. However: with GMS removed and SELinux enforcing, the attack surface for this bypass is minimal in the Babb OS deployment context.

**Hardware power gating as access revocation:** The full Outstack vision allows a peripheral (WiFi, modem, camera) to be physically power-gated as a security revocation. The QM215 RPM manages hardware power rails. The kernel does expose PMIC control through the SPMI sysfs interface, and specific power domains can be modified. However, this is deeply hardware-specific and risks system instability if done incorrectly. On the CAT S22 Flip, power-gating the WiFi chipset (WCNSS) is achievable via SPMI. Power-gating the modem is more dangerous (requires modem firmware cooperation). This is a research item, not a shipping feature for the prototype.

**Islands as strong process isolation:** Android Users provide strong isolation. SELinux domain isolation is real but not as strong. A kernel vulnerability that bypasses SELinux (which does exist, historically, in kernel 4.9) can cross Island boundaries. This is acceptable for a prototype; it is not acceptable for production security claims.

**Sovereignty succession:** The accounting notation and sovereignty succession rules are documented as unresolved in the Telux architecture. TeluxOS Android inherits this gap. For the prototype: the sovereign is whoever holds the device PIN that unlocks the TrustZone-backed ed25519 key. Multi-sovereign quorum is not implemented. Succession is "reset Island if sovereign key is lost."

**The accounting notation:** Still unresolved upstream. TeluxOS Android's ledger records use a generic CBOR schema. When the accounting notation is specified, the ledger record format will need to be versioned and updated.

---

## Part IX: Implementation Roadmap

### Phase 1 — Babb OS Stabilization (Current Track)
Complete the existing cats22-os research plan through all phases. Establish a working, reproducible Babb OS build with confirmed telephony on Zambia carriers. This is the foundation that everything else stands on.

Deliverables: Bootable signed image, confirmed call/SMS/STK/USSD on Airtel/Zamtel/MTN, eDRX confirmed, power budget documented, regression test suite established.

Timeline: Defined by cats22-os research plan.

### Phase 2 — Outstack Power Daemon
Implement `outstack-powerd` as a standalone native daemon. Test independently of all Telux components. Verify five-mode state machine, cgroup assignments, and battery threshold behavior before any Island or ledger code exists.

Deliverables: `outstack-powerd` binary, RC init file, cgroup hierarchy configuration, documented power measurements for each mode showing measurable battery life improvement.

Timeline: 4-6 weeks of implementation after Phase 1.

### Phase 3 — Telux Identity and Group Primitives
Implement `telux-identd` (DID generation and storage) and `telux-groupd` (Island and group registry). At this phase, Islands are created and destroyed but have no enforcement or ledger.

Deliverables: Working `telux-identd` that generates `did:key` identities backed by Keymaster 4.0. Working `telux-groupd` that creates Islands with sovereign keys and member lists. AIDL interfaces callable from an Android test app. SELinux domain extensions for Island types.

Timeline: 6-8 weeks after Phase 2.

### Phase 4 — Exchange Ledger
Implement `telux-ledgerd` with SQLite backend, CBOR record format, ed25519 signing, and chain-hash linking. Build the rule-based NL query parser.

Deliverables: Records written to `telux-ledgerd` from a test scenario (simulated two-member Island exchanging resources). Records queryable via structured and rule-based NL interface. Records exportable as signed CBOR blob.

Timeline: 6-8 weeks after Phase 3.

### Phase 5 — Cover Display and Hardware Integration
Complete the secondary display bring-up (DTS decompilation, driver configuration, `telux-coverd` surface renderer). Implement lid state → Outstack mode transitions. Implement long-press sovereign authentication flow.

Deliverables: External display showing Island status when lid is closed. Mode transitions on lid events. Demonstrable sovereign key ceremony on physical device.

Timeline: 4-6 weeks after Phase 4.

### Phase 6 — Field Demonstration
Deploy a two-device TeluxOS prototype in a simulated field scenario: two humans (each with a device), one AI member (a cloud endpoint with its own DID), one sensor service (a local BLE sensor with a device DID). Create an Island, conduct exchanges, query the ledger, demonstrate sovereignty revocation.

Deliverables: Working demonstration. Documented power measurements. Architecture review against full Telux design to identify gaps and next steps.

---

## Coda: What This Prototype Proves

A working TeluxOS prototype on the CAT S22 Flip would not prove that the full Telux architecture is complete. It would prove something more specific and more immediately valuable: that the core concepts are implementable with existing hardware and existing open-source infrastructure, that the power management model produces measurable benefits, that sovereignty-enforced exchange records are cryptographically sound at the device layer, and that a flip phone form factor is a natural host for this kind of ambient, low-profile sovereign exchange node.

The prototype is not the product. It is the proof that the product is possible.

The accounting notation remains unresolved. Sovereignty succession remains unresolved. Cross-Island federation remains unresolved. The interstellar record format is not tested on any device. None of this matters yet. What matters is that the first Island is created, the first sovereign key is generated in TrustZone, the first exchange is recorded to a chain-hash ledger, the first query returns the correct signed result, and the flip phone's lid closes and the device drops gracefully into CONSERVE mode.

That is what a prototype proves. Everything else follows from there.

---

*See also: `TeluxOS-OpenHardware-Prototype.md` for the complementary track — a fresh hardware flip phone designed from the ground up to host Telux/Outstack as a native Linux system.*
