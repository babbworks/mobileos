# Outstack Protocol
## Version 1.0

*June 1, 2026*

---

> A device that cannot govern its own power cannot guarantee its own availability. Availability is a sovereignty property. Outstack is the system that makes it real: it treats power not as a hardware constraint to be managed by the kernel but as a resource to be accounted for, recorded, and governed by the Sovereign. Every mode transition is a record. Every gate applied is a record. Every restoration is a record. The power budget is a ledger.

---

## 1. Purpose and Scope

This document is a sub-protocol of the HOME Standard. It defines the Outstack power governance system: its mode model, process class model, gate mechanism, ledger integration, daemon contracts, and distribution configuration interface.

Outstack governs which processes may execute at what resource level, based on the current power state of the device. It does not replace the operating system's power management — it operates above it, translating hardware power state into governance decisions that HOME services are contractually obligated to honour.

The following are in scope:
- The five Outstack power modes and their definitions
- The five process classes and their gating rules per mode
- The power ledger: how power events are recorded as BitPads frames
- The `outstack-powerd` daemon and its contracts with HOME services
- C0 signal conventions for power communication
- Hardware abstraction: battery, thermal, CPU, and radio inputs
- Distribution configuration: threshold profiles

The following are out of scope:
- Kernel power management internals (DVFS, cpuidle, wake locks)
- Android power HAL implementation details
- Battery hardware specifications

---

## 2. The Outstack Power Model

### 2.1 Core Principle

Every device has a power budget. That budget is not infinite, and in a mobile or edge device it is actively depleting. Most operating systems treat this as a hardware problem — manage it in the kernel, expose a battery percentage to applications, and let each application decide what to do. The result is competing draws on a shared resource with no coordination.

Outstack treats the power budget as a ledger. Processes draw from it. When the budget is low, draws are governed. When the budget is critical, only essential draws are permitted. The governance rules are not suggestions — they are enforced by `outstack-powerd`, which holds gating authority over process class execution through the HOME capability system.

### 2.2 Power Sources Tracked

Outstack tracks four power source dimensions:

| Dimension | Measurement | Signal |
|-----------|-------------|--------|
| Battery | Percentage (0–100), Voltage (mV) | Primary budget signal |
| Thermal | Device temperature (°C), CPU thermal zone | Rate-of-draw signal |
| CPU draw | Instantaneous wattage by process class | Draw accounting signal |
| Charging | Rate (mW), source type (USB, wireless, solar) | Budget replenishment signal |

Each dimension is sampled by `outstack-powerd` at the configured sampling interval (default: 30 seconds in Full Power mode; 60 seconds in Conservation; 120 seconds in Critical Reserve). A significant change in any dimension may trigger an immediate re-evaluation of the current power mode.

---

## 3. The Five Power Modes

Outstack operates in exactly one of five power modes at any time. Mode determines which process classes are permitted to execute. Transitions between modes are governed by threshold rules defined in the distribution's Outstack profile.

### 3.1 Mode Table

| Mode | Code | battery% threshold | Process Classes Permitted |
|------|------|--------------------|--------------------------|
| Full Power | 0x0 | > STANDARD_FLOOR (default 50%) | ALL |
| Standard | 0x1 | > CONSERVE_FLOOR (default 20%) | CRITICAL, INTERACTIVE, BACKGROUND |
| Conservation | 0x2 | > LOW_FLOOR (default 10%) | CRITICAL, INTERACTIVE, BACKGROUND (deferred) |
| Critical Reserve | 0x3 | > SHUTDOWN_FLOOR (default 3%) | CRITICAL, INTERACTIVE |
| Emergency | 0x4 | ≤ SHUTDOWN_FLOOR or thermal_critical | CRITICAL only |

### 3.2 Mode Definitions

**Full Power (0x0):**  
No process class restrictions. All HOME services operate at full capability. Background syncs, Academy learning batches, and Learning Engine inference all run. Outstack sampling is at the minimum interval. This mode persists as long as battery exceeds STANDARD_FLOOR or the device is charging.

**Standard (0x1):**  
OPPORTUNISTIC processes are gated. Learning Engine background inference is suspended. All user-initiated and system-critical services continue. This is the default mode during normal device use on battery. Most users spend most of their time here.

**Conservation (0x2):**  
BACKGROUND processes are deferred. This means: PADS background sync, health data aggregation, Academy progress updates, Academy recommendation generation, social feed updates, and environment sensor logging all pause. CRITICAL and INTERACTIVE continue without restriction. BACKGROUND processes that were mid-execution are allowed to reach their next safe checkpoint, then suspend. Records in-flight are preserved to staging; posting resumes when the mode lifts.

**Critical Reserve (0x3):**  
BACKGROUND and OPPORTUNISTIC are fully gated. Only CRITICAL and INTERACTIVE processes run. In practice: Exchange Engine, telux-ledgerd, telux-identd, and user-initiated actions continue. All periodic processing stops. The device is conserving power for core communication and recording capability. This is when the Sovereign still needs to make payments, receive messages, and record events — just nothing more.

**Emergency (0x4):**  
Only CRITICAL processes run. INTERACTIVE processes are gated. The Sovereign cannot initiate new UI actions without them being downgraded or queued. The device is preparing for power loss. Emergency mode is temporary by design: the device either resumes charging (exiting Emergency) or powers off. Outstack takes a final state snapshot record before permitting device power-down.

### 3.3 Mode Transitions

Mode transitions are not instantaneous. Outstack follows this sequence on any transition:

1. `outstack-powerd` detects a threshold crossing or triggering condition
2. A pre-transition assessment confirms the condition (one additional sample to avoid flip-flopping on threshold boundary)
3. `outstack-powerd` emits a `MODE_ENTER` C0 signal (slot 0x03) with the new mode encoded in category bits
4. A `POWER_CHANGE` record (task_code=0x00, `account_pair=0001` Engineering domain — Parent→Child allocation) is written to the System sub-entity (sub_entity=0, file_sep=0) by `outstack-powerd`
5. `outstack-powerd` updates its internal mode state and begins enforcing the new class gates
6. All HOME daemons listening on the system bus receive the mode signal and update their own execution policy

Steps 3–5 are atomic from `outstack-powerd`'s perspective. A daemon that receives the MODE_ENTER signal can be certain the record has already been posted before it acts on the signal.

### 3.4 Hysteresis

To prevent mode oscillation on a boundary (battery hovering at exactly 20%), Outstack applies hysteresis to downward transitions (entering a more restrictive mode) and upward transitions (relaxing to a less restrictive mode):

- **Downward:** trigger at threshold; resume only when battery exceeds threshold + HYSTERESIS_BAND (default 3%)
- **Upward:** trigger at threshold; downgrade only when battery falls below threshold - HYSTERESIS_BAND

This means: if CONSERVE_FLOOR is 20%, Conservation mode begins at 20%, and Standard mode resumes at 23%. A device sitting at 21% does not oscillate between Standard and Conservation.

---

## 4. The Five Process Classes

Every HOME process — daemon, service, agent, background task — declares a process class. The process class determines which Outstack modes permit its execution.

### 4.1 Class Table

| Class | Code | Permitted In Modes | Description |
|-------|------|--------------------|-------------|
| CRITICAL | 0x4 | All modes including Emergency | Core ledger, identity, exchange engine |
| INTERACTIVE | 0x3 | Full, Standard, Conservation, Critical Reserve | User-initiated operations, UI services |
| BACKGROUND | 0x2 | Full, Standard only (deferred in Conservation) | Periodic sync, health aggregation, PADS background |
| DEFERRED | 0x1 | Full, Standard only | Non-urgent updates; may be multi-day deferred |
| OPPORTUNISTIC | 0x0 | Full Power only | Learning Engine inference, Academy AI, speculative pre-fetch |

### 4.2 Class Definitions

**CRITICAL:**  
The process must run regardless of power state. CRITICAL processes include `telux-ledgerd` (the append-only ledger daemon), `telux-identd` (identity and signing), and the Exchange Engine's core settlement loop. CRITICAL processes may not be suspended by Outstack. If `outstack-powerd` itself determines that a CRITICAL process is drawing power at an unsustainable rate in Emergency mode, it may reduce the process's CPU scheduling priority — but it may not gate its execution.

**INTERACTIVE:**  
The process runs in response to Sovereign actions. Every service that presents UI, responds to user input, or mediates an exchange initiated by the user is INTERACTIVE. Gated in Emergency mode only. In Critical Reserve, INTERACTIVE processes run but are not permitted to spawn BACKGROUND sub-processes. An INTERACTIVE process that would normally trigger a BACKGROUND follow-on (e.g., sync after a record write) must either perform that follow-on synchronously at INTERACTIVE priority or drop it and log the deferral.

**BACKGROUND:**  
Periodic, non-urgent work. BACKGROUND processes declare a preferred execution window and a maximum acceptable deferral. `outstack-powerd` schedules them within those constraints, subject to the current mode. In Conservation mode, BACKGROUND processes are deferred to the next window when the mode lifts or when the device begins charging. A BACKGROUND process must expose a safe suspension point — a point at which it can be cleanly interrupted with no data loss. `outstack-powerd` targets suspension points, not arbitrary interruption.

**DEFERRED:**  
Work that can wait multiple execution cycles. Academy progress synchronisation, social graph updates, environment data archiving. DEFERRED processes may wait hours or days. They declare a maximum acceptable staleness (e.g., "this work must run at least once every 72 hours"). `outstack-powerd` honours this constraint and guarantees execution when the mode permits, within the declared window.

**OPPORTUNISTIC:**  
Work that benefits from spare resources but creates no harm if never run. Learning Engine speculative inference, Academy recommendation pre-generation, speculative cache warming. OPPORTUNISTIC work runs only in Full Power mode and only when the device is charging or has battery > OPPORTUNISTIC_FLOOR (default 70%). It is the first class suspended on any downward transition.

### 4.3 Class Declaration

A process declares its class in the ASSIGN record written to the System sub-entity when the process is registered with `outstack-powerd`:

```
task_code  = 0x05     ASSIGN
account_pair = 0001   (Parent → Child; Outstack → process)
domain     = 01       Engineering
value      = process_class_code (0x0–0x4)
sender_id  = outstack-powerd
sub_entity = 0        System sub-entity
```

A process that attempts to execute without a registered ASSIGN record is treated as BACKGROUND by `outstack-powerd` until registration completes. A process that attempts to execute a CRITICAL action without a CRITICAL class registration has that action queued at INTERACTIVE priority.

---

## 5. The Gate Mechanism

### 5.1 How Gates Work

A gate is the capability withdrawal that prevents a process from executing beyond its permitted class. `outstack-powerd` issues gates through the HOME capability system (managed by `telux-identd`): when a mode transition would gate a process class, `outstack-powerd` submits a REVOKE record targeting the execution capability of all processes in that class.

The gate is recorded. The REVOKE record in the Identity sub-entity (sub_entity=1) is the authoritative proof that the gate was applied:

```
task_code   = 0x09   REVOKE
account_pair = 0110  (Reservation/Escrow; capability reserved, not flowing)
domain      = 10     Hybrid (identity + power)
value       = gated_class_bitmask
sender_id   = outstack-powerd sovereign id
sub_entity  = 1      Identity sub-entity
```

### 5.2 Gate Record

Every gate applied by Outstack produces a GATE record in the System sub-entity (sub_entity=0):

```
task_code   = 0x02   GATE
account_pair = 0001  (Parent → Child; Outstack → process class)
domain      = 01     Engineering
value       = gated_class_bitmask (bitmask over the 5 process classes)
sender_id   = outstack-powerd
sub_entity  = 0
wall_ts     = gate_epoch
```

The bitmask encodes which classes are gated. This record is written in the same atomic operation as the MODE_ENTER signal — they share the same wall_ts and the same lamport_ts increment in telux-ledgerd.

### 5.3 Restoration

When a mode transitions upward (to a less restrictive mode), `outstack-powerd` issues a RESTORE record:

```
task_code   = 0x03   RESTORE
account_pair = 0111  (Repayment/Return; capability returned)
domain      = 01     Engineering
value       = restored_class_bitmask
sender_id   = outstack-powerd
sub_entity  = 0
```

RESTORE records trigger capability re-issuance by `telux-identd`: a new GRANT record in the Identity sub-entity restores execution permission. BACKGROUND and OPPORTUNISTIC processes that were deferred resume their declared execution windows. The order of resumption is:

1. INTERACTIVE (always first)
2. BACKGROUND processes in order of maximum staleness (most overdue first)
3. DEFERRED processes in order of maximum staleness
4. OPPORTUNISTIC processes if power conditions permit

---

## 6. The Power Ledger

### 6.1 Power Events as Records

Every significant Outstack event is a BitPads frame written to the System sub-entity (file_sep=0, sub_entity=0) of the Personal Island. This makes power history part of the same tamper-evident chain as all other HOME records.

**Standard power event record types:**

| task_code | Verb | Occasion | account_pair |
|-----------|------|----------|-------------|
| 0x00 | POWER_CHANGE | Battery level change exceeding SAMPLE_THRESHOLD | 0000 (Source→Sink; battery→device) |
| 0x01 | MODE_CHANGE | Any Outstack mode transition | 0001 (Parent→Child) |
| 0x02 | GATE | Process class gated | 0001 |
| 0x03 | RESTORE | Process class restored | 0111 (Repayment) |
| 0x04 | CALIBRATE | Battery calibration event (full charge cycle complete) | 0000 |
| 0x05 | ASSIGN | Process class assignment | 0001 |
| 0x06 | SUSPEND | Specific process suspended | 0110 (Reservation) |
| 0x07 | RESUME | Specific process resumed | 0111 |

### 6.2 POWER_CHANGE Record Detail

POWER_CHANGE records are the most frequent power ledger entries. They are written when battery percentage changes by more than SAMPLE_THRESHOLD (default: 5%) since the last POWER_CHANGE record. In Emergency mode, threshold drops to 1%.

```
Frame type: Full Record (13–29 bytes)
domain:     01 (Engineering)
quantity_type: 0x04 (Loss/Dissipation — power leaving the battery)
  [or 0x05 Generation/Input if device is charging]
value:      battery_percentage × 100 (integer; e.g., 3750 = 37.50%)
direction:  1 (Inflow) if charging; 0 (Outflow) if discharging
task_code:  0x00 (POWER_CHANGE)
account_pair: 0000 (Source→Sink)
wall_ts:    epoch seconds
sub_entity: 0 (System)
```

The direction bit (Outflow/Inflow) records whether the battery is charging or discharging at the time of the sample. A sequence of POWER_CHANGE records with direction=0 (Outflow) traces the power draw across a session. A transition to direction=1 (Inflow) marks the onset of charging.

### 6.3 Tally Records in the Power Ledger

Like all HOME domains, the power ledger supports period-close Tally records. An Outstack Tally records the aggregate draw over a defined period:

```
task_code:    0x15   COMMIT
account_pair: 1101   State Commit
group_sep:    63     Period Close
domain:       01     Engineering
value:        net_mwh_drawn (total milliwatt-hours consumed in period)
sub_entity:   0      System
```

The Tally value is the signed sum of all power flows in the period: positive values represent net discharge (the device consumed more than it charged); negative values represent net charge (the device gained more than it spent). The conservation invariant applies: the ledger must balance. `outstack-powerd` computes the period balance before writing the Tally; if the computed balance differs from the value field, the Tally is rejected by telux-ledgerd.

### 6.4 Mode Transition Record

A MODE_CHANGE record carries both the previous and new mode:

```
Frame type:  Full Record
task_code:   0x01   MODE_CHANGE
domain:      01     Engineering
account_pair: 1000  (Transformation; mode state changes form within system)
value:       (previous_mode << 4) | new_mode  (both encoded in one byte)
sub_entity:  0
```

The value encoding allows any decoder to recover both modes from one field: upper nibble = previous mode; lower nibble = new mode. Mode values: 0=Full, 1=Standard, 2=Conservation, 3=Critical Reserve, 4=Emergency.

---

## 7. The outstack-powerd Daemon

### 7.1 Role and Authority

`outstack-powerd` is the HOME power governance daemon. It is a CRITICAL process. It has the following exclusive authorities:

- Issue MODE_ENTER C0 signals to the system bus
- Write GATE and RESTORE records to the System sub-entity
- Issue process class capability grants and revocations through `telux-identd`
- Read power hardware state from the Android power HAL or equivalent hardware abstraction layer
- Configure sampling intervals and threshold profiles at runtime

No other HOME process may write to the power ledger (System sub-entity, task codes 0x00–0x07) without a delegation record from `outstack-powerd`. Attempts by other processes to write power records without delegation are rejected by `telux-ledgerd`.

### 7.2 Startup Sequence

On HOME system start, `outstack-powerd` follows this initialisation sequence:

1. Read current battery state from hardware
2. Determine initial Outstack mode from battery percentage against threshold profile
3. Write an OPEN record to the System sub-entity (task_code=0x10 OPEN; this marks the session boundary in the power ledger)
4. Write a POWER_CHANGE record with the current battery state
5. Write a MODE_CHANGE record if the determined initial mode differs from the last stored mode at prior shutdown
6. Emit a MODE_ENTER C0 signal to the system bus with the current mode
7. Begin sampling loop

### 7.3 Shutdown Sequence

On system shutdown (clean), `outstack-powerd` follows:

1. Detect shutdown signal
2. Write a final POWER_CHANGE record with current battery state
3. Write a SUSPEND record for each BACKGROUND and OPPORTUNISTIC process that was active
4. Write a CLOSE record to the System sub-entity (task_code=0x11 CLOSE; marks power session end)
5. If battery state warrants it, write an Emergency Tally (Tally record with Emergency mode flag)
6. Signal `telux-ledgerd` to flush and sync the ledger before shutdown proceeds
7. Permit shutdown to proceed only after telux-ledgerd confirms flush

In Emergency mode, the shutdown sequence is condensed: steps 3 and 5 are skipped to minimise time before power loss. Only the POWER_CHANGE, CLOSE, and `telux-ledgerd` flush are guaranteed.

### 7.4 Sampling Loop

The sampling loop is `outstack-powerd`'s primary activity:

```
every SAMPLE_INTERVAL seconds:
  1. Read battery percentage, voltage, temperature from HAL
  2. Read instantaneous CPU draw by process class from power HAL
  3. If battery_delta > SAMPLE_THRESHOLD since last POWER_CHANGE record:
       write POWER_CHANGE record
  4. Evaluate mode transition conditions:
       if battery_pct crosses a threshold (with hysteresis):
         execute mode transition sequence (§3.3)
  5. If thermal_zone > THERMAL_CRITICAL:
       if not already in Emergency mode:
         execute mode transition to Emergency regardless of battery level
  6. Emit system heartbeat signal (C0 slot 0x13) with system health index
  7. Sleep until next SAMPLE_INTERVAL
```

### 7.5 Thermal Override

Thermal state can trigger Emergency mode independently of battery level. If the device temperature exceeds THERMAL_CRITICAL (default: 45°C sustained for 60 seconds), `outstack-powerd` enters Emergency mode regardless of battery percentage. This is recorded as:

```
task_code:    0x01   MODE_CHANGE
account_pair: 1000   (Transformation)
value:        (prior_mode << 4) | 0x4   (Emergency = 0x4)
note:         "thermal_override: <temp>°C"
```

Thermal Emergency exits when temperature drops below THERMAL_SAFE (default: 40°C sustained for 120 seconds). The mode entered on thermal recovery is determined by the current battery percentage against the normal threshold profile — not necessarily the mode before the thermal override.

---

## 8. Service Contracts

### 8.1 Required Behaviour for CRITICAL Services

A service registered as CRITICAL must:

1. Maintain a safe suspension state at all times — a snapshot of its current work that can be preserved if hardware power loss occurs between sampling intervals
2. Respond to `outstack-powerd` capability re-issuance within 500ms (registration of the ASSIGN record must complete within this window)
3. Not cache pending records for more than one `telux-ledgerd` write cycle — records must be committed promptly, not batched indefinitely

### 8.2 Required Behaviour for INTERACTIVE Services

A service registered as INTERACTIVE must:

1. Respect INTERACTIVE priority signals from `outstack-powerd`: if a MODE_ENTER signal arrives mid-operation, complete the current atomic operation and then apply the new mode policy
2. Not initiate BACKGROUND sub-processes in Conservation, Critical Reserve, or Emergency modes
3. Expose a user-visible indicator of power mode to the Sovereign in Critical Reserve and Emergency modes

### 8.3 Required Behaviour for BACKGROUND Services

A service registered as BACKGROUND must:

1. Declare a suspension point: a method or signal that `outstack-powerd` can invoke to trigger clean suspension
2. Declare maximum acceptable deferral in its ASSIGN record (encoded as value in hours, up to 255)
3. On suspension, write any staged records to a local queue and confirm suspension to `outstack-powerd` within 2 seconds
4. On resumption (RESTORE signal received), resume from the staged queue before processing new work

### 8.4 Required Behaviour for DEFERRED and OPPORTUNISTIC Services

A service registered as DEFERRED or OPPORTUNISTIC must:

1. Tolerate indefinite non-execution — these processes have no guaranteed execution window in Conservation mode or below
2. Not write to `telux-ledgerd` while gated; staging is permitted but posting is not
3. Not signal users or generate ALERT-class signals while in gated state — their gating is invisible to the Sovereign unless the Sovereign explicitly queries power mode status

---

## 9. Distribution Configuration Profile

### 9.1 The Outstack Profile

A HOME distribution configures Outstack through an Outstack Profile: a set of threshold values and behavioural parameters. The profile is stored in the distribution's system partition and is not modifiable at runtime without a distribution update. The Sovereign may override individual thresholds within the distribution-defined override range.

**Required profile parameters:**

| Parameter | Default | Description | Sovereign Override |
|-----------|---------|-------------|-------------------|
| STANDARD_FLOOR | 50% | Battery % below which Standard mode activates | 30%–70% |
| CONSERVE_FLOOR | 20% | Battery % below which Conservation activates | 10%–35% |
| LOW_FLOOR | 10% | Battery % below which Critical Reserve activates | 5%–20% |
| SHUTDOWN_FLOOR | 3% | Battery % below which Emergency activates | 1%–8% |
| HYSTERESIS_BAND | 3% | Band above threshold for upward transition | 1%–10% |
| OPPORTUNISTIC_FLOOR | 70% | Battery % floor for Opportunistic processes when on battery | 50%–90% |
| SAMPLE_INTERVAL_FULL | 30s | Sampling interval in Full Power mode | 15s–120s |
| SAMPLE_INTERVAL_STD | 60s | Sampling interval in Standard mode | 30s–180s |
| SAMPLE_INTERVAL_CONS | 120s | Sampling interval in Conservation mode | 60s–300s |
| SAMPLE_INTERVAL_CRIT | 300s | Sampling interval in Critical Reserve mode | 120s–600s |
| SAMPLE_INTERVAL_EMRG | 30s | Sampling interval in Emergency mode (frequent — tracking discharge rate) | 15s–60s |
| SAMPLE_THRESHOLD | 5% | Battery delta that triggers POWER_CHANGE record | 1%–10% |
| THERMAL_CRITICAL | 45°C | Temperature triggering thermal Emergency override | 40°C–55°C |
| THERMAL_SAFE | 40°C | Temperature for thermal Emergency recovery | 35°C–48°C |

### 9.2 Charging Behaviour Overrides

When the device is charging, Outstack applies a charging-adjusted mode policy:

- If charging rate > FAST_CHARGE_THRESHOLD (default: 10W), Outstack operates one mode better than battery percentage would indicate. A device at 15% battery charging at 15W operates in Conservation mode rather than Critical Reserve.
- If charging rate > FAST_CHARGE_THRESHOLD and battery > 30%, Outstack operates in Standard mode regardless of battery percentage.
- These adjustments are not applied in Emergency mode; Emergency exits only via the standard battery threshold recovery path.

Charging state is recorded in POWER_CHANGE records via the direction bit (direction=1 = charging).

### 9.3 Constrained Hardware Profiles

For hardware without fine-grained power telemetry (e.g., low-end Android devices without per-process CPU power measurement), the distribution may declare a CONSTRAINED_POWER profile. In this profile:

- Per-process CPU draw measurement is unavailable; process class gating is based on mode only, not measured draw
- SAMPLE_INTERVAL values are doubled (coarser sampling to reduce the sampling overhead itself)
- The GATE and RESTORE records still write to the power ledger; the value field carries the mode transition code rather than a measured draw figure
- Thermal override remains active if hardware provides temperature sensors; if no temperature sensor is available, THERMAL_CRITICAL is never triggered

---

## 10. Integration with HOME Wire Conventions

### 10.1 C0 Signal Priority Remapping

As defined in HOME Wire Conventions §9.1, priority bits in the Meta byte map to Outstack process classes:

| Priority Bits | Process Class |
|---------------|--------------|
| `11` | CRITICAL |
| `10` | INTERACTIVE |
| `01` | BACKGROUND |
| `00` | OPPORTUNISTIC |

Outstack enforces this mapping in receiving: a frame arriving with priority bits `00` (OPPORTUNISTIC) in Emergency mode is not processed by receiving daemons. `outstack-powerd` instructs daemons to discard OPPORTUNISTIC-priority frames when in Emergency mode. The frames are not stored; they are not ledger entries. This is the correct behaviour: OPPORTUNISTIC work, by definition, is work that can be discarded without harm.

CRITICAL frames (`11`) are never discarded by Outstack. If a daemon is in a gated state and receives a CRITICAL-priority frame, it buffers the frame and processes it immediately on any mode change that permits its class — but if the process class is CRITICAL, it processes immediately regardless of mode.

### 10.2 Power Signals in Transmissions

When `outstack-powerd` emits a MODE_ENTER signal to another HOME device over a transmission channel (for multi-device scenarios), it uses the C0 signal convention defined in Wire Conventions §8.2:

```
C0 slot 0x03 (MODE_ENTER):
  Priority = 10   (INTERACTIVE — mode changes are user-relevant)
  ACK      = 0    (No acknowledgement required; broadcast)
  Category = new_mode (0x0–0x4)
```

The receiving device's `outstack-powerd` processes this as advisory: it does not change the receiving device's own power mode (which is governed by its own battery), but it adjusts the priority ceiling applied to incoming records from the signalling device. Records arriving from a device in Emergency mode are treated as high-priority regardless of their declared priority bits.

---

## 11. Conformance Requirements

A HOME distribution conforms to this document when:

1. **Five modes are implemented.** The distribution's `outstack-powerd` maintains exactly five modes (Full, Standard, Conservation, Critical Reserve, Emergency) with the defined gating rules for each process class.

2. **All seven record types are written.** Power ledger records for POWER_CHANGE, MODE_CHANGE, GATE, RESTORE, CALIBRATE, ASSIGN, SUSPEND, and RESUME are produced correctly with the defined task_code, account_pair, domain, and value encoding.

3. **Mode transitions produce signals before records.** The MODE_ENTER C0 signal is emitted before the corresponding MODE_CHANGE record is written. Daemons may rely on this ordering.

4. **Hysteresis is enforced.** Mode transitions do not oscillate. The HYSTERESIS_BAND is applied in both directions.

5. **Thermal override is implemented.** Emergency mode is triggered by thermal conditions regardless of battery level, provided temperature sensors are available. Distributions with CONSTRAINED_POWER profiles are exempt if no sensor is available.

6. **CRITICAL processes are never gated.** No gate record targets a CRITICAL-class process. `telux-ledgerd` and `telux-identd` continue to operate in all modes.

7. **Shutdown sequence preserves ledger integrity.** `outstack-powerd` does not permit device shutdown before `telux-ledgerd` confirms flush. In Emergency mode, the condensed sequence still requires this confirmation.

8. **Tally conservation is enforced.** Power ledger Tally records are computed and verified against the period balance before posting. A Tally whose value does not match the computed period net is rejected.

9. **Distribution profile is documented.** Every HOME distribution publishes its Outstack profile parameters. Sovereign override ranges must not be narrower than the defaults defined in §9.1.

---

## Appendix A: Power Mode Quick Reference

```
Mode 0x0  Full Power      ALL classes permitted        battery > STANDARD_FLOOR
Mode 0x1  Standard        CRITICAL + INTERACTIVE + BACKGROUND   > CONSERVE_FLOOR
Mode 0x2  Conservation    CRITICAL + INTERACTIVE (BACKGROUND deferred)   > LOW_FLOOR
Mode 0x3  Critical Reserve CRITICAL + INTERACTIVE only  > SHUTDOWN_FLOOR
Mode 0x4  Emergency       CRITICAL only                 ≤ SHUTDOWN_FLOOR or thermal
```

## Appendix B: Process Class Execution Matrix

```
                 Full   Standard  Conserve  CritRsv  Emergency
CRITICAL         RUN    RUN       RUN       RUN      RUN
INTERACTIVE      RUN    RUN       RUN       RUN      GATE
BACKGROUND       RUN    RUN       DEFER     GATE     GATE
DEFERRED         RUN    RUN       GATE      GATE     GATE
OPPORTUNISTIC    RUN*   GATE      GATE      GATE     GATE

* Only when battery > OPPORTUNISTIC_FLOOR or device is charging
```

## Appendix C: outstack-powerd System Record Summary

| Record | task_code | account_pair | Value Field | sub_entity |
|--------|-----------|-------------|-------------|------------|
| POWER_CHANGE | 0x00 | 0000 | battery_pct × 100 | 0 |
| MODE_CHANGE | 0x01 | 1000 | (prev_mode << 4) \| new_mode | 0 |
| GATE | 0x02 | 0001 | gated_class_bitmask | 0 |
| RESTORE | 0x03 | 0111 | restored_class_bitmask | 0 |
| CALIBRATE | 0x04 | 0000 | calibration_reference_mV | 0 |
| ASSIGN | 0x05 | 0001 | process_class_code (0x0–0x4) | 0 |
| SUSPEND | 0x06 | 0110 | process_identifier | 0 |
| RESUME | 0x07 | 0111 | process_identifier | 0 |

---

*Outstack Protocol v1.0 — June 1, 2026*  
*Sub-protocol of HOME Standard v1.x*  
*Cross-reference: HOME Wire Conventions v1.0 §8–§9, HOME Codebook Standard v1.0 §3.1*
