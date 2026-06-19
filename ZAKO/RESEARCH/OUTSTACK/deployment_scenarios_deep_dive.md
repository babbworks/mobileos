# Deployment Scenarios Deep Dive — Outstack Stress Tests

**Date:** 2025-02-03
**Purpose:** Identify the deployment scenarios that stress the Outstack power governance model differently, ensuring the five-mode design handles each one without special-casing

---

## Motivation

A power governance system that only works for one deployment context is a power management system with pretensions. Outstack claims to be a general governance framework — but "general" doesn't mean "untested against specifics." Each deployment scenario stresses different assumptions in the mode model.

This document walks through four representative scenarios, identifies where the current design breaks or bends, and notes what the spec must address.

---

## Scenario 1: Personal Phone — Zambia Context

### Description

A market trader in Lusaka uses a CAT S22 Flip as their primary device. They make and receive mobile money payments (MTN, Airtel, Zamtel), receive SMS, make calls, and occasionally browse. Charging is irregular — sometimes daily at home, sometimes every two or three days when power is available at a friend's shop or a charging station.

### Power Profile

- Battery: 1450mAh (small)
- Typical daily drain without governance: ~100% in 8-10 hours of moderate use
- Charging availability: unpredictable. Sometimes once/day, sometimes once/72h
- Critical function: mobile money (USSD-based, via modem STK)
- Secondary critical: incoming calls and SMS

### How It Stresses the Model

**Threshold placement matters enormously.** If STANDARD_FLOOR is too high (say 50%), this user spends most of their time in Conservation or below, which gates background sync. But if STANDARD_FLOOR is too low (say 30%), the user hits Critical Reserve too fast when they miss a charging window.

**The 72-hour no-charge scenario.** At average drain of ~12%/hour under Conservation, 1450mAh lasts maybe 14 hours from full. But with Outstack governance actively gating background processes, actual Conservation drain might be 5-6%/hour (modem paging + display standby + CRITICAL daemons only). That gives ~16-17 hours in Conservation alone. Critical Reserve adds more. Emergency adds more. Total governed lifetime from 100% could reach 24-36 hours depending on actual use.

**Mobile money is CRITICAL.** The USSD interaction for mobile money goes through the modem's STK (SIM Toolkit) interface. This means the modem must be in a state that can process USSD sessions. In Emergency mode, where we gate everything except CRITICAL, the modem needs to be not just paging-capable but STK-capable. This is a modem state question: can the modem handle USSD in PSM/eDRX mode? Answer: yes, but only when the modem wakes for a paging window. The user might experience a delay of up to one eDRX cycle (5+ minutes in extreme settings) before a USSD session can initiate.

**Implication for spec:** Emergency mode must define modem state precisely. "Basic paging capability" isn't enough — STK/USSD must also be available, or mobile money fails in Emergency mode, which is exactly when the user most needs it (low battery, no other options).

**Charging behaviour override is critical.** When this user finds a charger, the phone might be at 8% battery. If Outstack stays in Emergency mode until battery rises above SHUTDOWN_FLOOR + HYSTERESIS_BAND (say 6-8%), the user has a frustrating 10-15 minutes where the phone is charging but still barely functional. The charging-adjusted mode policy (operate one mode better when fast-charging) directly addresses this. But: what if they're charging from a 500mA USB-A port? That's slow charging. Do we still adjust?

**Question:** Should the charging override have a minimum charge rate threshold? Probably yes. Charging at 500mA on a 1450mAh battery is ~0.35C, which is slow but genuine. Charging at 100mA (computer USB suspend current) is barely charging at all. Threshold: maybe 5W? Need to model this.

---

## Scenario 2: Industrial IoT — Always-On Sensor Node

### Description

A solar-powered environmental monitoring station in a rural area. It records temperature, humidity, air quality. It has cellular connectivity (NB-IoT or LTE Cat-M1) for periodic data upload. It never has a human user interacting with it directly. It runs 24/7/365.

### Power Profile

- Battery: 10000mAh LiFePO4 (deep cycle, temperature tolerant)
- Solar input: ~20W panel, variable with weather and season
- Charging: continuous during daylight, zero at night
- Critical function: sensor sampling + data recording
- Secondary: data upload (can be deferred)

### How It Stresses the Model

**The mode model assumes a discharge cycle.** Outstack's five modes are designed around a phone that charges to full and then discharges. An IoT node with solar input oscillates around a mean charge level — it might sit at 60-80% during sunny days and dip to 40-50% during cloudy stretches. It rarely hits Full Power from a "I just finished charging" perspective.

**No INTERACTIVE class exists.** There's no user. No UI. No foreground app. The INTERACTIVE process class is meaningless here. Does this matter? Structurally, no — those processes simply don't exist on this device, so the gating rules for INTERACTIVE are vacuous. But the sampling logic that assumes "user activity" as a signal for mode assessment needs to handle "no user activity ever" without false conclusions.

**DEFERRED and OPPORTUNISTIC redefine.** On a phone, OPPORTUNISTIC means "run AI inference when battery is high." On an IoT node, OPPORTUNISTIC might mean "run edge ML on sensor data for anomaly detection." The semantic is the same — spare-resource work — but the triggering conditions are different. A node at 70% battery on a sunny day is effectively "full power" for its workload. A node at 70% during a week of overcast weather is conserving.

**Solar input as a signal.** The current spec tracks charging rate as a budget replenishment signal. For solar, the charging rate varies throughout the day. Outstack should probably incorporate "energy balance" — is the instantaneous solar input exceeding instantaneous draw? If yes, the battery is net-charging and the mode should relax. If no, the battery is net-discharging and the mode should tighten. This is different from "is the device plugged in."

**Implication for spec:** The threshold model works for IoT, but distributions need to set thresholds much lower. An IoT node might set STANDARD_FLOOR at 30% and CONSERVE_FLOOR at 15%, because its average operating point is lower. The distribution configuration interface must support this without hacking the daemon.

**Long-duration data recording.** The sensor node records continuously. If it enters Conservation and gates BACKGROUND (which might include the data upload process), that's fine — data accumulates locally. But if it enters Critical Reserve and gates everything except CRITICAL, does sensor sampling continue? It should — that's the node's primary purpose. This means the sensor sampling daemon must be classified CRITICAL on this device, even though it would be BACKGROUND on a phone.

**Implication for spec:** Process class assignment must be per-distribution, not hardcoded. What's CRITICAL on one device is BACKGROUND on another. The spec defines the framework; the distribution defines the policy.

---

## Scenario 3: Medical Device — Health Alert Must Never Fail

### Description

A personal health monitoring device (wearable or phone with health sensors). It monitors heart rate, blood oxygen, fall detection. When an anomaly is detected, it must alert emergency contacts and/or emergency services. This alert must never fail due to power governance gating the wrong process.

### Power Profile

- Battery: 3000mAh (typical phone-sized)
- Charging: daily (user charges at night), generally predictable
- Critical function: health anomaly detection + alert transmission
- Secondary: routine health data logging and sync

### How It Stresses the Model

**The health alert is CRITICAL.** Obviously. But what constitutes the alert chain? It's: sensor reading → anomaly detection algorithm → alert generation → modem transmission. That's four components. All four must be CRITICAL. If even one is gated, the chain breaks.

**Anomaly detection might be computationally expensive.** Some health algorithms (fall detection via accelerometer pattern matching, arrhythmia detection via HR variability analysis) are not trivial. They consume CPU. In Critical Reserve or Emergency mode, where CPU governor is set to powersave and cores might be parked, can the algorithm still run within its timing constraints? A fall detection algorithm that takes 3 seconds instead of 300ms because the CPU is throttled to 300MHz is a fall detection algorithm that missed the fall.

**Implication for spec:** CRITICAL processes should not have their CPU scheduling priority reduced, even in Emergency mode. The current spec says `outstack-powerd` "may reduce the process's CPU scheduling priority" for CRITICAL processes in Emergency mode if they're drawing unsustainable power. For a medical device, this exception needs a distribution-level override: some CRITICAL processes are *unconditionally* CRITICAL, not even degradable.

**False positive governance.** What if the health monitoring system raises a false alert while in Emergency mode? The device might waste its last 3% of battery transmitting a false alarm. This is not Outstack's problem to solve — Outstack enforces governance, it doesn't evaluate alert validity. But it means the medical application's classification as CRITICAL must be earned. Not everything that *claims* to be critical should be CRITICAL.

**The night charging assumption.** Most health device users charge at night. But what about a user who forgets? Day 2 without charging, battery at 25%, in Conservation mode. Background health data sync is gated. That's fine. But what if the health data sync is how a remote doctor monitors the patient? The sync delay might be clinically relevant.

**Question:** Should the medical scenario define a separate process class, something between BACKGROUND and CRITICAL? A "HEALTH_BACKGROUND" that gets deferred but never fully gated? Or is this just a matter of classifying the medical sync as INTERACTIVE rather than BACKGROUND?

**Conclusion:** The five-class model probably handles this correctly if the distribution classifies correctly. The health alert chain is CRITICAL. The routine health sync is INTERACTIVE (it's clinically important but not emergency). Analytics and long-term trending are BACKGROUND. The key is getting the classification right per-distribution, and the spec should provide guidance for health-critical deployments.

---

## Scenario 4: Spacecraft/Remote Relay — Extreme Power Constraints

### Description

A remote communications relay or deep-field sensor package. Power source is limited (small solar panel or primary battery with no recharge). Communication windows are scheduled (satellite passes, or periodic radio checks). The device might go days between communication opportunities.

This isn't necessarily a literal spacecraft — think: a remote weather station on a mountain peak with a satellite modem that has a 10-minute pass window every 6 hours.

### Power Profile

- Battery: fixed (primary lithium, 50Wh, non-rechargeable) or small solar (~5W panel + small buffer battery)
- Charging: minimal or zero
- Total mission life: 6 months on primary battery; indefinite on solar if weather cooperates
- Critical function: scheduled data transmission during comm windows
- Secondary: continuous sensor sampling at low rate

### How It Stresses the Model

**The mode model assumes eventual recharge.** Outstack's hysteresis and charging-adjusted modes assume the battery will come back up. For a primary battery (non-rechargeable), the modes represent a one-way descent. Full → Standard → Conservation → Critical → Emergency → power death. There is no recovery. The mode thresholds represent remaining mission life, not a daily cycle.

**Implication for spec:** The mode model still works — it just means the device spends its entire mission traversing the mode sequence once, not cycling. A primary-battery device might spend months in Standard, weeks in Conservation, days in Critical Reserve. The thresholds need to be set based on mission duration planning, not daily use patterns.

**Communication windows create temporal urgency.** When a satellite pass occurs, the device must transmit regardless of its current power mode. If it's in Conservation and the comm process is BACKGROUND (gated), the comm window is missed. This means the comm process must be CRITICAL — even though it only runs 10 minutes every 6 hours.

**But a CRITICAL process that runs 10 minutes every 6 hours looks a lot like DEFERRED.** The semantic of "must run no matter what" conflicts with the semantic of "mostly idle." This is the scheduled-critical-task problem. The process isn't always critical — it's critical during a specific window.

**Possible solution:** Time-triggered mode override. The device knows its comm windows (scheduled). It can pre-plan: "at T+6h, exit Conservation temporarily for comm window, restore Conservation after window closes." This is like a wakelock, but governance-aware. Maybe: a RESERVATION record that pre-allocates a mode exception for a specific time window.

**Question:** Should the spec define time-triggered mode exceptions? Or should the comm process simply be CRITICAL (always permitted to run) and trust that it sleeps between windows? The second approach is simpler and probably correct — a CRITICAL process that mostly sleeps draws negligible power. The process class determines *permission*, not *activity*.

**Thermal in extreme environments.** A relay on a mountain peak might face extreme cold (affecting battery chemistry — LiFePO4 handles this better than Li-ion, but still) or extreme heat (in direct sun). The thermal override might need to account for both high AND low temperature. Current spec only triggers Emergency on high temperature. Low temperature reduces battery capacity but shouldn't trigger Emergency — it should trigger a capacity recalculation.

**Implication for spec:** Consider adding a THERMAL_COLD threshold that triggers a battery capacity derating (report 80% of measured percentage if temp < 0°C, for example). This keeps the mode model honest about available energy without incorrectly entering Emergency mode.

---

## Cross-Scenario Observations

### What the Mode Model Gets Right

1. **Universality.** The five modes apply to all four scenarios without structural modification. The semantics (Full = no restrictions, Emergency = only essential) are universal.
2. **Configurability.** Threshold placement being per-distribution means each scenario sets thresholds appropriate to its context. The framework is fixed; the policy is flexible.
3. **Class separation.** The five process classes map naturally to different roles in each scenario: what's CRITICAL differs, but the concept of "must always run" is universal.

### What Needs Spec Attention

1. **Charging rate threshold for mode adjustment.** Not all charging is equal. Define a minimum rate for the charging-adjusted mode policy.
2. **Process class assignment is per-distribution.** Make this explicit and provide guidance for different deployment types.
3. **Modem state in Emergency.** Define precisely: paging, STK/USSD, and SMS reception must all work. Not just "basic paging."
4. **CRITICAL process CPU guarantee.** Some deployments need CRITICAL processes to run at full CPU capability even in Emergency. Provide a distribution override for the "may reduce priority" clause.
5. **Time-triggered wake patterns.** How does a scheduled comm window interact with mode gating? Likely: classify the comm daemon as CRITICAL and let it sleep between windows. But document this pattern.
6. **Thermal cold.** Consider battery capacity derating below 0°C in addition to thermal emergency above 45°C.
7. **Solar/variable power source.** The "direction bit" (charging/discharging) is a good start, but "energy balance" (instantaneous input vs. instantaneous draw) would better serve solar deployments.

---

## Summary Matrix

| Scenario | Primary Stress | Key Mode | Key Class Issue | Spec Action |
|----------|---------------|----------|-----------------|-------------|
| Zambia phone | Irregular charging, small battery | Conservation | CRITICAL must include USSD/STK | Define modem state |
| Industrial IoT | No user, solar input | Standard | No INTERACTIVE exists; sensor is CRITICAL | Per-distribution class |
| Medical device | Alert chain must never fail | Emergency | Multi-component CRITICAL chain | CPU guarantee for CRITICAL |
| Remote relay | Non-rechargeable, comm windows | All (one-way) | Scheduled-CRITICAL pattern | Document time-wake pattern |

---

*This analysis feeds into the formal spec's §3 (Mode Definitions) and §4 (Process Classes). Each stress point must have a clear answer in the protocol document.*
