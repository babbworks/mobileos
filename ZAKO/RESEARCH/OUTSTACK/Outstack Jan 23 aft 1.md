# Outstack — Jan 23 afternoon, session 1

**2025-01-23, ~14:30**

---

## The Problem With "Power Management"

I keep coming back to this: every existing mobile OS treats power as a management problem. Manage the battery. Manage the brightness. Manage the wakelocks. Manage.

But management is reactive. You're always chasing the problem — some rogue app drained 40% overnight, so you build a wakelock detector. Another app holds the radio awake for push notifications, so you build Doze. The whole thing is patch upon patch, and the user has no sovereignty over how their power budget is allocated. The phone just... does stuff, and sometimes it dies at 2pm.

What if we treated power not as a thing to manage, but as a thing to *govern*?

---

## The Spacecraft Analogy

This clicked for me when I was reading about Voyager's RTG power budget allocations. Voyager doesn't "manage" power. It *governs* power. Every subsystem has a budget. Every budget is accounted for. When the RTG output drops (which it does, year by year, as the plutonium decays), the governance system sheds subsystems in a known priority order. The magnetometer gets cut before the radio. The radio gets cut before the flight computer. It's not reactive — it's a pre-determined governance policy that executes automatically based on a power reading.

Mars rovers do this too. Curiosity's RTG gives it a known baseline, but the battery still fluctuates with load. The rover doesn't just have a "battery low" mode — it has a hierarchy of operational states with clear rules about what runs in each state.

A phone in Lusaka with 15% battery and no guarantee of when it'll charge next is, from a systems design perspective, *the same problem* as a spacecraft with a decaying RTG. The physics are different. The engineering discipline should be the same.

---

## Why Android's Approach Fails (for us)

Android has Doze, App Standby buckets, Adaptive Battery, background execution limits. These are all good individual mechanisms. But they share a structural flaw: they're advisory and probabilistic. The system tries to predict what you need and throttle what you don't. It's ML-driven guesswork.

For ZAKO's use context — irregular charging, single critical device, mobile money transactions that can't fail — guesswork isn't good enough. If the phone has 8% battery and a market trader needs to receive a payment, the system needs to *guarantee* that the payment machinery is running. Not guess. Not hope. Guarantee by policy.

This means we need:
1. A classification of every process by how essential it is
2. A set of system states defined by objective power readings
3. Hard rules: in state X, classes A and B run, classes C and D do not
4. Recording: every state change is recorded. Auditable. Not just logs.

---

## Initial Thoughts on Process Classification

What if every process declares what it is? Not in terms of Linux niceness or cgroups, but in terms of the power governance system.

Rough sketch of classes:

- **Essential** — Must run no matter what. The ledger daemon. The identity daemon. The payment engine. These run at 2% battery. These run in thermal shutdown prelude. Non-negotiable.
- **Interactive** — Runs when the user is actively doing something. UI, input processing, the foreground app. Not gated unless we're about to die.
- **Background** — Periodic work. Sync, health data, notifications. Can wait. Can be deferred for hours.
- **Optional/Luxury** — AI inference, recommendation engines, pre-fetching, analytics. Only runs when power is abundant.

Maybe there's a fifth class? Something between background and luxury? Need to think about this. The gap between "sync my health data every 4 hours" and "run ML inference on my spending patterns" feels like it needs a distinction. Health sync has a staleness constraint — it *must* run within some window. ML inference has no such constraint — it's purely opportunistic. That's a meaningful difference. "Deferred" vs "Opportunistic"? "Scheduled" vs "Speculative"?

Let me try five:
1. Essential (always runs)
2. Interactive (runs when user is active)
3. Background (periodic, has a deadline/staleness constraint)
4. Deferred (periodic, but tolerates multi-day delay)
5. Speculative (purely optional, discardable)

That feels right. The key distinction between 3 and 4 is urgency: background work has a window ("must sync within 4 hours"), deferred work has only a soft expectation ("should run within 72 hours, no hard consequence if it doesn't"). And 5 is truly fire-and-forget — if it never runs, nothing breaks.

Question: where does the modem fall? The modem itself must always be running (paging, at minimum). But the modem *doing things* — data sync, push notification retrieval — that might be class-dependent. The modem is the hardware; what you do with it is the governance question. The RIL daemon itself is Essential. The data connection that enables background sync is a shared resource gated by the class of the process requesting it.

Similarly: the display hardware is always available (when lid is open), but the *processes* that render to it are Interactive class. When Interactive is gated, the display goes dark — not because we power-gated the display hardware, but because nothing is drawing to it.

---

## Power as a Ledger

Here's the thought that excites me most: what if we record power state transitions the same way we record financial transactions? The HOME ledger already gives us an append-only record format. Power events fit naturally:

- Battery went from 45% to 44% — that's a debit. Power left the system.
- Device started charging — that's an income event.
- Mode transition from Normal to Conserve — that's a governance action. Recorded.
- Process class gated — that's a capability revocation. Recorded.

Why do this? Several reasons:

1. **Accountability.** If a process class was gated at an inappropriate time, the record shows it. You can audit the governance system.
2. **Pattern recognition.** Over time, the power ledger reveals usage patterns. When does this user typically charge? What's their daily power draw curve? This informs future governance decisions.
3. **Sovereignty.** The sovereign (the user) can inspect their power history. When did the phone gate my background sync? Why? The record exists. It's not hidden in some debug log that gets rotated away.
4. **Conservation invariant.** The ledger should balance. Power in minus power out equals current state. If it doesn't balance, something is wrong — a measurement error, a rogue process, a hardware issue.
5. **Diagnostics.** If a user reports "my phone died unexpectedly," the power ledger tells the story. What mode was it in? When did it transition? Was there a thermal event? Was a process drawing anomalous power? The answer is in the records.

This is the bookkeeping approach to power. Double-entry. Every draw has a source. Every gate has a record. The power budget is a ledger that must balance.

The record types I'm thinking about:
- POWER_CHANGE — battery level shifted significantly (say >5% since last record)
- MODE_CHANGE — mode transition (from X to Y)
- GATE — process class was gated (restricted from execution)
- RESTORE — process class was restored (permitted to execute again)
- SUSPEND — individual process was suspended
- RESUME — individual process was resumed
- CALIBRATE — battery calibration event (full cycle complete, fuel gauge recalibrated)

Each of these maps to HOME's record structure: a task_code, an account_pair (who did what to whom), a domain (Engineering, for power events), a value, a timestamp. The power ledger is just another sub-entity in the system's Island. Same format. Same tamper-evidence. Same auditability.

---

## Open Questions

- How many modes? I said five states in a brainstorm last week but only sketched four classes above. The states and classes might be different things — states are about total system power, classes are about process importance. They interact but they're not the same axis. The modes are: what is the system's current power situation? The classes are: how important is this process? The gating matrix is where they intersect: given this mode, which classes run?

- How does charging affect mode? If the device is at 15% battery but actively charging at a good rate, should it still be in critical mode? The power trajectory matters, not just the snapshot. A device at 15% and charging fast is in a fundamentally different situation than a device at 15% in a village with no electricity until Thursday. Maybe: if charging above some rate threshold, operate one mode "better" than battery alone would indicate. A kind of credit against future battery state.

- Thermal? Temperature affects battery chemistry, affects safe discharge rate, affects CPU capability. A hot phone at 30% battery might need to behave like a cool phone at 15%. Temperature is a multiplier on the power constraint, not a separate axis. Or is it? Maybe thermal should be able to force emergency mode regardless of battery level. A device overheating at 80% battery still needs to shed load — not for power reasons but for safety reasons. Thermal might be an independent emergency trigger.

- What's the enforcement mechanism? Do we use cgroups? SIGSTOP/SIGCONT? Something custom? The mechanism needs to be reliable and fast — when the mode changes, processes in the gated class need to stop immediately, not "eventually." cgroups feel right — the freezer cgroup can halt an entire group of processes atomically. SIGSTOP is more portable but less atomic (you send signals one at a time).

- What about the record format? HOME's BitPads gives us the frame structure. But power events are frequent — battery changes, sampling intervals, mode checks. Can the ledger handle the write volume without itself becoming a power draw problem? Need to think about this carefully. If we sample every 30 seconds but only write a record when something meaningful changes (mode transition, significant battery delta, gate event), the volume is probably manageable. Maybe 50-100 records per day? That's nothing for telux-ledgerd.

- Where does this daemon live in the system architecture? Kernel space has the lowest latency but maximum maintenance cost. Userspace has higher latency (sysfs reads take microseconds, not nanoseconds) but is dramatically easier to maintain and test. For 30-second sampling intervals, microsecond latency differences are irrelevant. Leaning toward userspace daemon.

- What's the relationship to Android's existing battery service? Android's BatteryService already reads the fuel gauge and broadcasts battery intents. Do we piggyback on that, or read the hardware directly? Reading sysfs directly means we don't depend on Android framework APIs that might change between AOSP versions. More stable, fewer coupling points. And sysfs is just file reads — trivial from native C.

- Hysteresis. If the threshold for entering Conservation is 20% battery, what's the threshold for leaving it? If it's also 20%, a device hovering at 20% will oscillate between modes constantly — enter, exit, enter, exit. Need a dead band. Maybe: enter at 20%, exit at 23%. Three percent hysteresis. This prevents oscillation at threshold boundaries. The exact band width is a tuning parameter per deployment.

---

## Where This Goes

I think the core insight is: power governance, not power management. Governance implies:
- Defined authority (the power daemon has exclusive authority over mode)
- Defined rules (threshold → mode → class gates)
- Defined records (every action is recorded)
- Defined accountability (records can be audited)
- Defined rights (some processes have the right to run regardless)

This isn't just technical — it maps to sovereignty. A sovereign governs. A sovereign's device should govern its own resources with the same discipline. Not beg apps to use less power. Not hope that ML predictions about usage patterns are accurate. Govern.

The metaphor of the ledger keeps pulling me back. In bookkeeping, every transaction has a counterparty. Every debit has a credit. The books must balance. Apply this to power: every draw has a source (the battery). Every gate has a record (accountability). Every mode transition is a governance action with a paper trail. The power budget is a ledger that must balance over any closed period — energy in equals energy out plus what remains in the battery. If it doesn't balance, something is being measured wrong or something is drawing power outside governance.

This is the insight that distinguishes Outstack from Doze, from AUTOSAR wake management, from any existing power management scheme I can find: the *recording* of power governance as part of the same tamper-evident chain that records financial transactions, identity assertions, and capability grants. Power isn't a separate domain — it's a dimension of the ledger. It's accounted for with the same discipline.

The practical implication: when the sovereign reviews their device's behavior, they don't just see "battery: 45%". They see a history. When did it enter Conservation? Why? What was gated? For how long? When did it recover? What was the pattern? This is transparency. This is what sovereignty over a device actually means — not just owning it, but understanding it. Being able to audit it. The device is accountable to its sovereign.

Next: need to formalize the mode model. How many modes, what thresholds, what gating rules. And the daemon design — does this live in kernel space or userspace? What interfaces does it need? And the process class model — what's the taxonomy? Four classes? Five? The gap between "background sync" and "ML inference" feels like it needs another level.

---

*Ref: Voyager RTG degradation schedule, Mars 2020 power state machine documentation (public), Android Doze architecture (AOSP source), double-entry bookkeeping principles*
