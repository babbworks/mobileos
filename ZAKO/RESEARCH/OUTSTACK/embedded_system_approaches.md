# Embedded System Power Approaches — Survey for Outstack Design

**Date:** 2025-02-01
**Purpose:** What can ZAKO learn from how other domains handle power governance? This surveys automotive, aerospace, industrial, and IoT approaches and maps their concepts to what became Outstack's five-mode design.

---

## 1. Automotive — AUTOSAR Power Modes

### How It Works

AUTOSAR (AUTomotive Open System ARchitecture) defines a standardized ECU (Electronic Control Unit) state management model. The EcuM (ECU Manager) module manages transitions between power states.

**AUTOSAR power states:**
- RUN — full operation
- POST_RUN — shutdown preparation, non-essential functions off
- SLEEP — CPU halted, RAM retained, wake sources active
- STARTUP — boot sequence, not yet operational
- SHUTDOWN — power rails going off

Additionally, AUTOSAR defines "partial networking" where individual ECU subsystems can be in different states. A CAN bus transceiver can be active while the main CPU sleeps — waking the CPU only when a relevant CAN message arrives.

### What's Relevant to Outstack

**The partial networking concept is directly analogous to our process class gating.** In AUTOSAR, you don't shut down the entire ECU — you shut down subsystems selectively. The CAN transceiver stays awake (CRITICAL in our terms) while the application layer sleeps (BACKGROUND). The principle: identify what must always respond, and keep only that awake.

**Run-down sequences.** AUTOSAR's transition from RUN to SLEEP isn't instantaneous — there's a defined sequence where each subsystem is notified, allowed to reach a safe state, and then shut down. This maps directly to Outstack's mode transition sequence: pre-transition assessment, signal emission, record writing, gate enforcement. AUTOSAR got this right: you can't just yank power. You need an orderly shutdown of each layer.

**Wake source management.** AUTOSAR explicitly defines which hardware events can wake the ECU from SLEEP. Only specific CAN message IDs, specific GPIO pins, or timer events qualify. Everything else is ignored. Outstack's equivalent: in Emergency mode, only CRITICAL-priority C0 signals are processed. All others are discarded. Same principle: define your wake sources explicitly, ignore everything else.

### What Doesn't Map

AUTOSAR's model is binary per-subsystem (each subsystem is either on or off). Outstack is graduated — five modes, not two states. AUTOSAR doesn't have "conservation" — it has "run" and "sleep" with not much in between. The automotive use case permits this because the car is either on (engine running, full power available from alternator) or off (parked, quiescent). A phone or field device lives in the middle most of the time.

Also: AUTOSAR has no ledger concept. Transitions happen but aren't recorded in a tamper-evident chain. The automotive assumption is that safety-critical logging happens at a different layer (diagnostic event memory / DEM). We're unifying governance and recording in one system.

---

## 2. Aerospace — Spacecraft Subsystem Budgeting

### How It Works

Spacecraft power systems operate on a fixed budget with known degradation. The power system is typically:

- **Power source:** RTG (decaying) or solar panels (varying with distance/angle)
- **Power bus:** regulated voltage rail(s) feeding all subsystems
- **Power controller:** dedicated hardware unit that monitors bus voltage/current and makes switching decisions
- **Subsystem loads:** each subsystem has a defined power allocation and a priority

The priority scheme for load shedding is defined before launch and encoded in the power controller's firmware. When available power drops below total demand, loads are shed in reverse priority order until the budget balances.

**Example (simplified Voyager-like):**
| Priority | Subsystem | Allocation |
|----------|-----------|------------|
| 1 (highest) | Flight computer + attitude control | 20W |
| 2 | Radio (transmitter) | 15W |
| 3 | Science instruments (selected) | 10W |
| 4 | Heaters (non-critical) | 8W |
| 5 (lowest) | Science instruments (secondary) | 5W |

When RTG output drops below 58W total, priority 5 is shed. Below 53W, priority 4. And so on. The shedding is deterministic, predictable, and pre-validated.

### What's Relevant to Outstack

**This is almost exactly our model.** The five process classes (CRITICAL through OPPORTUNISTIC) map to spacecraft priority levels. The five modes map to budget states — Full Power means the budget covers everything; Emergency means the budget only covers the highest priority.

**Pre-determined shedding order.** Spacecraft don't decide at runtime what to shed — it's decided during design and validated through testing. Outstack does the same: the gating rules are defined in the distribution profile, not computed dynamically. When Conservation mode activates, the system doesn't "decide" to gate BACKGROUND — it was always going to gate BACKGROUND in Conservation. The decision was made at design time.

**The power budget as a conservation quantity.** Spacecraft engineers think of power as a conserved resource: generation must equal consumption plus storage delta. If the equation doesn't balance, something is wrong. Outstack's ledger conservation invariant (the Tally must balance) is exactly this discipline applied to a mobile device.

**Redundancy in critical paths.** Spacecraft have redundant power paths for critical systems. The flight computer might have two independent power feeds. Outstack's equivalent: CRITICAL processes are never gated, and the daemon itself is CRITICAL. The power governance system cannot power-governance itself out of existence.

### What Doesn't Map

Spacecraft power budgets are *static* — the total available power is known (with degradation curves) for the entire mission. A phone's power budget is *dynamic* — it depends on charging behavior, temperature, usage patterns, and battery health. Outstack can't pre-compute a power schedule for the device's entire life. It must react to real-time measurements.

Also: spacecraft are single-purpose systems. Every subsystem is known at design time. A phone runs arbitrary applications. Outstack must handle processes it's never seen before (a newly installed app), whereas a spacecraft never encounters a new subsystem post-launch.

---

## 3. Industrial PLCs — Fail-Safe Power Management

### How It Works

Programmable Logic Controllers in industrial settings (manufacturing plants, water treatment, power generation) use a different power philosophy: fail-safe over fail-operational.

A PLC's power management priorities:
1. **Maintain safe state.** If power fails, outputs go to their defined safe state (valves close, motors stop, alarms trigger). This is hardware-enforced — relay outputs de-energize on power loss, which is inherently safe for "normally open" circuits.
2. **Orderly shutdown.** If UPS battery is depleting, the PLC writes its current state to non-volatile memory and initiates a controlled process shutdown sequence.
3. **No intermediate modes.** PLCs are typically binary: running or stopped. There's no "conservation mode" for a PLC controlling a chemical process — you either have enough power to control the process safely, or you shut it down entirely.

Power monitoring in industrial contexts:
- UPS battery monitoring via SNMP or Modbus
- Power supply redundancy (N+1 supplies, hot-swap capable)
- Power sequencing on boot (rails come up in order: control power → I/O power → field power)

### What's Relevant to Outstack

**Safe state as a design requirement.** Outstack's Emergency mode needs a defined "safe state" — what does the device look like when only CRITICAL processes run? For a phone: modem paging, identity daemon, ledger daemon. For an IoT device: whatever constitutes safe operation for that deployment. The PLC discipline of defining safe state explicitly, before deployment, is correct. Outstack should require each distribution to define its Emergency safe state.

**State snapshot before power loss.** PLCs write state to NVRAM before shutdown. Outstack's shutdown sequence does the same: final POWER_CHANGE record, final CLOSE record, telux-ledgerd flush. The principle: power loss should never mean data loss. Whatever was known at the last good moment must be recoverable.

**Power sequencing.** PLCs bring up power rails in a defined order. Outstack's startup sequence is analogous: read battery state *first*, determine mode *second*, enforce gating *third*. The order matters — you must know the power situation before you can govern anything.

### What Doesn't Map

PLCs don't degrade gracefully — they're either operational or shut down. Outstack's whole value proposition is graduated degradation: five modes, each successively more restrictive but still functional. The PLC binary model is too coarse for a general-purpose device where "doing less" is better than "doing nothing."

Also: PLCs have no concept of user sovereignty over the power policy. The engineer programs the PLC; the PLC follows the program. Outstack has a sovereign who can override thresholds within bounds. The governance is sovereign-mediated, not engineer-dictated.

---

## 4. IoT Sensor Networks — Duty Cycling and Energy Harvesting

### How It Works

Low-power IoT sensor networks (think: LoRaWAN sensor nodes, Zigbee mesh nodes, agricultural soil sensors) use a radically different approach: duty cycling.

The node sleeps >99% of the time. Periodically (every 1s to every 1h depending on application), it wakes, takes a measurement, optionally transmits, and goes back to sleep. The "power management" is the duty cycle ratio itself.

**Power budget model:**
- Sleep: ~1µA (RTC only)
- Active: ~10-50mA (CPU + sensor + radio)
- Transmit: ~100-150mA (radio Tx burst)
- Total daily budget: determined by (sleep_current × sleep_time) + (active_current × active_time) + (tx_current × tx_time)

Energy harvesting adds a variable income stream:
- Solar: proportional to light exposure
- Vibration: proportional to environmental motion
- Thermal gradient: proportional to temperature differential

The node's energy management system tracks "energy neutral operation" — the goal is that harvested energy over a period equals consumed energy over the same period. If the energy balance goes negative, the node extends sleep intervals (reducing the duty cycle) to restore balance.

### What's Relevant to Outstack

**Energy balance as a signal.** This is the "solar energy balance" concept I was exploring for field devices. IoT nodes formalize it: track income vs. expenditure over a rolling window. If expenditure exceeds income, tighten the duty cycle. This maps to Outstack's mode model: if the device is net-discharging faster than expected, tighten the mode.

**Adaptive duty cycling maps to adaptive sampling intervals.** Outstack already varies its sampling interval by mode (30s in Full, 120s in Conservation, 300s in Critical). IoT nodes do the same thing — they sample more frequently when power is abundant and less frequently when power is scarce. Same principle, similar implementation.

**Energy neutrality as a conservation target.** The IoT goal of "energy neutral operation" maps to Outstack's ledger conservation invariant. Over a sufficiently long period (a day, a week), the power ledger should balance: energy in ≈ energy out (with degradation losses accounted for as a known quantity). If it doesn't balance, something is wrong.

**Structured degradation.** IoT networks define what degrades as power gets scarce:
1. First: reduce measurement frequency
2. Then: disable non-critical sensors
3. Then: extend transmission interval
4. Finally: enter deep sleep for extended period, wake only for critical events

This maps to Outstack's process class gating:
1. First: gate OPPORTUNISTIC (Full→Standard)
2. Then: gate/defer BACKGROUND (Standard→Conservation)
3. Then: gate INTERACTIVE (Conservation→Critical Reserve)
4. Finally: gate everything except CRITICAL (Critical Reserve→Emergency)

The mapping is direct. IoT networks validated this approach for sensor nodes; Outstack applies it to general-purpose devices.

### What Doesn't Map

IoT sensor nodes are single-purpose. They don't have users. They don't have multiple applications competing for power. Their "process class" is just the duty cycle ratio — there's only one process (sense-transmit) and the question is how often it runs.

Also: IoT nodes typically have no UI, no interactive component, no concept of user-initiated action that takes priority. The INTERACTIVE process class — "the user is doing something right now, don't gate it" — doesn't exist in the sensor network model.

---

## Synthesis: How These Map to Outstack's Five-Mode Design

| Outstack Concept | Automotive | Aerospace | Industrial | IoT |
|-----------------|------------|-----------|------------|-----|
| Five modes | RUN/SLEEP (binary) | N/A (continuous budget) | RUN/STOP (binary) | Adaptive duty cycle (continuous) |
| Process classes | Subsystem on/off | Priority-ordered loads | N/A (single purpose) | Single workload |
| Gating mechanism | ECU state transitions | Load shedding by priority | Safe-state outputs | Duty cycle extension |
| Ledger/records | DEM (diagnostic events) | None (telemetry only) | NVRAM state snapshots | None |
| Thermal response | Defined derating curves | Heater/shade control | Environmental compensation | N/A (low power = low heat) |
| Sovereignty | None (engineer-programmed) | None (ground-commanded) | None (engineer-programmed) | None (designer-configured) |

### What Outstack Uniquely Adds

1. **Graduated modes** — not binary (on/off) but five levels of graceful degradation
2. **Recording as a first-class concern** — power events in a tamper-evident ledger
3. **Sovereignty** — the user can inspect and (within bounds) override the governance
4. **Multi-application governance** — handles competing processes, not just single-purpose workloads
5. **Thermal as a mode trigger** — not just a derating factor but an independent path to Emergency mode

### Design Decisions Informed by This Survey

1. **Pre-determined shedding order** (from aerospace): Gating rules are declared at distribution build time, not computed at runtime. Deterministic behavior.
2. **Safe state definition** (from industrial): Every distribution must define what Emergency mode looks like for that device. What processes run? What hardware domains are energized?
3. **Orderly transition with notification** (from automotive): Mode transitions notify all affected services and give them time to reach safe suspension points before gating is enforced.
4. **Adaptive sampling** (from IoT): Sampling interval varies with mode. More frequent sampling when power is abundant provides better data; less frequent sampling in Critical Reserve reduces overhead.
5. **Conservation invariant** (from aerospace + IoT): The ledger must balance. Power in minus power out equals state delta. If it doesn't balance, flag it.

---

## Appendix: Terminology Mapping

| Outstack Term | Automotive Equiv | Aerospace Equiv | Industrial Equiv | IoT Equiv |
|--------------|-----------------|-----------------|-----------------|-----------|
| Mode | ECU state | Power budget level | Operating state | Duty cycle tier |
| Process class | Subsystem | Load priority | N/A | N/A |
| Gate | Subsystem disable | Load shed | Safe-state transition | Sleep extension |
| Restore | Wake/enable | Load restore | Process restart | Duty cycle increase |
| CRITICAL | Wake-source subsystem | Flight-critical load | Safety function | N/A |
| Emergency | SLEEP | Minimum bus voltage | Emergency stop | Maximum sleep |

---

*This survey confirms: Outstack's architecture is not novel in concept — it's novel in application domain (general-purpose mobile device) and in the recording discipline (ledger integration). The patterns are well-validated across multiple industries.*
