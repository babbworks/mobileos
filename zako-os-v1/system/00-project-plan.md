`                                                                                                                                                                                                                                                ,,,, ,,,                                                             ZAKO OS v1 — Master Project Plan

A phased engineering schedule for building the ZAKO sovereign mobile operating system from specification to operational device.

**Language:** C (MISRA-C subset) for all daemons, protocol libraries, AND primary user-facing applications. The core ZAKO app suite (PADS, Exchange, Sovereignty Dashboard, Outstack widget) will be native C with direct framebuffer or minimal toolkit rendering — radically efficient, instant-launch, minimal RAM. APK/Android app support retained as a secondary capability for third-party software (F-Droid ecosystem), but ZAKO's own tools do not depend on the JVM or Android framework UI stack. POSIX shell for build tooling. Python for test orchestration.

**Integration model:** Zero AOSP framework modifications. Pure addition layer via init.rc services, Unix socket IPC, resource overlays, and system properties.

**Target hardware:** Cat S22 Flip (QM215/MSM8937), 2GB RAM, 16GB eMMC, 1450mAh. Canadian SIM for interim carrier testing.

**Tracking:** All work logged in workwarrior `~/wwv02` profile `zako-os-v1`.

---

## Phase 1 — Foundation Libraries

*No hardware dependency. Pure C libraries testable on any Linux machine.*

### Objective
Build the cryptographic and protocol primitives that every ZAKO daemon depends on. These are self-contained C libraries with zero external dependencies beyond a C99 compiler and standard libc.

### Components

| ID | Component | Description | Est. Effort | Dependencies |
|----|-----------|-------------|-------------|--------------|
| S-24 | `libzako-hash` | BLAKE3 wrapper — chain_hash, frame_hash, genesis anchor computation | 1 week | None (uses BLAKE3 reference impl) |
| S-30 | `libzako-sign` | ed25519 wrapper — keygen, sign, verify. Wraps TweetNaCl. | 1 week | None (uses TweetNaCl) |
| S-31 | `libzako-did` | DID formatter — did:key method, z6Mk prefix, BASE58BTC encode/decode | 3 days | S-30 |
| S-20 | `libzako-bitpads` | BitPads v2.0 codec — Meta byte, 4 frame types, encode/decode | 2 weeks | S-24 |
| S-21 | `libzako-bitledger` | BitLedger v3.0 codec — 40-bit record, conservation invariant, CRC-15 | 2 weeks | S-20 |
| S-22 | `libzako-c0` | C0 Enhancement Grammar — 13 positions, priority/ACK/continuation parse | 1 week | S-20 |

### Quality Gate
- Every library has unit tests (coverage >90% of branches)
- Every library passes cppcheck MISRA-C analysis with zero warnings
- Every input-parsing function fuzz-tested with AFL++ (1M+ iterations, no crashes)
- Every library builds for both host (x86_64 Linux) and target (ARM32)
- API documented in header file comments (Doxygen-extractable)

### Exit Criteria
All 6 libraries compile, pass tests, pass static analysis, and pass fuzzing on host. Cross-compile for ARM32 verified.

---

## Phase 2 — Core Daemons

*Requires Phase 1 libraries. Testable on host Linux with mock sysfs.*

### Objective
Build the three CRITICAL daemons and the power governor. These are the submerged layer — the machinery that enforces ZAKO's promises.

### Components

| ID | Component | Description | Est. Effort | Dependencies |
|----|-----------|-------------|-------------|--------------|
| S-10 | `telux-ledgerd` | Append-only SQLite ledger, conservation enforcement, chain hashing, fsync-before-ACK, Unix socket intake | 4 weeks | S-20, S-21, S-24 |
| S-11 | `telux-identd` | Key generation, DID management, capability grants, identity lock, signing service | 3 weeks | S-30, S-31, S-24 |
| S-01 | `outstack-powerd` | Five-mode state machine, sysfs battery/thermal reading, cgroup freezer, record emission, C0 signal broadcast | 4 weeks | S-20, S-22, S-24 |
| S-40 | `libzako-bus` | System bus — Unix domain socket multiplexer, C0 signal routing between daemons | 2 weeks | S-22 |

### Quality Gate
- Each daemon starts, runs, and stops cleanly under `valgrind` with zero memory errors
- Each daemon passes its protocol conformance test suite (derived from spec)
- `telux-ledgerd`: conservation invariant tested with adversarial batches (intentional imbalance → rejection)
- `telux-identd`: key operations tested against known test vectors (RFC 8032)
- `outstack-powerd`: state machine tested against all 6 scenarios from `state_machine_examples.md`
- All daemons communicate correctly over the system bus

### Exit Criteria
Three daemons + bus library running on host Linux in a test harness. Can create an Island, generate a DID, write records to the ledger, sign them, and have Outstack gate mock processes. All without target hardware.

---

## Phase 3 — Exchange & Transmission

*Requires Phase 2 daemons running. Builds the external-facing capabilities.*

### Objective
Build the outbound transmission layer (sharedb), the bilateral exchange engine, and the codec that enables SMS/QR record exchange.

### Components

| ID | Component | Description | Est. Effort | Dependencies |
|----|-----------|-------------|-------------|--------------|
| S-14 | Exchange Engine | SEND/RECEIVE cycle, bilateral conservation check, atomic dual-leg posting | 3 weeks | S-10 |
| S-12 | `telux-sharedb` | Channel abstraction (SMS/QR/BLE/IP), queue management, per-channel formatting | 3 weeks | S-10, S-11 |
| S-25 | `libzako-padsurl` | pads-v1 URL codec — encode records as <300 char URLs for SMS | 1 week | S-20, S-21 |
| S-32 | Capability system | GRANT/REVOKE/DELEGATE records, depth check (max 3), cascade revocation | 2 weeks | S-11 |
| S-23 | `libzako-pictography` | 4-bit symbol codec, codebook switching, ALERT promotion | 1 week | S-22 |

### Quality Gate
- Exchange Engine: tested with bilateral payment scenario (ZMW transfer between two mock Sovereigns)
- sharedb: tested with mock SMS channel (local loopback) — record encoded, "sent," received, decoded, posted to ledger
- pads-v1 URL: round-trip encode/decode matches for all frame types
- Capability cascade: tested — revoke a grant and verify all downstream delegations are invalid

### Exit Criteria
Two simulated ZAKO devices (both running on host) can exchange a signed payment record via mock SMS channel, with conservation enforced, chain hashes verified, and both ledgers consistent.

---

## Phase 4 — AOSP Integration & Build System

*Requires target hardware (Cat S22 Flip) or QEMU emulation.*

### Objective
Establish the AOSP build environment, device tree, and get a bootable image with ZAKO daemons running on Android.

### Components

| ID | Component | Description | Est. Effort | Dependencies |
|----|-----------|-------------|-------------|--------------|
| I-01 | AOSP manifest | Repo manifest, local manifest for ZAKO packages | 1 week | — |
| V-01 | Device tree | BoardConfig.mk, fstab, init.rc, overlays, proprietary-files.txt | 2 weeks | I-01 |
| I-02 | Vendor blob extraction | Pull blobs from stock device, organize in vendor/ tree | 1 week | Physical device |
| B-01 | Kernel build | Cross-compile Linux 4.9 CAF with ZAKO defconfig options (cgroup freezer, thermal) | 2 weeks | I-01 |
| V-03 | init.rc services | Service entries for all ZAKO daemons, SELinux domains | 1 week | V-01 |
| V-02 | SELinux policy | Custom domains for 5 ZAKO daemons, file_contexts for /data/zako/ | 2 weeks | V-03 |

### Quality Gate
- `m -j$(nproc)` completes without error
- `boot.img` flashes and device reaches home screen
- ADB shell accessible, SELinux enforcing
- All 5 ZAKO daemons start (visible in `ps`)
- `outstack-ctl status` reports correct mode
- Zero GMS packages present
- Captive portal, NTP, DNS all point to non-Google endpoints

### Exit Criteria
Cat S22 Flip boots ZAKO OS, all daemons running, telephony stack initializes (SIM detected), ADB works.

---

## Phase 5 — Device Bring-Up & Telephony

*Requires Phase 4 bootable image + Canadian SIM.*

### Objective
Achieve full telephony operation and device-specific hardware bring-up. This is where ZAKO becomes a functional phone.

### Components

| ID | Component | Description | Est. Effort | Dependencies |
|----|-----------|-------------|-------------|--------------|
| — | Voice calls | MO + MT calls on Canadian carrier | Testing | Phase 4 |
| — | SMS | Send + receive on Canadian carrier | Testing | Phase 4 |
| — | LTE data | Attach, browse, IP connectivity | Testing + APN config | Phase 4 |
| — | USSD | *#100# balance check (carrier-dependent) | Testing | Phase 4 |
| B-05 | Lid sensor | Hall effect SW_LID integration with Outstack (flip close → CONSERVE) | 1 week | DTB decompile |
| B-06 | T9 keypad | GPIO-keys scan code mapping, verify all physical keys | 1 week | DTB decompile |
| B-04 | Cover display | ST7789 SPI driver, telux-coverd framebuffer writes | 2–4 weeks | DTB decompile, reverse engineering |
| S-03 | Outstack radio helper | Java service translating mode changes to eDRX AT commands | 1 week | Phase 4 + RIL |

### Quality Gate
- Voice call connects (both directions) on Canadian SIM
- SMS sends and receives
- LTE data connects and loads a webpage
- Lid close triggers CONSERVE mode (SW_LID → outstack-powerd)
- All physical keypad keys register correct scan codes
- Cover display shows time/battery (stretch goal for Phase 5)

### Exit Criteria
A functional phone. Makes calls, sends SMS, connects to data, responds to physical inputs. ZAKO daemons running in the background enforcing power governance. The device is usable.

---

## Phase 6 — Applications & User Experience

*Requires Phase 5 functional phone.*

### Objective
Build the user-facing ZAKO application suite as native C programs with direct rendering — not as Android APKs running on the JVM. These applications will launch instantly, consume minimal RAM, and operate within Outstack's power governance with the same efficiency as the daemons themselves.

### UI Rendering Approach (Research Required at Phase Start)

The core question: how do native C applications render UI on an Android device without using the Android framework's View system?

Options to research:
- **Direct framebuffer** — write pixels directly to `/dev/graphics/fb0` or via SurfaceFlinger's native surface API. Maximum efficiency. Requires building a minimal widget toolkit or using an existing one (lvgl, nuklear).
- **SurfaceFlinger native client** — use Android's `ANativeWindow` / `Surface` API from native code. Official, supported, composites correctly with Android status bar. Used by games and video players.
- **Wayland/wlroots compositor** — replace SurfaceFlinger entirely with a lightweight Wayland compositor. Maximum control but breaks APK compatibility.
- **Minimal Android Activity in C** — use Android's `NativeActivity` (pure C entry point, EGL rendering). Officially supported, minimal JVM involvement. Gets a window from the system compositor.
- **lvgl (Light and Versatile Graphics Library)** — C-based embedded GUI library. Runs on framebuffer, designed for resource-constrained devices. Used in automotive and industrial HMIs.

**Phase 6 will begin with a UI rendering research sprint** to determine the right approach. The decision balances: rendering efficiency, APK ecosystem compatibility (for F-Droid third-party apps), compositing with status bar/notifications, and implementation complexity.

### Components

| ID | Component | Description | Est. Effort | Dependencies |
|----|-----------|-------------|-------------|--------------|
| V-50 | UI toolkit research | Evaluate rendering approaches for native C apps on Android | 2 weeks | Phase 5 |
| V-51 | `libzako-ui` | Minimal rendering toolkit — whatever approach the research selects | 4 weeks | V-50 |
| V-06 | ZAKO First-Run | Setup wizard: language, SIM, PIN, privacy. Native C. | 3 weeks | V-51 |
| V-07 | T9 IME | Input method. May need thin Java/JNI shim for Android IME framework, but core logic in C. | 3 weeks | V-51 |
| V-10 | PADS app | Field records — Work Islands, forms, signoffs, exchange. Native C UI. | 4 weeks | Phase 3 + V-51 |
| V-11 | Exchange app | Send, receive, query, natural language. Native C UI. | 4 weeks | Phase 3 + V-51 |
| V-12 | Sovereignty dashboard | Island management, grants, identity, ledger browser. Native C. | 3 weeks | Phase 3 + V-51 |
| V-08 | Outstack display | Power mode indicator — status bar or dedicated display element. Native. | 1 week | V-51 |
| S-13 | telux-coverd full | Cover display: time, date, battery, mode, last ledger entry, caller ID. Pure C. | 2 weeks | B-04 |

### APK Compatibility Layer (Retained)

Android's application framework (ART runtime, View system, Activity lifecycle) remains intact in the Visible layer. Third-party apps from F-Droid run normally. But ZAKO's own core tools bypass this stack entirely — they are native processes with native rendering, communicating with ZAKO daemons via the same Unix socket IPC the daemons use with each other.

The user experience: ZAKO apps launch in under 200ms, use <5MB RAM each, and respond at framebuffer refresh speed. Third-party APKs (Organic Maps, Mull browser, etc.) run normally through Android's standard runtime but are governed by Outstack's process class system.

### Quality Gate
- Every ZAKO native app launches in <200ms cold start (measured)
- Every ZAKO native app uses <5MB RSS at steady state
- UI renders correctly on the 480×800 main display
- T9 keypad navigation works for all UI elements (no touch dependency)
- Setup Wizard completes without network access
- PADS app creates a Work Island, assigns a task, records completion
- Exchange app shows ledger history, allows natural language query

### Exit Criteria
A complete native user experience. A non-technical user can pick up the device, complete setup, make a payment record, and query their history — all rendered natively, all instant, all within ZAKO's power discipline.

---

## Phase 7 — Hardening & Production Readiness

*Requires all above. Final gate before release.*

### Objective
Security hardening, kernel-level enforcement modules, performance validation, and release preparation.

### Components

| ID | Component | Description | Est. Effort | Dependencies |
|----|-----------|-------------|-------------|--------------|
| B-02 | Telux-SEC LSM | Island boundary enforcement at kernel level (supplementary to SELinux) | 4 weeks | All Phase 2–5 |
| B-03 | Outstack exec gate | LSM hook at execve() — kernel-enforced process class gating | 2 weeks | S-01 running |
| — | 48h stability soak | Continuous operation, no crashes, no memory leaks | Testing | All above |
| — | Power validation | Measure idle power vs. 50mW target, standby vs. 100mW, governed drain curves | Testing | All above |
| — | Security audit | SELinux policy review, cppcheck full MISRA scan, fuzz campaign final results | Audit | All above |
| I-04 | OTA system | Full + incremental OTA generation, signing, test deploy | 2 weeks | Phase 4 |
| I-05 | CI/CD | Docker-based automated build + basic boot test | 2 weeks | Phase 4 |

### Quality Gate
- Telux-SEC prevents unauthorized Island access (tested with deliberate violation attempt)
- Exec gate prevents BACKGROUND process launch in CRITICAL mode (tested)
- 48h soak: zero crashes, zero memory growth, zero wakelock leaks
- Idle power: measured <100mW with SIM registered (Canadian carrier)
- Full MISRA-C scan: zero mandatory rule violations in ZAKO code
- OTA: full update installs, device boots, all daemons running

### Exit Criteria
Release candidate. Passes all gates in `distribution-checklist.md`. Ready for Zambian SIM testing when available.

---

## Timeline Estimate (Sequential)

| Phase | Duration | Cumulative |
|-------|----------|------------|
| Phase 1 — Foundation Libraries | 8 weeks | 8 weeks |
| Phase 2 — Core Daemons | 12 weeks | 20 weeks |
| Phase 3 — Exchange & Transmission | 10 weeks | 30 weeks |
| Phase 4 — AOSP Integration | 8 weeks | 38 weeks |
| Phase 5 — Device Bring-Up | 6 weeks | 44 weeks |
| Phase 6 — Applications | 12 weeks | 56 weeks |
| Phase 7 — Hardening | 8 weeks | 64 weeks |

**Total: ~64 weeks (15 months) sequential.**

With parallelism (Phase 6 apps can start during Phase 5 bring-up, Phase 7 CI/CD during Phase 5): **~12 months is achievable.**

---

## Parallel Tracks

Some work can proceed in parallel:

| Track A (Software) | Track B (Hardware) | Track C (Apps) |
|--------------------|--------------------|----------------|
| Phase 1: Libraries | — | — |
| Phase 2: Daemons | Phase 4 starts (AOSP sync, device tree) | — |
| Phase 3: Exchange | Phase 5: Telephony testing | Phase 6 starts (Setup Wizard, T9 IME) |
| — | — | Phase 6 continues (PADS, Exchange app) |
| Phase 7: Hardening | Phase 7: Power validation | Phase 7: UX polish |

**Effective parallel timeline: ~10–12 months.**

---

## Immediate Next Actions

1. **Create component directories** — `system/components/libzako-hash/`, `system/components/libzako-sign/`, etc.
2. **Write S-24 (libzako-hash) design doc** — API surface, BLAKE3 integration approach, test strategy
3. **Begin S-24 implementation** — first code in the project

Phase 1 starts now.

---

## Workwarrior Task Backlog (to be created)

After this plan is confirmed, the following task tree should be created:

### Phase 1 Tasks
- `task add "Design libzako-hash API (BLAKE3 wrapper)" project:zako-os +code +bitpads priority:H`
- `task add "Implement libzako-hash" project:zako-os +code +bitpads priority:H`
- `task add "Design libzako-sign API (ed25519 wrapper)" project:zako-os +code +telux priority:H`
- `task add "Implement libzako-sign" project:zako-os +code +telux priority:H`
- `task add "Design libzako-did API (DID formatter)" project:zako-os +code +telux priority:M`
- `task add "Implement libzako-did" project:zako-os +code +telux priority:M`
- `task add "Design libzako-bitpads API (frame codec)" project:zako-os +code +bitpads priority:H`
- `task add "Implement libzako-bitpads" project:zako-os +code +bitpads priority:H`
- `task add "Design libzako-bitledger API (conservation codec)" project:zako-os +code +bitpads priority:H`
- `task add "Implement libzako-bitledger" project:zako-os +code +bitpads priority:H`
- `task add "Design libzako-c0 API (enhancement grammar)" project:zako-os +code +bitpads priority:M`
- `task add "Implement libzako-c0" project:zako-os +code +bitpads priority:M`

---

*This plan is the synthesis of: 01-component-inventory.md (49 components), 02-language-research.md (C MISRA subset), 03-aosp-linkage-points.md (45 touch points, zero mods), BabbCat-Implementation-Plan.md (phase structure), and distribution-checklist.md (quality gates).*
