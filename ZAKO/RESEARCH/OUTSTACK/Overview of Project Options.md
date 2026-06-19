# Overview of Project Options — Outstack Implementation Approaches

**Date:** 2025-01-28
**Status:** Decision analysis
**Context:** Deciding where in the system stack the Outstack power governance daemon should live

---

## The Question

Outstack needs to:
1. Read hardware power state (battery, thermal, CPU draw)
2. Classify all running processes
3. Enforce class gating (stop/resume processes based on mode)
4. Write records to the power ledger
5. Emit signals to other HOME services
6. Do all of this with minimal power overhead itself

Where in the system stack should this logic live? The options span from deep kernel integration to pure userspace. Each has consequences for reliability, maintainability, and the project's relationship with upstream.

---

## Option 1: Kernel-Level Integration

### Description

Build Outstack as a kernel module or set of kernel patches. The mode state machine lives in kernel space. Process gating uses direct scheduler integration — the kernel scheduler itself knows about Outstack classes and can refuse to schedule gated processes. Battery/thermal readings are direct hardware register reads, zero overhead.

### Pros

- **Lowest latency.** Mode transitions are immediate. No context switch to userspace needed. Process gating is a scheduler flag, not a signal.
- **Highest reliability.** Kernel code can't be OOM-killed. Can't be starved of CPU by a runaway process. If the kernel is running, Outstack is running.
- **Direct hardware access.** Fuel gauge registers, thermal zones, PMIC control — all accessible directly. No HAL abstraction overhead.
- **Atomic enforcement.** The scheduler can refuse to run a gated process in the same cycle it would have been scheduled. Zero window of non-enforcement.

### Cons

- **Maintenance nightmare.** Every kernel upgrade requires rebasing our patches. We're already committed to tracking CAF/LTS upstream for security. Kernel patches create unbounded forward maintenance cost.
- **Upstreaming unlikely.** The Linux kernel power management maintainers are not going to accept an entirely new process governance framework from a small project. We'd carry this forever.
- **Platform coupling.** Kernel interfaces differ between SoC vendors. The QM215's PMIC interface (SPMI + RPM) is Qualcomm-specific. MediaTek is different. If ZAKO ever runs on non-Qualcomm hardware, the kernel module needs a per-platform port.
- **Debugging difficulty.** Kernel bugs crash the system. A mode-transition bug in kernel space is a reboot. In userspace, it's a daemon restart.
- **SELinux complexity.** Kernel modules need careful SELinux policy. Adding a new kernel-level actor to the Android security model is a significant audit surface increase.
- **Contradicts our upstream compact.** We committed to minimum kernel modification. A kernel module is maximum kernel modification.

### Verdict

Too expensive to maintain. Too coupled to hardware. Contradicts our architecture principles. Reject.

---

## Option 2: Android Framework Integration (System Server)

### Description

Integrate Outstack into Android's system_server process. Add a new system service (PowerGovernanceService) that lives alongside BatteryService, PowerManagerService, etc. Use Android's existing BatteryManager callbacks, thermal callbacks, and ActivityManagerService for process management.

### Pros

- **Access to existing infrastructure.** Android already tracks battery state, thermal zones, and process lifecycles. We'd be wiring into an existing sensor framework.
- **Process management built-in.** ActivityManagerService already knows about every app process. We can use its existing freeze/thaw mechanisms (AOSP has process freezing in Android 13+).
- **Standard upgrade path.** If we structure it as an additional service in frameworks/base, AOSP upgrades are a merge rather than a rebase.
- **SELinux well-understood.** System services in system_server have established SELinux patterns.

### Cons

- **Java/Kotlin in the critical path.** system_server is a JVM process. GC pauses in the middle of a mode transition are unacceptable. CRITICAL processes must not experience scheduling gaps because system_server is collecting garbage.
- **system_server restart kills everything.** If our service crashes system_server (possible with any Java exception that escapes our service boundary), the entire UI and all apps restart. This is not acceptable for a CRITICAL process class guarantee.
- **Coupling to AOSP internals.** Every time Google refactors BatteryService or ActivityManagerService, our integration breaks. AOSP's internal APIs are not stable.
- **Over-privileged context.** system_server has access to everything. Our power daemon needs specific, scoped privileges — battery state, cgroup control, signal delivery. Running in system_server gives it far more access than it needs.
- **Record writing from Java.** Our BitPads record format is a binary wire format. Writing it from Java through JNI to telux-ledgerd adds unnecessary complexity and latency.

### Verdict

Too coupled to AOSP's unstable internals. JVM in the critical path is architecturally wrong for a power governance system. The reliability requirements don't match the runtime characteristics. Reject.

---

## Option 3: Leverage Android's Existing Doze/Standby

### Description

Don't build a custom governance system at all. Instead, configure Android's existing Doze mode, App Standby Buckets, and Restricted Standby to achieve similar results. Map our process classes to standby bucket assignments. Use Doze maintenance windows as our background execution windows.

### Pros

- **Zero development cost.** Already built. Already tested on billions of devices. Already handles edge cases we haven't thought of.
- **GMS-free Doze still works.** Doze is part of AOSP, not GMS. We can use it without Google dependencies.
- **OEM-optimized.** On the QM215, the power HAL already knows how to drive the RPM into deep sleep during Doze. We'd benefit from Qualcomm's per-platform optimization.

### Cons

- **Advisory, not mandatory.** Doze is designed to be transparent to apps. Apps can hold partial wakelocks to escape it. Apps can request battery optimization exemption. The system can't guarantee that a gated process stays gated. This fails our core requirement.
- **No sovereignty.** The user can't inspect why Doze did what it did. No records. No audit trail. No ledger. The system's power decisions are opaque internal state.
- **No class model.** Standby buckets are four tiers (Active, Working Set, Frequent, Rare) defined by usage recency, not by process importance. A payment daemon that's rarely used interactively would be bucketed as "Rare" and aggressively throttled — exactly wrong.
- **No mode transitions.** Doze is binary: Doze or not-Doze. It doesn't have intermediate governance modes. Going from "mild conservation" to "critical reserve" isn't expressible.
- **No thermal integration.** Doze doesn't enter based on thermal state. It enters based on motion sensor and screen state. A phone overheating in a pocket won't trigger Doze.
- **No cross-service coordination.** Doze doesn't signal other daemons about its state transitions. HOME services can't coordinate their behavior around Doze events.

### Verdict

Doze solves a different problem (extending standby time) with a different philosophy (be invisible to apps). It's a good mechanism for what it is, but it can't express our governance model. We need mandatory enforcement, records, thermal response, and multiple graduated modes. Doze gives us none of these. Reject as primary mechanism.

However: we should *not disable Doze*. Doze operates below our governance layer and provides hardware-level sleep optimization that benefits all modes. Outstack governs which processes run; Doze optimizes the power draw of whatever the kernel is doing in the gaps. They're complementary.

---

## Option 4: Real-Time OS Approach

### Description

Run Outstack on a separate processor (e.g., the QM215's RPM co-processor, or an external MCU) as a real-time system. The power governance logic executes on dedicated hardware with deterministic timing. It communicates with the Linux system via a defined interface (shared memory, IPC, GPIO signals).

### Pros

- **Deterministic.** A RTOS on dedicated hardware can guarantee response time to power events. No kernel scheduling interference. No memory pressure effects. No GC. No process starvation.
- **Physically isolated.** A compromised application processor can't interfere with power governance. The governance system is on separate silicon.
- **Always-on.** An MCU can run continuously even when the main processor is in deep sleep. It can wake the main processor only when needed.
- **The "right" architecture.** This is how spacecraft actually do it. The power system is a separate subsystem with its own computer.

### Cons

- **QM215 RPM is not programmable.** On our target hardware, the RPM microcontroller runs Qualcomm's signed firmware. We cannot load custom code onto it. This option requires either different hardware or an external MCU.
- **Hardware dependency.** Adding an external MCU adds BOM cost, board space, power draw (ironic), and a new failure mode. It makes ZAKO hardware-specific in a way that contradicts running on commodity devices.
- **Communication overhead.** IPC between the MCU and Linux introduces latency and complexity. Mode transitions need to propagate from the MCU to Linux services — that's a cross-chip protocol to design, test, and maintain.
- **Debugging difficulty.** Two-chip debugging is harder than single-chip. Reproducing race conditions between the MCU and Linux requires specialized tooling.
- **Premature for v1.** We haven't validated the mode model itself yet. Building dedicated hardware for an unproven governance model is backwards. Validate in software first.

### Verdict

Architecturally beautiful. Practically premature. The right answer for a future hardware design where we control the BOM. Wrong for v1 on the CAT S22 Flip. File this for the open hardware track. Reject for now.

---

## Option 5: Userspace Daemon (Native, C/Rust)

### Description

Build `outstack-powerd` as a native userspace daemon. Written in C (or Rust, if we can tolerate the toolchain). Runs as a system service started by init. Communicates with the kernel via sysfs, ioctl, and cgroups. Communicates with HOME services via the system bus (C0 signals). Writes records to telux-ledgerd via its append interface.

### Pros

- **No kernel modifications.** Reads battery state from `/sys/class/power_supply/`. Reads thermal from `/sys/class/thermal/`. Controls CPU governor via `/sys/devices/system/cpu/`. All standard kernel interfaces that don't change between kernel versions.
- **No AOSP framework coupling.** Doesn't live inside system_server. Doesn't depend on Android framework APIs. If AOSP refactors its power management internally, we don't care.
- **Restartable.** If `outstack-powerd` crashes, init restarts it. The system continues. No full-system restart needed. Mode state is recoverable from the last ledger entry.
- **Minimal privilege.** Needs: read access to power_supply and thermal sysfs, write access to cpufreq governors, CAP_KILL for SIGSTOP/SIGCONT to gated processes, write access to power ledger. All scopeable with SELinux policy.
- **Testable.** Can be tested in isolation with mock sysfs. Can be unit-tested for mode transitions. Can be fuzz-tested for threshold boundary behavior. Can't do any of this easily with kernel code.
- **Portable.** sysfs interfaces are standard Linux. If we ever port to non-QM215 hardware, the daemon works unchanged as long as the sysfs paths exist (and they will — they're standardized).
- **Matches our process model.** It's another HOME daemon. It has an ASSIGN record. It has a process class (CRITICAL). It writes to the ledger like everything else. No special snowflake architecture.
- **Low overhead.** Native daemon polling sysfs every 30–120 seconds is negligible power draw. The daemon mostly sleeps.

### Cons

- **Higher latency than kernel.** sysfs reads are system calls. Not instantaneous. A sysfs read of battery percentage takes ~microseconds on QM215. For 30-second sampling intervals, this is irrelevant. For immediate thermal response, it matters — but only by microseconds.
- **SIGSTOP races.** Between detecting a mode transition and delivering SIGSTOP to all gated processes, there's a window. A process could perform an unauthorized action in that window. Mitigation: cgroup freezer is atomic — move the process class cgroup to frozen state, then send SIGSTOP for confirmation.
- **Depends on kernel not lying.** If the fuel gauge driver reports wrong battery percentage, we make wrong decisions. But this is true of every option — garbage in, garbage out regardless of where the logic lives.
- **No power-gate control?** Software can't directly control hardware power rails. We can set CPU governors and freeze processes, but we can't power-gate a WiFi chip from userspace — that needs kernel runtime PM. Mitigation: we control what runs, not the hardware power domains directly. The kernel's runtime PM handles hardware power-gating of idle peripherals. We just ensure the peripherals become idle by stopping the processes that use them.

### Verdict

This is the right answer. Maintainable, testable, portable, and architecturally consistent with HOME's daemon model. The cons are manageable. The latency concern is irrelevant at our sampling intervals. The enforcement gap is closeable with cgroup freezer as the primary mechanism and SIGSTOP as secondary/confirmation.

---

## Decision: Option 5 — Userspace Daemon

`outstack-powerd` will be a native C daemon running as a CRITICAL-class HOME system service.

### Implementation Priorities

1. Define the mode state machine (five modes, threshold-driven transitions)
2. Define the process class model (five classes, per-mode gating rules)
3. Build the sysfs reading layer (battery, thermal, CPU)
4. Build the enforcement layer (cgroup freezer + SIGSTOP/SIGCONT)
5. Build the ledger writing layer (BitPads frame emission to telux-ledgerd)
6. Build the signal emission layer (C0 MODE_ENTER signals)
7. Define the configuration interface (threshold profiles per distribution)

### What We Still Get From Kernel

- CPU frequency governor control (via sysfs)
- cgroup freezer (via cgroupfs)
- Thermal zone reporting (via sysfs)
- Battery fuel gauge reporting (via power_supply sysfs)
- Runtime PM for hardware power-gating (kernel handles this automatically for idle devices)
- RPM deep sleep coordination (kernel handles this in cpuidle governor)

### What We Don't Need From Kernel

- Custom scheduler integration
- Custom power domain framework
- Kernel module loaded at boot
- Patches to any upstream kernel subsystem

This preserves our upstream compact perfectly. The kernel is stock. Our power governance is a userspace policy layer that reads kernel-provided information and makes governance decisions.

---

## Appendix: Why Not Rust?

Considered Rust for `outstack-powerd`. Benefits: memory safety, no use-after-free in a long-running daemon. Costs: Rust cross-compilation toolchain for ARM32 is heavier than C, Rust binary size is larger (matters for the system partition budget), and the existing HOME daemon infrastructure (telux-ledgerd, telux-identd) is C. Introducing Rust for one daemon creates a toolchain split.

Decision: C for v1, with careful static analysis (Coverity or similar) and extensive fuzzing of the state machine. Revisit Rust if the daemon's complexity grows beyond what careful C can manage.

---

*Next steps: formalize the five-mode model (see: state machine design notes)*
