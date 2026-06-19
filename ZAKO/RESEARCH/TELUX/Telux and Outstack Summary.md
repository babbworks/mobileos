# Detailed Summary: Telux & Outstack Folders

**Date:** 2026-05-31  
**Purpose:** Investigative summary to support continued research and document generation

---

## TELUX

### Origin & Identity
Telux is a proposed operating system (or OS layer) whose byline is **"System for Group Exchange."** Its logo is modeled on ancient Sumerian clay tokens — the first known formal value-representation system, predating writing. This symbolism is intentional and ambitious: Telux is explicitly designed to operate across **millennia-scale timeframes**, as humanity expands through space.

### The Core Concept: newgroup
Telux evolves the native Linux concept of `group` into something dramatically broader. A **newgroup** (the proposed primitive) can contain any combination of:
- Human users (local or remote)
- Local system services
- Commercial APIs
- AI models / non-human entities
- IoT-connected physical systems (robots, vehicles, rovers)

These groups are dynamic — they can be permanent or temporary — and represent **coalesced organizations** with processing priorities relative to other groups.

### Islands / Isles
Each group belongs to a **place** called an **Island** (or Isle). Islands are the primary security boundary. The concept reinforces extreme isolation:
- Every island must declare who or what is **Sovereign** over it
- Sovereignty is hierarchical and grantable
- The sovereign controls group lifecycle: the "birth" and "death" of groups and members
- "Death" of a member simply means the end of their persistent presence on the island

### The Three-Layer Architecture (Outstack in Telux context)
In the original Telux framing, **Outstack** is the primary system service — a daemon (or potentially deeper kernel integration) — named after the Outstack rock outcrop, northernmost land in the UK: uninhabited, exposed, foundational. It operates in three strata:

| Layer | Name | Visibility |
|-------|------|-----------|
| 1 | **Visible** | Group chat interface, query API, member-visible logs, exchange records |
| 2 | **Submerged** | Outstack daemon, sovereignty enforcer, permissioned logs, power governor |
| 3 | **Bedrock** | LSM module (Telux-SEC), immutable audit trail, hardware-isolated key storage (TPM/HSM) |

### Logging & Exchange Transparency
- Logging is tiered to match the three-layer visibility model
- **Group life should feel like a group chat** — two services communicating should be as easy to inspect as a text thread
- What any entity provides another, and whether obligations were met, must be easily queryable
- A **shorthand accounting notation** (developed but not yet documented in these files) underpins exchange tracking, to be adapted for data, power, token flows between entities

### Technical Analysis (From Conversation Record)
The file documents an AI's analysis of the Telux architecture. Key findings:

**What native Linux groups lack** (the "gap table"):
- Dynamic membership, remote entity membership, AI/non-human identities, temporary groups, hierarchical sovereignty, inter-entity obligation tracking, power budget per group, group-scoped IPC/"chat"

**Three implementation tiers proposed:**
1. **Userspace only** (NSS module, PAM, D-Bus/custom IPC, cgroups v2) — portable but bypassable by root
2. **Userspace + LSM** (custom Linux Security Module) — kernel-enforced, harder to bypass, recommended starting point
3. **Deep kernel** (new syscalls, scheduler integration, network stack) — maximum enforcement, heavy maintenance

**Recommended approach:** Outstack as a **privileged daemon + custom LSM** providing bedrock enforcement.

**Base system options considered:**
- Alpine Linux (current preference)
- Linux From Scratch, Buildroot, bare kernel + custom init
- **Recommended for longevity:** vendored fork model — fork LTS kernel, vendor musl libc, own the stack but selectively track upstream security fixes

**Low-power & low-bit record design:**
- Kernel: tickless operation (NO_HZ_FULL), cpufreq governors
- Per-group power budgets via cgroups v2
- Record formats compared: Binary TLV (64-256 bits), CBOR (128-512 bits), custom fixed-width (32-128 bits), Huffman-coded
- For interstellar: Reed-Solomon/LDPC forward error correction, store-and-forward, hash chains, idempotent records

**Open architectural questions (unresolved):**
1. Identity model for non-human entities (certificates? tokens? new primitive?)
2. Sovereignty inheritance / revocation rules
3. Bedrock access — who can reach the bottom layer?
4. Cross-island communication between physical servers
5. Time model for interstellar latency (Lamport timestamps? Vector clocks?)
6. The accounting notation (not yet shared)
7. Failure modes — what happens when a sovereign dies/disconnects?
8. Bootstrap — who is sovereign over the first island?

**Analogues cited:** Plan 9 (namespace isolation), Genode (capability-based sovereignty), CICS (COBOL transaction monitor), Erlang/OTP (actor model)

---

## OUTSTACK FOLDER

### Document 1: `Outstack System.md` (Jan 26, 2025 — Core Design Doc)
The more recent, structured Outstack design document reimagines Outstack as a **standalone Alpine Linux derivative OS**, stripped of the Telux group-exchange framing and focused purely on the dual pillars of **security and power management as unified resource control**.

**Core tenets:**
1. Default deny — nothing runs, nothing has power, nothing has access unless explicitly granted
2. Hierarchical isolation — CPU, memory, network, and power as independent containment boundaries
3. Verifiable state — at any moment the system can attest what's running and what's consuming power
4. Graceful degradation — security incidents and power exhaustion trigger controlled shutdowns, not crashes

**Kernel strategy:** Alpine's kernel source + layered hardened config (no fork, no patches). KSPP essentials: stack protector, FORTIFY_SOURCE, hardened usercopy, SLAB freelist hardening, shuffle page allocator, init-on-alloc/free, devmem/devkmem/kexec disabled. Per-board defconfigs (RPi4, iMX8, generic x86).

**Security layers (boot to runtime):**
- Layer 1: Secure boot chain (TPM/eFuse → U-Boot/UEFI → dm-verity kernel)
- Layer 2: Runtime integrity (dm-verity read-only root, IMA/EVM, mutable /var only)
- Layer 3: MAC (AppArmor for process confinement + Landlock for self-sandboxing)
- Layer 4: Network (nftables default-deny egress, WireGuard for external comms, no listeners)

**Power management (`outstack-powerd`):**
- Reads power budgets from `/etc/outstack/power.conf`
- Monitors via powercap/RAPL, INA sensors, SoC PMICs
- Power domains with budgets, governors, violation actions
- Sleep state coordination: peripheral gating before suspend, explicit wake source whitelisting
- **Power + Security crossover:** power anomalies (unexpected draw) trigger security alerts; compromised peripherals can be **power-killed**, not just software-disabled; power state included in attestation reports

**Package tiers:**

| Tier | Source | Policy |
|------|--------|--------|
| Passthrough | Alpine direct | Trust Alpine, auto-update |
| Rebuilt | Alpine APKBUILD, our flags | Hardened CFLAGS/LDFLAGS, audit |
| Custom | Our APKBUILDs | Full control (outstack-init, outstack-powerd) |

**Image system:** A/B rootfs (dm-verity protected slots) + recovery partition + encrypted data partition. Signed OTA updates with automatic rollback. Profile-based builds: minimal, iot-sensor, gateway.

---

### Document 2: `Outstack Jan 23 aft 1.md` (Earlier, More Visionary)
The earlier, more expansive Outstack vision — describes a **custom kernel** with power as a native scheduling dimension. The most technically ambitious document in the corpus.

**Five system operating modes:**

| Mode | Trigger | Behavior |
|------|---------|---------|
| FULL | External power / >80% battery | Unrestricted |
| NORMAL | 60-80% | Normal operation |
| CONSERVE | 20-60% | Background limited |
| CRITICAL | 5-20% | Critical tasks only |
| EMERGENCY | <5% | Survival mode |

**Five process power classes:**
- OSTK_CRITICAL, OSTK_INTERACTIVE, OSTK_BACKGROUND, OSTK_DEFERRED, OSTK_OPPORTUNISTIC

**Power source model types:** RTG (aerospace radioactive decay), BATTERY, SOLAR, FUEL_CELL, EXTERNAL — with degradation tracking, cycle count, health %, and predictive runtime calculations.

**Custom scheduler (`outstack_sched_class`):**
- Integrates with Linux's `fair_sched_class`
- In EMERGENCY: only CRITICAL tasks run
- In CRITICAL: only CRITICAL + INTERACTIVE
- Score-based selection combining class priority, power budget, power priority, fairness (time since last run)
- Penalizes high-power tasks in CONSERVE mode

**Execution gate at exec():**
- Intercepts every `execve()` call
- Looks up policy for executable
- Can DENY (-EPOWER), DEFER, or ALLOW based on current power mode
- Critical tasks can trigger `outstack_shed_power_for_critical()` to displace others
- Power context set on new process after successful exec

**Modified OpenRC init:** Reads battery at boot to select initial mode; starts only services appropriate for that mode; each service has MIN_MODE and POWER_CLASS settings.

**Userspace tools:** `ostk-profile` (mission modes), `ostk-budget`, `ostk-defer`, `ostk-monitor`, `ostk-policy`

**Mission profiles (examples):** inspection (8h runtime, camera INTERACTIVE), emergency (24h runtime, minimal services), survey (12h, GPS BACKGROUND)

**BABB integration:** USB-C security dongle detected by Outstack as both a power source identifier and security key; its presence can unlock secure storage and elevate user to OPPORTUNISTIC power class. BABB could itself run Outstack-aware firmware for coordinated power management.

**Claimed historical significance:**
1. First Linux kernel with power as a scheduling dimension
2. Execution gating at kernel level
3. Autonomous power management
4. Aerospace principles for terrestrial use
5. Industrial focus — professional/industrial portable market
6. Predictive, not reactive

---

### Documents 3–4: `Overview of Project Options.md` / `embedded_system_approaches.md` (Duplicate)
Comparative analysis of five architectural approaches:

| Approach | Idle Power | Security | Complexity |
|----------|-----------|---------|-----------|
| Outstack (Linux) | 100-500mW | Software | Medium |
| Microkernel (seL4) | 10-100mW | Strong | High |
| Bare-Metal RTOS | 1-50mW | Weak | Low |
| Async Event Loop (Rust/Embassy) | 10-100µW | Memory-safe | Medium |
| TrustZone/TEE | 100-500mW | Hardware | Very High |

**Recommendation for tradesmen/industrial portable tools:**
- Primary: Bare-Metal RTOS + Async I/O
- Secondary: Microkernel for high-security models
- **Explicitly NOT recommended (yet): Linux/Outstack** — too much overhead for handheld battery-powered tools; better for stationary gateways or mains-powered hubs

**Hybrid patterns noted:**
- Microkernel + TrustZone (automotive)
- RTOS + Async Runtime (FreeRTOS + custom async I/O)
- Linux + Dedicated Power Core (like Apple T2)

---

### Document 5: `state_machine_examples.md`
Detailed implementation of five state machines for industrial handheld devices targeting STM32F411 (ARM Cortex-M4):

1. **Device Power State Machine** — OFF → BOOT → ACTIVE → IDLE → STANDBY → DEEP_SLEEP → LOW_BATTERY
2. **Sensor Reading State Machine** — OFF → WARMUP → MEASURING → VALID/ERROR → IDLE (configurable rates: 10Hz, 1Hz, 0.1Hz, 1/60Hz)
3. **Display State Machine** — OFF → INIT → ACTIVE → DIM → STANDBY
4. **Button/Input State Machine** — debounce, short press, long press, double-click detection
5. **Battery Management State Machine** — UNKNOWN → NORMAL → CHARGING → LOW → CRITICAL → SHUTDOWN

---

### Document 6: `state_machine_register_mapping.md`
Brief overview bridging state machine concepts to STM32 memory-mapped register control on STM32F411 (ARM Cortex-M4, 100MHz, 128KB RAM, 512KB Flash). Demonstrates that every state transition maps to specific register writes. References full content that was apparently planned but not generated.

---

### Document 7: `embedded_field_device_architecture.md`
Comprehensive synthesis — the fullest technical analysis in the Outstack folder.

**Key finding:** "There is no single 'right' answer" — a triangle of constraints governs MCU selection: power consumption, display capability, processing performance.

**Critical insight: ESP32 cannot run Linux** (no MMU). Instead, ESP32 suits a **tool-based plugin system** — a linker-composed dispatch loop where each tool declares power budget, run interval, and required power domains. Radio dominates power: BLE beacons = 11× better than WiFi for brief transmission.

**STM32 spectrum:**

| MCU | Power | Display | Battery Life | Notes |
|-----|-------|---------|-------------|-------|
| L476 | 8mW | Memory LCD (monochrome) | 30 days | Ultra-low power |
| F411 | 30mW | 2.4" OLED | 7-10 days | **"Goldilocks"** |
| F429/F7 | 390mW | 5-7" TFT | 1-2 days | Rich graphics |

**Display dominates power:**
- Monochrome Memory LCD: 5mW
- Color OLED: 50mW
- TFT with backlight: 300mW (60× difference)

**Recommended stack:** STM32F411 + 2.4" OLED + FreeRTOS or bare-metal super-loop + DMA display updates. BOM ~$35-45 prototype.

**Explicit conclusion on Outstack:** "Outstack OS concepts apply to gateway/development systems, not microcontroller firmware."

---

## Cross-Cutting Themes & Tensions

**1. Outstack's identity is split.**  
In the Telux conversation it's a multi-layer *service within an OS*. In the standalone Outstack documents it's an *OS itself*. These need reconciling.

**2. Scale gap.**  
Telux aims for interstellar, millennia-scale operation. The Outstack embedded analysis targets handheld field tools running STM32 chips. The bridge between these scales is not yet designed.

**3. Power = Security is the unifying thread.**  
Both Telux and Outstack converge on the idea that power control *is* security control — a peripheral you can physically gate cannot be exploited from software.

**4. The accounting notation is the missing key.**  
Several documents reference a custom shorthand for commercial exchange that would underpin the Telux record system, but it never appears in these files. This is the single highest-priority undocumented piece.

**5. Linux overhead vs. Outstack ambition.**  
The embedded analysis recommends against Linux for battery-powered tools. Yet Outstack's most ambitious vision is Linux-based. A hybrid (Linux gateway + bare-metal nodes) seems the practical path.
