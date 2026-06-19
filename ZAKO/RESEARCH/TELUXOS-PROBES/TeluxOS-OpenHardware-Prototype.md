# TeluxOS Open Hardware Track: A Flip Phone Built for the OS
## Assembling a Purpose-Optimized Device from Open Source Components

*Speculation and Research Document — Track Two*
*Prepared: May 31, 2026*

---

## Preface: Building the Right Way Around

Every consumer phone is built hardware-first and the OS is adapted to the hardware. The vendor selects a chipset for cost and margin reasons. The hardware abstraction layer is a pile of binary blobs. The OS is a fork of Android that must accommodate whatever decisions the hardware team made.

This document proposes building the other way: start from TeluxOS's architecture — Outstack's power-as-security doctrine, the Telux sovereignty and exchange model, the five-mode operational system — and specify the hardware that would best realize these concepts from silicon upward.

The result is not the most powerful phone available in 2026. It is not trying to be. It is trying to be a device where the OS and the hardware share the same design philosophy, where the power management co-processor is exactly as important as the application processor, where the modem is a fully auditable separate module rather than a proprietary blob inside the main chipset, and where "optimized for its actual workload" beats "fastest available" because the workload is sovereign group exchange, not streaming video.

This is a speculation and research document. It contains a recommendation for hardware that, at the time of writing, can be acquired and assembled. It also contains honest assessments of what is currently missing — particularly in the flip phone form factor space — and what would need to be created rather than found.

---

## Part I: Design Philosophy

### The Flip Phone Is Not a Nostalgia Choice

The flip form factor is the correct form factor for TeluxOS for several reasons that have nothing to do with aesthetics:

**Physical mode boundaries.** The hinge creates a hardware-mediated state transition. A closed flip is a different operational state from an open flip — not just because the screen is off, but because the user has made a physical gesture that communicates intent. Telux's Island suspension on lid-close is not a software heuristic ("the user has been inactive for 30 seconds"). It is a response to a deliberate physical act. This is a more honest model of state transitions than any software timeout.

**Dedicated keypad.** A T9 physical keypad with a D-pad and call/end keys is a direct-input interface to sovereign operations. Long-press a key combination on a physical button and you get a deterministic result. There is no touchscreen ambiguity, no tap-target misfire, no accidental activation. For a device where a sovereignty invocation should be deliberate and acknowledged, physical keys are better than touch targets.

**Two displays with distinct roles.** The external display — visible when the flip is closed — is an ambient information surface. For TeluxOS, this is the Island status surface: power mode, active groups, pending authorizations, message counts. The internal display — visible when open — is the active workspace. Two surfaces with distinct roles map more cleanly to TeluxOS's operational model (ambient monitoring vs. active engagement) than a single always-on slab.

**Form factor signals intent.** A flip phone communicates "this is a tool with a specific purpose, not a general-purpose entertainment device." For professional or field deployment contexts — the Zambia field team in the Telux deployment vision, the industrial inspection scenario — a ruggedized flip phone is culturally legible as work equipment in a way that a touchscreen slab is not.

### Open Source from Silicon to UI

The hierarchy of openness targeted in this track:

1. **Kernel:** Mainline Linux (not Android, not a CAF fork). The device tree and drivers exist in mainline. No out-of-tree modules required for core functionality.
2. **Modem:** Separate modem module with AT command interface. The modem has its own processor and firmware, but the interface between modem and host is a documented standard (AT commands over USB serial or UART). The host OS does not need modem firmware blobs in its own partition.
3. **GPU:** Open-source kernel driver. This eliminates closed GPU kernel modules, which are significant security concerns (they run in ring 0 with no isolation).
4. **Boot:** U-Boot with publicly available source. Custom bootloader keys possible.
5. **OS:** PostmarketOS (Alpine Linux, musl libc, mainline kernel) as the base. No Android runtime.
6. **UI:** phosh or sxmo — established, actively maintained Linux mobile interfaces.

Points 1-4 together eliminate the blob dependency that makes the CAT S22 Flip track require orange state bootloader, unverifiable vendor partition contents, and dependence on Qualcomm's closed RPM firmware. This track is auditable from hardware to application.

### Not Fast. Efficient.

The SoC choice below will disappoint anyone looking for benchmark numbers. The RK3566 and the NXP i.MX8M Mini are not competing with the Snapdragon 8 Gen 3 on any metric except the one that matters for TeluxOS: efficient execution of its actual workload, measured in battery life per meaningful operation.

The actual workload of a TeluxOS device:
- Cryptographic operations (DID signing, capability token verification, ledger record hashing)
- Group message processing (small payloads, frequent)
- Power state management (the Outstack daemon's own computation)
- Voice calls (modem-mediated; ARM mostly idle during call)
- SMS and USSD (minimal CPU involvement)
- Ledger queries (SQLite on a small database)
- Occasional: natural language query processing (intensive but infrequent)

None of these workloads require a high-end SoC. They require a SoC with excellent idle power characteristics, robust cryptographic acceleration, good mainline Linux support, and optionally a low-power co-processor that can maintain system state while the main cores sleep.

A well-specced device for this workload looks backward by 2026 consumer standards. It is exactly right for this purpose.

---

## Part II: SoC Selection

### Requirements

The SoC for a TeluxOS flip phone must satisfy:

1. **Mainline Linux kernel support** — device tree, drivers, and a working defconfig in the mainline kernel tree. Not Android-only support. Not a vendor BSP that requires patches to build.
2. **Open-source GPU driver** — the GPU kernel module must be open source and in mainline. Mali-400 (Panfrost), Mali-G52 (Panfrost), Vivante GC series (Etnaviv) all qualify. Adreno (freedreno) qualifies for some models.
3. **Low idle power** — the SoC itself (not a fully-loaded dev board) should be targetable below 100mW at deep idle with minimal peripherals active.
4. **Hardware cryptographic acceleration** — for the DID signing and ledger hashing workloads.
5. **Manageable thermal envelope** — a flip phone has minimal thermal mass. The SoC cannot sustain high-performance loads for extended periods. This is a constraint that aligns with TeluxOS's computational profile.
6. **Community support** — active kernel maintainers, tested hardware, accessible dev boards for initial bring-up.
7. **Optional but highly desirable: M-class co-processor** — a Cortex-M4 or similar always-on processor for Outstack power management while the main cores sleep.

### Candidate Analysis

**Candidate 1: Allwinner A64**

The A64 is the SoC in the original PinePhone, the Pine A64 single-board computer, and several other open hardware devices. It is the single most thoroughly documented open-source-friendly mobile SoC in existence.

Strengths:
- linux-sunxi.org community: comprehensive wiki, tested mainline patches, high community expertise
- A53 quad-core with well-understood power characteristics
- Mali-400 GPU: fully open source via Panfrost driver, in mainline since kernel 5.2
- Extremely cheap (< $5 in volume, ~$15-50 for modules/SoMs)
- The PinePhone port of PostmarketOS is the most mature mobile Linux port in existence — starting here means standing on proven ground

Weaknesses:
- 28nm process node (2015-era) — not power-efficient compared to modern nodes
- No M-class co-processor
- Dev board idle power measured at 4-5W — but this is with peripherals active; a stripped phone PCB with careful power rail design achieves significantly lower
- The PinePhone itself demonstrated ~3-5 days standby on Linux with careful power management (power rails properly controlled)
- No hardware cryptographic accelerator (software crypto only)
- PinePhone Pro (A64's spiritual successor on RK3399) is being discontinued; community is consolidating

**Verdict on A64:** Viable. The most proven path for a first prototype. The PostmarketOS + A64 combination works today, out of the box, with the most mature software stack available for Linux mobile. Recommended for Track 2A: the fastest path to a working TeluxOS phone.

---

**Candidate 2: Rockchip RK3566**

The RK3566 is the current state of the art for open-source-friendly mid-range SoCs. Used in the PineNote, Quartz64, and several commercial development boards. Active mainline development with Collabora commercial sponsorship.

Strengths:
- Cortex-A55 quad-core @ 1.8GHz on 22nm — significantly more efficient than the A64's A53 on 28nm
- Mali-G52 GPU: Panfrost driver available, in mainline (4K playback confirmed on PostmarketOS devices)
- Strong mainline kernel support momentum (active Collabora patches, regular upstream merges)
- Power envelope: 0.03W minimum achievable, 2.5W typical operation, 5.25W peak — the minimum is genuinely impressive
- Hardware cryptographic engine (Crypto V1.5 in Rockchip's terminology)
- LPDDR4X support (more efficient memory than A64's LPDDR3)
- Better video codec support than A64

Weaknesses:
- No M-class co-processor for Outstack power management
- Dev boards ($60-120) more expensive than A64
- Less mature PostmarketOS phone support compared to A64 (works, but fewer person-years of testing)
- GPU driver (Panfrost for Mali-G52) less mature than Panfrost for Mali-400

**Verdict on RK3566:** Recommended primary SoC. The 22nm process and 0.03W achievable minimum power are strong arguments. The Collabora-backed mainline kernel support is a meaningful long-term investment. For a device where the OS will evolve over years and kernel support matters deeply, the RK3566's trajectory is better than the A64's plateau.

---

**Candidate 3: NXP i.MX8M Mini**

The i.MX8M Mini is NXP's industrial-grade application processor, used in automotive, industrial control, and the Librem 5 (older revision). Its defining feature is the Cortex-M4 co-processor alongside the primary Cortex-A53 cores.

Strengths:
- **Cortex-M4 co-processor: directly maps to Outstack's power management daemon.** The M4 runs independently while A53 cores are in deep sleep. This is architecturally identical to what Qualcomm's RPM microcontroller does on the QM215, and what Outstack's power governor is designed to exploit.
- 14nm FinFET process — the most modern process node in this list
- Industrial temperature range (-40°C to 125°C) — relevant for ruggedized deployment
- Deterministic real-time capability on M4 (suitable for sensor data collection while main SoC sleeps)
- Official NXP power management documentation (Application Notes AN13400, AN12410)
- A53 + M4 architecture enables: A53 sleeps, M4 monitors sensors, handles BLE, manages power domains — only wakes A53 when necessary

Weaknesses:
- Development kits are $199-249 (expensive for experimentation)
- Proprietary BSP heritage: mainline VPU and ISP support still catching up to NXP's vendor kernel
- More limited open hardware community than Allwinner or Rockchip
- GPU is Vivante GC NanoUltra (Etnaviv driver, mature for 2D but limited 3D)
- Price in volume is higher than RK3566 (~$15-25 vs. ~$8-12)

**Verdict on i.MX8M Mini:** The architecturally ideal choice for Outstack integration — the M4 co-processor is a perfect native substrate for the Outstack power daemon running while A53 cores sleep. However: the higher cost, less mature open-source GPU support, and proprietary BSP heritage make it the harder path. Recommended as Track 2B (alternative specification) for organizations willing to invest more deeply in the hardware layer to get the full Outstack architecture.

---

### Primary Recommendation: RK3566

For the primary open hardware track, the Rockchip RK3566 is recommended. It provides the best balance of:
- Active mainline kernel development
- Achievable ultra-low idle power (0.03W minimum)
- Adequate performance for TeluxOS workload
- Open-source GPU with accelerated 3D
- Reasonable cost

The lack of an M-class co-processor is partially compensated by the RK3566's own power management capabilities: the power management controller (PMU) can handle system sleep states and wake events independently, though not with the same architectural elegance as the i.MX8M Mini's M4.

### Secondary Recommendation: NXP i.MX8M Mini (Track 2B)

For organizations specifically interested in realizing the full Outstack power architecture — where the power management daemon is a physically separate processor with its own always-on context — the i.MX8M Mini is the right choice despite its higher cost and more complex bring-up path.

---

## Part III: Modem Selection

### The Separate Modem Philosophy

The architectural decision to use a separate modem module — rather than a SoC with an integrated modem — is fundamental to the open hardware track's security model and deserves explicit justification.

An integrated modem (as in the QM215, where the MDM9x07 is on the same die) has a direct memory path to the application processor's address space. Qualcomm's MSM8937 uses SMEM (Shared Memory) channels (`/dev/smd*`) for QMI protocol communication. The modem firmware has a pathway to host RAM. This is a well-documented attack surface: compromised modem firmware can exfiltrate data from the host system.

A separate modem module communicates with the host via a clearly bounded interface: USB serial (AT commands), USB CDC Ethernet (for data), or UART. The modem has its own processor and memory; the host system processes AT command responses as text strings. The attack surface is the AT command parser and the USB stack, both of which are auditable and much narrower than a shared memory channel to proprietary firmware.

For TeluxOS, where the security model is explicit and sovereignty of the host system is a first-class concern, the separate modem architecture is not a performance tradeoff — it is a security requirement.

### Quectel EG25-G

The Quectel EG25-G is the modem module used in the PinePhone and PinePhone Pro. It is the best-documented, most open-source-supported LTE modem module available.

Specifications:
- LTE Category 4 (150Mbps downlink / 50Mbps uplink)
- Supports LTE Band 1/2/3/4/5/7/8/12/13/18/19/20/25/26/28/38/39/40/41 (broad global coverage)
- Integrated GPS/GNSS (GPS, GLONASS, BeiDou, Galileo) — directly relevant for field deployment
- Form factor: 29mm × 32mm × 2.4mm LGA package
- Interface: USB 2.0 High Speed (primary), UART (backup), I2C for configuration
- Active power: 500mW-1W during LTE data transfer (community measurements)
- Sleep: significantly less (~5-15mW in airplane mode with module in PSM)
- Retail pricing: $50-100 USD; volume pricing significantly lower

Open source status:
- AT command interface fully documented by Quectel (public documentation)
- PinePhone community has developed open firmware patches for the EG25-G (oFono driver, ModemManager support)
- The modem's primary firmware is Qualcomm MDM9x07-based and not open, but the interface layer is standardized
- ofono + ModemManager provide full telephony stack on Linux, including voice calls, SMS, USSD, STK, MMS, LTE data

**Verdict:** The EG25-G is the only viable choice for this application. No other modem module combines LTE Cat 4, GNSS, proven PostmarketOS support, and documented AT interface at this price point.

### Modem as Telux Island Member

In TeluxOS architecture, the modem is a distinct entity — not part of the host OS, with its own identity. On the open hardware track with a separate modem module:

- The EG25-G gets a DID (`did:key` derived from its hardware serial number)
- It joins the device's primary Island as a service member
- All data sessions it initiates are recorded in the exchange ledger (source: device DID, destination: remote IP → DNS-resolved domain, resource: bytes, authorized by: sovereign)
- This makes the ledger a complete record of network activity — not a security audit log (TeluxOS is not a monitoring tool) but a self-sovereign record the device owner controls and can query

---

## Part IV: Complete Bill of Materials

### Primary Configuration (RK3566 Track)

| Component | Part | Specification | Estimated Cost |
|-----------|------|---------------|----------------|
| SoC | Rockchip RK3566 | Cortex-A55 quad @ 1.8GHz, Mali-G52 | Via SoM |
| SoM (System on Module) | Radxa ROCK 3 Compute Module or similar | RK3566, 2-4GB LPDDR4, 32GB eMMC | $45-65 |
| Modem | Quectel EG25-G | LTE Cat 4, GNSS, 29×32mm LGA | $25-40 (volume) |
| WiFi/BT | AzureWave AW-CM276NF or Realtek RTL8822CS | 802.11ac 2×2 + BT 5.0, SDIO/USB | $8-15 |
| Main Display | 3.5" 480×640 IPS MIPI-DSI panel | SH8601A controller (open driver) or ST7796S | $8-15 |
| Cover Display | 1.3" 240×240 SPI IPS panel | ST7789 controller (in mainline kernel) | $3-6 |
| Battery | 3000mAh Li-Po, 3.7V nominal | ~60mm × 50mm footprint | $5-10 |
| PMIC | External LTC4162 or BQ25895 for charging | USB-C PD charging | $4-8 |
| Audio Codec | Wolfson WM8904 or NXP SGTL5000 | I2S, earpiece + speaker + mic | $3-6 |
| Amplifier | Texas Instruments TPA2012D2 or similar | Class D, mono, <1W | $2-4 |
| Sensors | STMicro LIS2DH12 (accelerometer), VCNL4040 (proximity), AH3761 (hall effect lid) | I2C bus | $4-8 total |
| USB-C | USB-C PHY + controller (FUSB302) | USB 2.0, PD negotiation | $3-5 |
| SIM socket | Nano-SIM, push-pull | — | $1-2 |
| Keypad | Custom T9 PCB with dome switches | 12 digits + D-pad + call/end (18 keys) | $4-8 |
| Hinge | Dual-barrel flip hinge, ~60mm span | Stainless steel, 180° travel | $5-10 |
| Flex cable | Hinge flex: 10-lane FPC, 200mm length | Display + USB pass-through | $3-6 |
| PCB fabrication | 2× 4-layer boards (main + cover) | JLCPCB 5 sets | $50-80 (prototype) |
| Enclosure | 3D-printed PLA/PETG prototype | FDM prototype | $10-20 |

**Estimated first prototype cost: $183-308 in parts** (at individual quantities, not volume)
**Volume cost at 100 units: $60-95 per unit** (component cost only, no assembly, no NRE)

Note: These are research-grade cost estimates based on current distributor pricing (Digi-Key, Mouser, LCSC). PCB assembly (PCBA) services add $15-40 per board at small volumes. A first functional prototype with external assembly is realistically $300-600 total per device.

### Alternative Configuration (NXP i.MX8M Mini Track)

Replace RK3566 SoM with:

| Component | Part | Delta Cost |
|-----------|------|-----------|
| SoM | Toradex Verdin i.MX8M Mini or similar | +$60-80 vs. RK3566 SoM |
| Power MCU | M4 runs Outstack power daemon natively | No additional cost (M4 is on-die) |
| PMIC | NXP PF8100 (paired with i.MX8M Mini) | Included or $8 |

The NXP track's PCB is more complex due to the i.MX8M Mini's power architecture requirements (the PF8100 PMIC must be properly sequenced with the SoC). NXP's evaluation kits document this thoroughly; first-time designers should budget additional bring-up time.

---

## Part V: Physical Design

### Form Factor Target

Target dimensions (closed): 60mm wide × 105mm tall × 18mm deep.

Reference: The original Motorola RAZR V3 was 53mm × 98mm × 14mm closed. The Kyocera DuraXV Extreme (a modern rugged flip) is 57mm × 112mm × 18mm. The CAT S22 Flip is 55mm × 111mm × 20mm. The target is within this envelope.

Internal volume budget (each half):
- Lower half (keypad + modem + battery): ~52mm × 95mm × 8mm usable area
- Upper half (display + main PCB + SoM): ~52mm × 95mm × 8mm usable area
- Hinge: consumes ~18mm of depth and 12mm of each half's bottom edge

### Two-PCB Architecture

**Upper board (main board):** RK3566 SoM mounted, MIPI-DSI connector for main display, I2S for audio codec, USB-C connector (accessible at top edge), WiFi/BT module, SIM card slot, I2C expansion bus.

**Lower board (keypad/modem board):** T9 keypad matrix controller, D-pad, call/end key switches, Quectel EG25-G modem mounted flat, battery connector, charging IC, hall effect sensor (detects lid closure via magnet in lower half's top edge).

**Interconnect flex cable:** 10-conductor FPC through the hinge barrel:
- Lane 1-4: USB 2.0 HS differential pairs (USB from lower board to upper board for modem over USB)
- Lane 5-8: Power rails (battery + power from lower PMIC to upper board)
- Lane 9: I2C SDA (keypad matrix controller to main SoC)
- Lane 10: I2C SCL

**Cover display:** Small separate PCB on the outer face of the upper half. Connected to the upper main board via a short internal FPC. SPI interface (SCK, MOSI, CS, DC, RST = 5 lines). ST7789 controller on a 1.3" 240×240 panel. Always-on power domain (200µW draw when displaying static image using memory-in-pixel feature if available, otherwise 5-15mW for standard refresh).

### Hinge and Lid Detection

The hinge is a standard dual-barrel consumer electronics hinge. A small magnet embedded in the lower half's top edge triggers the hall effect sensor (AH3761) on the lower PCB's top edge. The AH3761 generates a GPIO interrupt on lid-close/open, which the kernel exposes as `SW_LID` input events. This is identical to the mechanism on the CAT S22 Flip and all other modern flip phones.

The hinge flex cable's strain relief is critical: each time the device opens and closes, the flex bends. Consumer electronics flex cables through hinges specify 50,000-100,000 bend cycles as minimum. For a device that might open/close 50 times per day, this is 2-5 years of use. Use a cable with appropriate flex specifications and ensure the hinge geometry is designed to avoid sharp-radius bending.

---

## Part VI: OS Stack Selection

### PostmarketOS: The Natural Host for Outstack

PostmarketOS (pmOS) is an Alpine Linux derivative for mobile devices. The alignment with the Telux/Outstack architecture is not coincidental — it is the strongest argument for this track.

**Alpine Linux is what Outstack targets.** The Telux/Outstack design documents explicitly name Alpine Linux as the preferred base system:
> "Alpine Linux (preferred) ... Recommended: Vendored fork model (fork LTS kernel, vendor musl libc, track upstream security)"

PostmarketOS is exactly this: a vendored Alpine derivative with a community-maintained LTS kernel fork (mainline kernel with mobile-specific patches), musl libc (no glibc overhead), and BusyBox userspace. The entire design philosophy — minimal by default, add only what you need, no unnecessary background processes — is the same philosophy as Outstack.

**OpenRC is already there.** PostmarketOS uses OpenRC as its init system, identical to Alpine. The Outstack design documents describe modifying OpenRC to integrate power-mode-aware service startup:
> "Modified OpenRC Init: Reads battery at boot, selects initial mode, starts only services appropriate for that mode. Each service has MIN_MODE and POWER_CLASS settings."

This is implementable directly on PostmarketOS without any framework modification. OpenRC scripts have full access to battery state, and adding MIN_MODE and POWER_CLASS metadata to service configuration is a straightforward OpenRC extension.

**Mainline kernel is the rule, not the exception.** PostmarketOS's packaging guidelines strongly prefer mainline kernel support. The RK3566 has active mainline patches. This means the device tree and drivers that PostmarketOS uses will be upstreamable and maintainable over time — not a private fork that diverges from the kernel community with every major release.

**Package system.** Alpine APK is simple, fast, and correct. Building and distributing Telux/Outstack system services as Alpine APK packages is straightforward. The package format supports pre/post install scripts, dependency declarations, and the same build infrastructure used for Outstack's described "Custom tier" packages.

### UI Layer: sxmo as the Default

PostmarketOS supports multiple UI options. For TeluxOS's flip phone profile, the primary recommendation is sxmo (Simple X Mobile).

**Why sxmo:**
- Keyboard-first and hardware-button-first interface paradigm: exactly right for a T9 keypad device
- Minimal resource consumption (dwm-based window manager, or sway for Wayland)
- Shell-script-first configuration: the same ethos as Alpine's pragmatic minimalism
- Physical button bindings are a first-class feature, not an afterthought
- Active maintenance for PostmarketOS devices

**Phosh as alternative:**
phosh (GNOME Phone Shell) is the more mature and polished Linux mobile UI, used by the Librem 5 and many PostmarketOS devices. It is touchscreen-first, which is a mismatch for a T9 keypad flip phone. However, phosh is well-integrated with the Linux mobile stack (oFono, ModemManager, NetworkManager, PulseAudio/PipeWire) and provides more application compatibility for existing Linux mobile applications. Recommended for development and testing; sxmo for production deployment.

### T9 Input on Linux

The T9 physical keypad is a challenge on Linux mobile. The kernel sees 18 key events (12 numeric + 6 directional/call keys). An input method engine must interpret multi-tap sequences (pressing 2 three times = 'c' in T9 conventional) and dictionary-based predictive input.

Options:
- **ibus-t9**: an open-source IBus input method for T9 key sequences. Available in Alpine repositories. Functional for basic input.
- **custom T9 daemon**: a small C or Rust daemon that reads `/dev/input/` events, maintains a multi-tap state machine, and writes composed characters to a virtual keyboard device. Most control; most work.
- **fcitx5 with T9 plugin**: fcitx5 is a well-maintained input framework. A T9 plugin would need to be developed (existing T9 plugins exist for fcitx4 and could be ported).

For Zambia/African language deployment: Bemba, Nyanja, Tonga, and Lozi all use Latin character sets with minor diacritical additions. T9 multi-tap input for these languages follows the same pattern as English T9. The additional characters (ŵ in Tonga, ŋ in some orthographies) can be mapped to long-press sequences on the 1/# keys.

---

## Part VII: Outstack Integration on This Stack

### The Native Environment

On the open hardware PostmarketOS track, Outstack is not a system service layered on top of an existing OS. It is a first-class system component, built as Alpine APK packages, managed by OpenRC, accessing hardware power domains directly through the kernel's device tree and sysfs interfaces.

**`outstack-powerd` on PostmarketOS:**

The power daemon is an OpenRC service, started at boot priority S50 (after hardware init, before application services). It reads:
- Battery state: `/sys/class/power_supply/battery/{capacity,status,current_now,voltage_now,charge_full,cycle_count}` — the Linux power supply sysfs interface, populated by the kernel driver for the battery management IC
- CPU temperature: `/sys/class/thermal/thermal_zone*/temp`
- Peripheral power states: accessible via device tree power domain interfaces in `/sys/kernel/debug/` (when debugfs mounted) or via standard sysfs device power management files

For the RK3566, Rockchip's power management architecture exposes CPU cluster power states and peripheral clocks through a combination of:
- `cpufreq` governor (schedutil recommended) for CPU frequency scaling
- `cpuidle` driver for CPU sleep states
- Runtime PM (`/sys/bus/platform/devices/*/power/`) for peripheral power management
- Rockchip's power domain driver (`rockchip-pd`) which allows power gating entire subsystems (GPU, USB, VPU) through device tree-described power domains

This is the hardware power gating capability that Outstack identifies as physically unforgeable: a GPU that is power-gated via `rockchip-pd` is not merely software-disabled — it is without power, and no software running on the A55 cores can undo that without going through the kernel's power domain management, which `outstack-powerd` controls.

**Process Power Classes on Alpine + cgroups v2:**

Unlike the AOSP Android 4.9 track, PostmarketOS on a modern kernel (6.x) has cgroups v2 with unified hierarchy. The five Outstack process classes map directly:

```
/sys/fs/cgroup/
├── critical.slice/         (cpu.weight=1000, memory.min enforced)
├── interactive.slice/      (cpu.weight=100)
├── background.slice/       (cpu.weight=10, io.weight=50)
├── deferred.slice/         (cpu.weight=1, SIGSTOP in CRITICAL mode)
└── opportunistic.slice/    (cpu.weight=1, frozen in CONSERVE mode)
```

OpenRC services declare their power class in their init script via a `POWER_CLASS` variable. `outstack-powerd` assigns newly started daemons to the appropriate cgroup based on this declaration. The power class assignment is permanent for the service's lifetime; changing it requires sovereign authorization.

**Execution Gating on Modern Kernel:**

Linux 5.13+ includes Landlock (file access control LSM) and modern BPF programs can hook sys_execve. On a 6.x kernel (which PostmarketOS targets for new devices), `outstack-powerd` can implement execution gating using a BPF program that:
1. Intercepts `sys_execve` for non-critical executables
2. Queries a shared memory region maintained by `outstack-powerd` for the current system power mode
3. Returns `-EPERM` (mapped as "insufficient power budget") in EMERGENCY mode for DEFERRED class executables
4. Logs the gate event to the Telux ledger

This is the kernel-level execution gating that was described as theoretically desirable but not implementable on Android kernel 4.9. On PostmarketOS with a modern kernel, it is implementable.

### For the NXP i.MX8M Mini Track: Outstack on the M4

The M4 co-processor opens a qualitatively different architectural mode for Outstack. Rather than a Linux daemon that monitors and reacts to power state, Outstack's power management can be a real-time firmware application running natively on the M4, with the A53 Linux system as a supervised peer.

**M4 firmware responsibilities:**
- Continuous battery monitoring via I2C to the charging IC (BQ25895 or similar)
- Hall effect sensor interrupt handling (lid open/close)
- System power mode state machine (the five-mode model runs entirely on M4)
- A53 wake/sleep arbitration: M4 sends an interrupt to wake A53 when power mode changes, incoming call arrives (from EG25-G via UART), alarm fires, or lid opens
- Low-power peripheral management: accelerometer, proximity sensor, cover display refresh — all on M4, A53 fully asleep

**A53 (Linux/TeluxOS) responsibilities:**
- Application execution (newgroup, Island management, exchange ledger, UI)
- Cryptographic operations (DID signing, ledger hashing)
- Modem data session management
- User interaction

**Communication between M4 and A53:**
NXP's i.MX8M Mini uses RPMsg (Remote Processor Messaging) over shared memory for M4-A53 communication. This is a standard Linux kernel framework (`drivers/rpmsg/`). The A53 Linux driver sends commands to M4 firmware via RPMsg; M4 firmware sends power events back. `outstack-powerd` on A53 is reduced to an RPMsg client — it no longer polls sysfs for battery state (M4 maintains that), it receives push notifications from M4 about mode changes.

**The architectural significance:** The M4 is the bedrock of the Outstack power hierarchy. In Telux terms, it is closer to the Bedrock Layer — "immutable audit trail, TPM/HSM key storage" — than the Submerged Layer (where the daemon lives). The M4 firmware is not replaced by user-space operations. It is verified at boot (if using NXP's High Assurance Boot with signed M4 firmware image). It is isolated from the Linux world by hardware boundary.

This architecture realizes the Outstack vision more completely than any Android-layer implementation. The power domain controller is a physically separate processor whose firmware is a separately-audited, separately-signed binary. Compromising the Linux A53 world cannot compromise the M4's power management model.

---

## Part VIII: Telux Primitives on Native Linux

Without Android's runtime constraints, Telux primitives can be implemented much closer to the vision documented in the architecture papers.

### newgroup as D-Bus Service + Linux Namespaces

`telux-groupd` on Linux runs as a system D-Bus service. The group message bus uses D-Bus signals within each Island's network namespace. Member processes join the Island by registering with `telux-groupd` via D-Bus, presenting a signed capability token (generated by their DID and authorized by the Island sovereign).

Linux network namespaces provide the communication isolation: each Island has its own network namespace. Traffic between Islands must pass through a policy-enforced bridge. Traffic outside an Island (to the external network) passes through the modem's IP interface, gated by `outstack-powerd`'s network budget policy.

### Islands as Linux Namespaces + cgroups v2 + Landlock

On a 6.x kernel, Islands are:

- **Network namespace:** Isolated network stack per Island (no direct cross-Island network visibility)
- **Mount namespace:** Island-specific filesystem view (Island data directories only)
- **cgroups v2 subtree:** Island-specific resource limits (CPU, memory, I/O, network bandwidth)
- **Landlock rules:** File access restrictions specific to the Island's data paths
- **User namespace:** (optional, if full UID isolation is desired) Each Island can have its own UID map

This is a much stronger isolation than anything achievable on Android Go with a 4.9 kernel. The combination of these Linux primitives creates an Island that is genuinely isolated at the kernel level, not just by SELinux policy.

### Exchange Ledger: SQLite + CBOR + ed25519

The ledger implementation is identical in concept to the Android track but implemented as a standalone Alpine service:

- SQLite database in `/var/lib/telux/ledger/` (root-owned, Island-accessible only via `telux-ledgerd` IPC)
- CBOR record encoding (using `libcbor` from Alpine repositories)
- ed25519 signing via `libsodium` (or `openssl` if hardware crypto acceleration is preferred)
- Hash chain: each record's hash is stored and the next record's CBOR encoding includes `prev_hash` field
- On the NXP track: signing keys stored in M4-managed secure enclave if hardware supports it; alternatively on-device libsodium with key sealed to TPM if available

### Natural Language Query: Whisper + Gemma on 4GB RAM

The most significant capability enabled by 4GB RAM (achievable on RK3566 or i.MX8M Mini vs. the 2GB on CAT S22 Flip):

**Voice-to-ledger query pipeline:**
1. User speaks query ("What did the sensor service send to the billing island last Thursday?")
2. `whisper.cpp` (quantized tiny model, ~70MB, runs on A55 cores in ~2 seconds) transcribes voice to text
3. Transcribed text goes to the query layer: either rule-based parser (0 overhead) or small LLM
4. For 4GB RAM: `llama.cpp` running Gemma 2B (4-bit quantized, ~1.3GB) can process a domain-specific query in 5-10 seconds on A55 cores
5. LLM output is parsed for ledger query parameters; actual ledger access goes through `telux-ledgerd` with calling DID's authorization scope enforced

The Gemma 2B model at 4-bit quantization fits in ~1.3GB. With 4GB RAM and a minimal Alpine userspace (vs. 2GB with Android Go runtime), this is feasible for occasional query use. Continuous LLM inference is too power-intensive for frequent use; the model should be loaded on demand and unloaded after a configurable idle period.

Power cost of a Gemma 2B query on A55: approximately 1.5-2.5W for 5-10 seconds = 2-7mWh per query. At 3000mAh battery capacity (~11.1Wh usable), this is 0.02-0.06% of battery per query. Negligible.

---

## Part IX: Power Optimization Strategy and Targets

### The Achievable Numbers

The following estimates are based on component datasheets, PinePhone community measurements (adjusted for hardware differences), and PostmarketOS power management research. They are targets, not guarantees — real-world measurements on actual hardware will differ.

**Deep sleep (lid closed, A53 in sleep, modem in eDRX 40.96s cycle):**
- RK3566 in deep sleep: ~20-30mW (community reports from Quartz64, adjusted for mobile-form reduction)
- EG25-G modem in eDRX: ~15-30mW
- ST7789 cover display (static image, 1-second refresh): ~5-10mW
- Hall effect sensor, accelerometer standby: ~1mW
- Total: ~41-71mW
- At 3000mAh / 3.7V (~11.1Wh): **156-270 hours** theoretical deep sleep

**Reality factor:** Real-world standby is typically 40-60% of theoretical due to periodic wakeups (incoming SMS, calls, eDRX page cycles, cron jobs). Realistic target: **60-120 hours standby** (2.5-5 days).

**For comparison:** A stock Android phone with GMS in the same class achieves 48-72 hours standby. A de-Googled Android (Babb OS) achieves 72-108 hours. TeluxOS on this hardware targets the top end of the achievable range for a device with active LTE connectivity.

**Active use (lid open, screen on, light usage — calls, messaging, ledger queries):**
- RK3566 at moderate load (1-2 active cores): ~400-700mW
- Display (3.5" IPS at 50% brightness): ~150-250mW
- EG25-G in voice call: ~300-500mW
- Audio: ~100-200mW during call
- Total during voice call: ~950-1,650mW
- Voice call battery life: **6.7-11.7 hours continuous call** from 3000mAh battery

**Typical daily usage pattern estimate:**
- 30 minutes screen-on (UI, queries, messaging): ~500mW avg × 0.5h = 250mWh
- 15 minutes voice calls: ~1.3W avg × 0.25h = 325mWh
- 23.25 hours standby: ~55mW avg × 23.25h = 1,279mWh
- Total daily consumption: ~1,854mWh
- Against 3000mAh × 3.7V = 11,100mWh usable: **~5.9 days per charge at this usage pattern**

This matches the target for a device where charging infrastructure may be irregular.

### The Outstack Power Mode Calendar

A day in the life of a TeluxOS flip phone:

| Time | Event | Outstack Mode | Active Components |
|------|-------|--------------|-------------------|
| 23:00 | User closes flip | CONSERVE | M4 only, eDRX 20s |
| 23:05 | Inactivity threshold | CRITICAL | M4 only, eDRX 40s |
| 03:00 | Incoming call | M4 wakes A53 | A53 boots in <2s, call rings |
| 03:02 | Call ends, flip closes | CRITICAL | Back to deep sleep |
| 07:00 | User opens flip | NORMAL | A53 active, screen on |
| 07:05 | Battery 45% | NORMAL | All services active |
| 12:00 | Battery 20% | CRITICAL | Background daemons suspended |
| 15:00 | Battery 8% | EMERGENCY | Only calls and sovereign auth |
| 18:00 | Plugged in to charge | NORMAL (charging) | All services resume |

The mode transitions happen automatically, without user action, based on battery state. The user experiences them as: the phone lasts longer between charges. Applications they didn't explicitly open don't run when the battery is low. The phone receives calls even when the battery is nearly dead because the modem management is intelligently isolated.

---

## Part X: Build Path

### Step 0: Software Before Hardware

The primary mistake in custom hardware projects is ordering PCBs before the software is understood. The open hardware track begins with software on existing hardware.

**Development environment:**
1. Acquire a Radxa ROCK 3A or Quartz64 development board (RK3566, ~$60-80). This SoC is the same as the target device.
2. Install PostmarketOS on the development board. Follow pmOS wiki for RK3566 devices.
3. Build and test `outstack-powerd` on the development board. Verify five-mode state machine against development board power measurements.
4. Build and test `telux-groupd`, `telux-ledgerd`, `telux-identd` on the development board.
5. Implement basic sxmo configuration with T9 keypad simulation (USB keyboard in T9 mode).
6. When software stack is stable on development board, proceed to PCB design.

**For NXP i.MX8M Mini track:**
1. Acquire NXP i.MX8M Mini EVK (~$249). Flash PostmarketOS (i.MX8M Mini is supported).
2. Develop M4 firmware using NXP's MCUXpresso SDK (C, no OS required on M4).
3. Implement Outstack power state machine as M4 firmware, RPMsg interface to A53.
4. Verify A53 deep sleep with M4 managing wake events.

### Step 1: Schematic Design

Using KiCad (open source EDA tool). Start from:
- RK3566 datasheet reference schematic
- EG25-G application circuit (Quectel publishes this)
- Wolfson WM8904 reference design
- ST7789 SPI display reference circuit

Critical design reviews before PCB layout:
- Power sequencing: RK3566 has strict power rail sequencing requirements
- PCB antenna design for EG25-G cellular and WiFi/BT (antenna placement, ground plane cutouts)
- USB-C PD negotiation (FUSB302 or equivalent controller)
- Flex cable routing through hinge

### Step 2: PCB Layout and Fabrication

Layout in KiCad. Two separate boards (upper/lower), designed to mate through hinge flex.

PCB specification:
- 4-layer stackup (signal / GND / power / signal)
- 0.8mm overall board thickness (allows battery and components to fit in 8mm half-height)
- Impedance-controlled traces for USB HS differential pairs through flex

Fabrication at JLCPCB or PCBWay: 5 sets of both boards, assembled (SMT components), ~$200-400 depending on component complexity. Delivery: 2-3 weeks.

### Step 3: Hardware Bring-Up

Sequence:
1. Power on with USB-C connected, no battery. Verify power rails with multimeter before connecting SoM.
2. Connect SoM. Attempt U-Boot boot via SD card. Confirm UART console output.
3. Boot PostmarketOS from SD card. Confirm kernel boots, SSH accessible.
4. Validate each peripheral: WiFi, modem AT commands over USB, audio, display (main then cover), keypad matrix, hall effect sensor, battery management.
5. Port device tree from development board to new hardware. Add nodes for cover display, keypad controller, hall effect sensor.
6. Once all peripherals confirmed working, move rootfs to eMMC.

Expected timeline: 4-8 weeks for hardware bring-up by an experienced embedded developer. Longer for first-time PCB designers.

### Step 4: Integration and Testing

With hardware working and PostmarketOS running, integrate the Telux/Outstack software stack from Step 0 (developed on dev board). Most of this is a direct transplant with device-specific configuration.

Final validation:
- Phone calls work (voice + SMS + USSD + STK)
- TeluxOS Island creation and sovereignty operations
- Exchange ledger write and query
- Power mode transitions verified on actual battery
- 72-hour standby test with periodic incoming calls

---

## Part XI: Track Comparison

| Dimension | AOSP Track (CAT S22) | Open Hardware Track (RK3566) |
|-----------|---------------------|------------------------------|
| Time to first working prototype | 3-6 months | 9-18 months |
| Telux integration depth | Android service layer | Kernel-native (LSM, BPF, namespaces) |
| OS purity | Android Go (blobs required) | Full mainline Linux (no mobile blobs) |
| Power optimization ceiling | Moderate (Android overhead) | High (Alpine minimal userspace) |
| Flip phone UI maturity | Excellent (Android handles it) | Partial (sxmo + custom T9) |
| Modem security model | Integrated (shared memory, blobs) | Separate module (AT interface, auditable) |
| Boot chain auditability | Orange state (no custom key re-lock) | Full U-Boot + custom signing keys |
| Maintenance burden | High (Android Go releases, CAF kernel) | Moderate (mainline kernel, PostmarketOS) |
| Natural language capability | Limited (2GB RAM) | Full (4GB RAM, Gemma 2B viable) |
| Risk | Low-moderate | High (PCB errors, new hardware) |
| Cost per unit at volume | ~$0 new hardware cost (device already exists) | ~$60-95 BOM at 100 units |
| Outstack M4 architecture | Not available (no M4 on QM215) | Available on NXP i.MX8M Mini variant |

### Recommended Sequencing

The two tracks are not alternatives — they are sequential milestones.

**Build the AOSP prototype first.** It is faster, it uses existing hardware, and it validates the Telux/Outstack concepts on a real device in the real deployment context (Zambia, field teams, mobile money infrastructure) before any new hardware investment. The Babb OS foundation means the first Telux capabilities can run within months, not years.

**Build the open hardware prototype second.** Informed by everything learned from the AOSP prototype, design hardware that removes the constraints the AOSP track must live with: the 32-bit kernel, the proprietary blobs, the 2GB RAM ceiling, the non-re-lockable bootloader. The open hardware prototype is where the full Telux vision is realized.

**The open hardware prototype feeds back to a next-generation device.** Once a working open hardware prototype exists, the design can be refined: better power measurements inform future BOM decisions, field deployment testing identifies form factor improvements, and the relationship between Outstack's power model and the device's actual power hardware can be tuned to real-world measurements rather than datasheet estimates.

---

## Coda: The Device That Earns Its Existence

A phone built from open source components, running a Linux system derived from Alpine, with a separate auditable modem, a power management co-processor running Outstack firmware, a sovereignty enforcement daemon, and an exchange ledger recording every meaningful operation between Island members — this device is not remarkable because it has the best specifications available in 2026.

It is remarkable because every layer of it can be read. The kernel source is in mainline Linux. The modem interface is AT commands documented in a public Quectel specification. The bootloader is U-Boot. The operating system is PostmarketOS with Alpine packages. The Outstack daemon is compiled from source you own. The Telux ledger records are CBOR-encoded, ed25519-signed, chain-hashed entries in a SQLite database you can query directly.

The device earns its existence not by being faster than alternatives but by being *transparent*. By being the kind of device that can be deployed in a field context where the people using it can know, at every level they choose to investigate, what the device is doing and why.

That is what Telux was designed to be: not an operating system for the people who build the most sophisticated devices, but an operating system for the people who need to trust the device they are holding.

---

*See also: `TeluxOS-AOSP-Prototype.md` for the complementary track — integrating Telux architecture with the existing CAT S22 Flip Babb OS project via Android/AOSP.*
