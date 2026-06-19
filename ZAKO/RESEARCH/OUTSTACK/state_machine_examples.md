# State Machine Examples — Outstack Mode Transitions in Practice

**Date:** 2025-02-10
**Purpose:** Walk through concrete scenarios to validate that the five-mode state machine behaves correctly at boundaries, during charging, and during unexpected events. These examples serve as test vectors for the implementation.

---

## Conventions

- Battery percentage is written as `batt%`
- Mode names: FULL (0x0), STD (0x1), CONS (0x2), CRIT (0x3), EMRG (0x4)
- Threshold defaults: STANDARD_FLOOR=50%, CONSERVE_FLOOR=20%, LOW_FLOOR=10%, SHUTDOWN_FLOOR=3%
- HYSTERESIS_BAND=3%
- Arrows: `→` mode transition, `↓` time passing
- Records are noted with their task_code mnemonic

---

## Example 1: Full Charge → Complete Discharge

### Scenario

Device charged to 100%, unplugged, used normally throughout the day. No charging until the device dies.

### Walkthrough

```
T+0h:   batt=100%, charging=YES, mode=FULL
         Charging cable removed.
         batt=100%, charging=NO, mode=FULL (still above STANDARD_FLOOR)
         Record: POWER_CHANGE (direction=0, value=10000)

T+2h:   batt=85%, mode=FULL
         Normal usage. Background sync running. Opportunistic ML running.
         Record: POWER_CHANGE (value=8500) [delta >5% threshold]

T+4h:   batt=68%, mode=FULL
         Record: POWER_CHANGE (value=6800)

T+5h:   batt=55%, mode=FULL
         Record: POWER_CHANGE (value=5500)

T+6h:   batt=50%, mode=FULL → STD
         Battery crosses STANDARD_FLOOR (50%)
         Confirmation sample taken (30s later, still ≤50%)
         Sequence:
           1. MODE_ENTER C0 signal emitted (category=0x1)
           2. MODE_CHANGE record (value=0x01, prev=FULL new=STD)
           3. GATE record (value=bitmask for OPPORTUNISTIC: 0b00001)
           4. OPPORTUNISTIC processes receive SIGSTOP via cgroup freezer
         Record: POWER_CHANGE (value=5000)
         Record: MODE_CHANGE (value=0x01)
         Record: GATE (value=0x01)

T+6h:   STD mode active
         OPPORTUNISTIC processes gated (ML inference, speculative prefetch)
         All other classes continue normally
         CPU governor: schedutil (unchanged)

T+9h:   batt=35%, mode=STD
         Record: POWER_CHANGE (value=3500)

T+11h:  batt=20%, mode=STD → CONS
         Battery crosses CONSERVE_FLOOR (20%)
         Confirmation sample.
         Sequence:
           1. MODE_ENTER C0 signal (category=0x2)
           2. MODE_CHANGE record (value=0x12, prev=STD new=CONS)
           3. GATE record (value=bitmask for BACKGROUND+DEFERRED+OPPORTUNISTIC: 0b00111)
           4. BACKGROUND processes reach suspension point, then SIGSTOP
           5. CPU governor switched to powersave
         Record: MODE_CHANGE (value=0x12)
         Record: GATE (value=0x07)
         Per-process: SUSPEND records for each BACKGROUND process

T+11h:  CONS mode active
         BACKGROUND processes suspended (sync, health data, PADS)
         CRITICAL + INTERACTIVE continue
         Display timeout shortened to 20s
         eDRX cycle extended

T+14h:  batt=10%, mode=CONS → CRIT
         Battery crosses LOW_FLOOR (10%)
         Sequence:
           1. MODE_ENTER C0 signal (category=0x3)
           2. MODE_CHANGE record (value=0x23)
           3. GATE record (value=bitmask adds INTERACTIVE to gated... 
              Wait — no. INTERACTIVE is permitted in CRIT.
              Correction: in CRIT, BACKGROUND is fully gated (not just deferred)
              but INTERACTIVE is still permitted.
              Gate record: 0b00111 (same classes, but BACKGROUND is now hard-gated not deferred)
           4. Core parking activates (min 1 core online)
         Record: MODE_CHANGE (value=0x23)

T+14h:  CRIT mode active
         Only CRITICAL + INTERACTIVE run
         User can still make calls, send messages, interact with UI
         No background processing whatsoever
         Modem in maximum eDRX

T+16h:  batt=3%, mode=CRIT → EMRG
         Battery crosses SHUTDOWN_FLOOR (3%)
         Sequence:
           1. MODE_ENTER C0 signal (category=0x4)
           2. MODE_CHANGE record (value=0x34)
           3. GATE record (value=bitmask for everything except CRITICAL: 0b01111)
           4. INTERACTIVE processes gated (UI services suspended)
           5. Display: emergency-only (call only)
           6. Final state snapshot to durable storage
         Record: MODE_CHANGE (value=0x34)
         Record: GATE (value=0x0F)
         Per-process: SUSPEND records for INTERACTIVE processes

T+16h:  EMRG mode active
         Only CRITICAL: telux-ledgerd, telux-identd, Exchange Engine core, modem paging
         No user interaction possible (display emergency-only)
         Modem maintains paging for incoming calls/SMS
         Sampling interval: 30s (frequent, tracking rapid discharge)

T+16.5h: batt=1%, mode=EMRG
         System initiates shutdown preparation
         Record: POWER_CHANGE (value=0100)

T+17h:  batt=0%, SHUTDOWN
         Shutdown sequence:
           1. Final POWER_CHANGE record (value=0000)
           2. CLOSE record written
           3. telux-ledgerd flush confirmed
           4. Shutdown permitted
         Device powers off.
```

### Observations

- Total governed lifetime: ~17 hours from 100% on a 1450mAh battery with active use. Ungoverned might be 8-10 hours.
- The gain comes from Conservation mode extending the 20-10% band and Critical Reserve extending the 10-3% band significantly.
- Record trail: 6 POWER_CHANGE + 4 MODE_CHANGE + 3 GATE + N SUSPEND = full audit trail.

---

## Example 2: Gradual Discharge with Charging Mid-Critical

### Scenario

Device at 12% battery in Critical Reserve mode. User finds a charger and plugs in. Device is charging at 10W (fast charge capable charger + USB-C).

### Walkthrough

```
T+0:    batt=12%, mode=CRIT, charging=NO
        Only CRITICAL + INTERACTIVE running
        User plugs in charger (10W USB-C PD)

T+0+1s: charging=YES, rate=10W
        Record: POWER_CHANGE (direction=1, value=1200) [charging event]
        
        Charging override evaluation:
          rate (10W) > FAST_CHARGE_THRESHOLD (10W) — meets threshold
          battery (12%) > 10% (LOW_FLOOR) — above Emergency
          Override: operate ONE MODE BETTER than battery indicates
          Current mode by battery alone: CRIT (10% < 12% ≤ 20%... wait)
          Actually 12% is above LOW_FLOOR (10%), so battery-only mode is CRIT
          One mode better: CONS
          
        Mode transition: CRIT → CONS
        Sequence:
          1. MODE_ENTER C0 signal (category=0x2)
          2. MODE_CHANGE record (value=0x32, prev=CRIT new=CONS)
          3. RESTORE record (value=bitmask for INTERACTIVE — already active, so no-op)
          4. BACKGROUND processes remain suspended (CONS defers them, doesn't restore)
        Record: MODE_CHANGE (value=0x32)
        
T+5min: batt=14%, charging=YES, mode=CONS
        BACKGROUND still deferred (Conservation mode)
        But charging rate is good

T+20min: batt=21%, charging=YES, mode=CONS
         Battery crosses CONSERVE_FLOOR (20%) + HYSTERESIS_BAND (3%) = 23%
         Wait — 21% is above CONSERVE_FLOOR but not above 23% (hysteresis)
         Mode stays CONS.
         
T+30min: batt=24%, charging=YES, mode=CONS → STD
         Battery exceeds 23% (CONSERVE_FLOOR + HYSTERESIS_BAND)
         Upward transition fires
         Sequence:
           1. MODE_ENTER C0 signal (category=0x1)
           2. MODE_CHANGE record (value=0x21)
           3. RESTORE record (value=bitmask for BACKGROUND+DEFERRED: 0b00110)
           4. BACKGROUND processes resumed (SIGCONT)
           5. CPU governor back to schedutil
         Record: MODE_CHANGE (value=0x21)
         Record: RESTORE (value=0x06)
         Per-process: RESUME records for BACKGROUND processes
         
         Additionally: rate (10W) > FAST_CHARGE_THRESHOLD, batt (24%) < 30%
         Charging override: one mode better than battery indicates
         Battery alone says STD (20% < 24% ≤ 50%)
         One mode better would be FULL
         But second override rule: rate > threshold AND battery > 30% → FULL
         24% < 30%, so second rule doesn't apply
         Stay in STD. (The first override already moved us from CRIT to CONS earlier;
         now battery has naturally risen to STD territory)

T+45min: batt=31%, charging=YES, mode=STD → FULL
          Second charging override rule kicks in:
          rate (10W) > FAST_CHARGE_THRESHOLD AND battery (31%) > 30%
          → operate in FULL regardless of battery%
          Mode transition: STD → FULL
          Sequence:
            1. MODE_ENTER C0 signal (category=0x0)
            2. MODE_CHANGE record (value=0x10)
            3. RESTORE record (restores OPPORTUNISTIC: 0b00001)
            4. All process classes active
          Record: MODE_CHANGE (value=0x10)
          Record: RESTORE (value=0x01)
          
T+45min: FULL mode, charging
         All processes run. ML inference resumes. Background sync catches up.
         The device is fully functional while charging.
```

### Observations

- The charging override prevents the frustrating "plugged in but still restricted" state
- The sequence is: CRIT → CONS (immediate, via override) → STD (natural, via battery rise) → FULL (via second override rule)
- Total time from plugging in to full functionality: ~45 minutes. Without override, the user would wait until battery reached 53% (STANDARD_FLOOR + HYSTERESIS) to return to FULL — potentially 2+ hours
- Every transition is recorded. Auditable.

---

## Example 3: Thermal Override Event

### Scenario

Device at 65% battery, mode FULL, left in direct sunlight on a car dashboard. Device temperature climbs.

### Walkthrough

```
T+0:     batt=65%, mode=FULL, temp=32°C
         Normal operation.

T+5min:  batt=64%, temp=38°C
         Temperature rising. No threshold crossed yet.
         (THERMAL_CRITICAL=45°C, needs 60s sustained)

T+10min: batt=63%, temp=42°C
         Still below threshold.

T+13min: batt=63%, temp=45°C
         Threshold reached. Start 60s sustain timer.

T+14min: temp=46°C (sustained above 45°C for 60s)
         THERMAL OVERRIDE TRIGGERED
         
         Mode transition: FULL → EMRG (regardless of battery=63%!)
         This is unusual — battery is healthy but thermal forces Emergency
         
         Sequence:
           1. MODE_ENTER C0 signal (category=0x4)
           2. MODE_CHANGE record (value=0x04, prev=FULL new=EMRG)
              note field: "thermal_override: 46°C"
           3. GATE record (value=0b01111, all except CRITICAL gated)
           4. All non-CRITICAL processes gated immediately
           5. CPU governor → powersave
           6. Core parking: max cores
           7. Display: emergency-only
         
         Record: MODE_CHANGE (value=0x04) with thermal flag
         Record: GATE (value=0x0F)

T+14min: EMRG mode (thermal)
         Only CRITICAL processes running
         CPU at minimum frequency reduces heat generation
         Device draws minimum power → minimum self-heating
         Screen essentially off
         
T+20min: temp=44°C
         Dropping! Below THERMAL_CRITICAL but above THERMAL_SAFE (40°C)
         No recovery yet. Must sustain below THERMAL_SAFE for 120s.

T+25min: temp=41°C
         Still above 40°C. Timer not started.

T+28min: temp=39°C
         Below THERMAL_SAFE (40°C). Start 120s sustain timer.

T+30min: temp=38°C (sustained below 40°C for 120s)
         THERMAL OVERRIDE RECOVERED
         
         What mode do we enter? NOT the pre-override mode (FULL).
         Evaluate current battery against thresholds:
           batt=62%, STANDARD_FLOOR=50%
           62% > 50% → FULL mode by battery
         
         Thermal recovery → FULL
         (If battery had dropped to 18% during thermal emergency, recovery 
          would enter CONS, not FULL. Battery state at recovery time determines 
          the new mode.)
          
         Sequence:
           1. MODE_ENTER C0 signal (category=0x0)
           2. MODE_CHANGE record (value=0x40, prev=EMRG new=FULL)
              note: "thermal_recovery: 38°C"
           3. RESTORE record (value=0b01111, all classes restored)
           4. All processes resumed
           5. CPU governor → schedutil
         
         Record: MODE_CHANGE (value=0x40)
         Record: RESTORE (value=0x0F)
         Per-process: RESUME records for all suspended processes

T+30min: FULL mode restored
         Device back to normal. All processes running.
         User probably noticed: ~16 minutes of emergency mode.
```

### Observations

- Thermal override is independent of battery. A device at 95% battery can be in Emergency.
- Recovery mode is determined by *current* battery state, not pre-override mode. This prevents stale mode restoration.
- The 60s sustain-to-enter and 120s sustain-to-exit asymmetry prevents oscillation around the thermal threshold. Harder to enter, easier to stay in once entered. Wait — it's actually harder to exit (120s vs 60s). That's the conservative choice: once thermal is a problem, be cautious about declaring it resolved.
- The record trail clearly shows "thermal_override" — distinct from normal battery-driven transitions. Auditable.

---

## Example 4: Lid-Close Trigger (CAT S22 Flip Specific)

### Scenario

User closes the flip phone lid. The Hall effect sensor generates a SW_LID kernel event. How does Outstack respond?

### Walkthrough

```
T+0:    batt=55%, mode=FULL, lid=OPEN
        User is actively using the phone (foreground app running)
        
T+0:    User closes lid. SW_LID event generated.
        
        Question: Does lid-close trigger a mode transition?
        
        Analysis: Lid-close means the user is done interacting. The device
        is now in a pocket or on a table. But "not interacting" doesn't mean
        "low power." The user might close the lid during a phone call (the 
        call continues). They might close it while music plays. They might
        close it and expect background sync to continue.
        
        Design decision: Lid-close is NOT a mode transition trigger.
        It's an INTERACTIVE class signal — the display turns off, the 
        touch input is disabled, but the current mode persists. INTERACTIVE
        processes that require display (launcher, UI-dependent apps) go
        idle naturally. BACKGROUND and CRITICAL continue unchanged.
        
        What changes:
          - Display off (standard Android screen-off behavior)
          - Touch input disabled
          - Outer display shows clock/notifications (if in FULL/STD/CONS mode)
          - No mode change. No power record. (Screen-off is not a governance event.)
          
        What does NOT change:
          - Power mode (stays FULL)
          - Process gating (no changes)
          - Background sync (continues)
          - Music playback (continues)
          - Active call (continues)

T+5min: Standard Android display timeout would have fired anyway.
        Lid-close just accelerated the display-off by the remaining timeout.
        Outstack is unchanged.
```

### Observations

- Lid-close is a *hardware event* but not a *governance event*. The distinction matters.
- Android's existing screen-off behavior handles the display. Outstack doesn't need to duplicate this.
- Mode transitions are driven by *power state* (battery, thermal), not by *usage state* (screen on/off, lid open/closed).
- However: if the device is lid-closed for a long time AND battery is dropping, normal threshold-driven transitions will fire. The lid state is irrelevant to mode determination.

**Exception case:** Could a distribution define lid-close as a "prefer conservation" hint? Maybe. If the lid is closed, the device *could* be slightly more aggressive about deferring BACKGROUND work. But this is optimization, not governance. Keep it out of the core spec. Distributions can implement this as a policy extension if they want.

---

## Example 5: Rapid Oscillation Prevention (Hysteresis Validation)

### Scenario

Battery hovering at exactly 20% (the CONSERVE_FLOOR threshold). The fuel gauge reports 20%, then 21%, then 20%, then 19%, then 20% over successive samples.

### Walkthrough

```
T+0:    batt=21%, mode=STD
        (Device was in STD, battery declining)

T+60s:  batt=20%, mode=STD → CONS
        Battery ≤ CONSERVE_FLOOR (20%). Confirmation sample:
        
T+90s:  Confirmation: batt=20% (still at threshold)
        Transition confirmed. Enter Conservation.
        Record: MODE_CHANGE (value=0x12)
        BACKGROUND deferred.

T+120s: batt=21%
        Battery rose above CONSERVE_FLOOR (20%)!
        BUT: hysteresis requires battery > CONSERVE_FLOOR + HYSTERESIS_BAND
        That means battery must exceed 23% to return to STD.
        21% < 23%. MODE STAYS CONS.
        No transition. No record.

T+180s: batt=20%
        Still in CONS. 20% ≤ CONSERVE_FLOOR. Would re-enter CONS but already there.
        No transition. No record.

T+240s: batt=19%
        Still CONS. 19% > LOW_FLOOR (10%). No downward transition.
        No record.

T+300s: batt=20%
        Still CONS. 20% < 23% (hysteresis threshold for upward). No change.
        No record.

T+360s: batt=22%
        Still CONS. 22% < 23%. No change.
        
T+420s: batt=23%
        Battery = CONSERVE_FLOOR + HYSTERESIS_BAND (20% + 3% = 23%)
        Hysteresis requires ABOVE 23%, not equal to.
        Still CONS.

T+480s: batt=24%
        Battery > 23%. Upward transition triggered.
        Confirmation sample:
        
T+510s: Confirmation: batt=24% (confirmed above hysteresis threshold)
        Transition: CONS → STD
        Record: MODE_CHANGE (value=0x21)
        Record: RESTORE (BACKGROUND processes resumed)
```

### Observations

- Between T+0 and T+480s (~8 minutes), the battery oscillated around the threshold but the mode only changed ONCE (entering Conservation at T+90s). Without hysteresis, it would have flipped 4-5 times.
- The hysteresis band creates a "dead zone" between 20% and 23% where the device stays in whatever mode it's currently in. If it entered CONS at 20%, it stays there until 24%. If it was in STD at 22%, it would stay STD until dropping to 20%.
- The confirmation sample (30s extra check) adds additional stability for edge cases where the fuel gauge reports a transient reading.

---

## Example 6: Emergency to Full (Charger Plugged in During Emergency)

### Scenario

Device in Emergency mode at 2% battery. User finds a fast charger.

### Walkthrough

```
T+0:    batt=2%, mode=EMRG, charging=NO
        Only CRITICAL running. Device barely functional.
        
T+0:    User plugs in fast charger (18W USB-C PD)
        charging=YES, rate=18W
        Record: POWER_CHANGE (direction=1, value=0200)
        
        Charging override evaluation:
          "These adjustments are not applied in Emergency mode;
           Emergency exits only via the standard battery threshold recovery path."
          
        CHARGING OVERRIDE DOES NOT APPLY IN EMERGENCY.
        Device stays in EMRG despite fast charging.
        
        Why? Safety. Emergency mode means the device was dangerously low.
        The battery chemistry needs recovery time. Immediately going to
        full operation while at 2% and just starting to charge could cause
        voltage sag under load that triggers a brownout. Conservative: let 
        the battery actually recover first.

T+5min: batt=4%, mode=EMRG
        4% > SHUTDOWN_FLOOR (3%) + HYSTERESIS_BAND (3%) = 6%?
        No: 4% < 6%. Stay in EMRG.
        Still Emergency. Charging but restricted.

T+12min: batt=7%, mode=EMRG → CRIT
         7% > 6% (SHUTDOWN_FLOOR + HYSTERESIS_BAND)
         Upward transition: EMRG → CRIT
         Sequence:
           1. MODE_ENTER C0 signal (category=0x3)
           2. MODE_CHANGE record (value=0x43)
           3. RESTORE record (INTERACTIVE class restored)
           4. Display becomes usable
           5. INTERACTIVE processes resumed
         Record: MODE_CHANGE (value=0x43)
         Record: RESTORE (INTERACTIVE restored)
         
         NOW apply charging override:
           rate (18W) > FAST_CHARGE_THRESHOLD (10W)
           Override: one mode better → CRIT becomes CONS
           
         Immediate follow-up transition: CRIT → CONS
         Sequence:
           1. MODE_ENTER C0 signal (category=0x2)
           2. MODE_CHANGE record (value=0x32)
           3. (BACKGROUND still deferred in CONS — no process change vs CRIT)
         Record: MODE_CHANGE (value=0x32)

T+12min: CONS mode, fast charging
         CRITICAL + INTERACTIVE running. User can interact.
         BACKGROUND deferred (but recovering quickly)

T+25min: batt=15%, mode=CONS
         15% > CONSERVE_FLOOR + HYSTERESIS = 23%? No (15% < 23%)
         Still CONS by battery. But charging override...
         Already applied (one mode better: CONS instead of CRIT-by-battery)
         Wait: at 15%, battery-only mode would be CRIT (10% < 15% ≤ 20%)
         With override: one better = CONS. Current mode is CONS. Correct.

T+35min: batt=25%, mode=CONS
         Battery-only mode: STD (20% < 25% ≤ 50%)
         With override: one better = FULL... 
         But: second rule requires battery > 30% for FULL override
         25% < 30%. So just one mode better: STD
         Transition: CONS → STD
         Record: MODE_CHANGE (value=0x21)
         Record: RESTORE (BACKGROUND processes resume)

T+45min: batt=32%, mode=STD → FULL
         Second override rule: rate > threshold AND battery > 30% → FULL
         Transition: STD → FULL
         Record: MODE_CHANGE (value=0x10)
         Record: RESTORE (OPPORTUNISTIC restored)
         
         All processes running. Device fully functional.
         Total recovery time from EMRG to FULL: ~45 minutes on fast charger.
```

### Observations

- Emergency mode is sticky even during charging. This is a deliberate safety decision.
- The exit from Emergency follows the standard battery path: must exceed SHUTDOWN_FLOOR + HYSTERESIS (6%).
- After exiting Emergency, the charging override kicks in and accelerates the remaining recovery.
- From the user's perspective: plug in → ~12 minutes of limited functionality (EMRG) → then progressively more functionality over the next 30 minutes. Not instant, but not unreasonably slow.

---

## Summary of Validated Behaviors

| Scenario | Key Validation |
|----------|---------------|
| Full discharge | All five modes activated in sequence, records produced at each |
| Charge mid-CRIT | Override moves mode up one level immediately; natural recovery follows |
| Thermal override | Battery-independent Emergency entry and battery-dependent recovery |
| Lid close | Not a governance event; display management only |
| Oscillation | Hysteresis prevents flip-flopping at threshold boundary |
| Emergency + charger | Emergency is sticky; override applies only after normal exit |

---

*These examples should be reproducible as integration tests against the `outstack-powerd` daemon once implemented. Each scenario defines exact inputs and expected outputs.*
