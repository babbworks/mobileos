# ZAKO OS v1 — Language Research

Survey of language choices for system-level software across Linux, defense/aerospace, embedded, and high-assurance domains. Informs ZAKO's per-component language decisions.

---

## Survey Results

### What Linux Itself Uses

| Component | Language | Notes |
|-----------|----------|-------|
| Linux kernel | C (GNU C dialect) | 32M+ lines. Rust entering since 6.1 for new drivers only. Core remains C. |
| systemd (PID 1, journald, networkd, udevd) | C | ~1.5M lines. No Rust, no C++. Pure C with custom allocators. |
| BusyBox | C | Single-binary userspace. Pure C, minimal libc dependency. |
| toybox (Android's BusyBox) | C | Android-native equivalent. Pure C. |
| musl libc | C | Clean-room libc. Pure C, POSIX-compliant. |
| Android init, logd, adbd | C++ | Android-specific. Uses AOSP's libbase. |
| Android system_server | Java | Framework-level. Not kernel/daemon level. |
| Kernel build system | Kbuild (Make + shell + Perl) | Build tooling only. Perl for scripts/checkpatch.pl, kernel-doc. |
| Android build system | Blueprint/Soong (Go), Make | Build tooling only. |

**Pattern:** All long-running system daemons in Linux are C. Build tooling uses whatever works (Make, shell, Perl, Go). The runtime path is always C.

### Defense/Aerospace Standards

| Standard | Domain | Language Requirements | Notes |
|----------|--------|---------------------|-------|
| MISRA C:2012 | Automotive, avionics, medical, industrial | C subset — ~175 rules restricting unsafe constructs | The industry standard for safety-critical C. No dynamic allocation, no recursion, restricted pointer arithmetic. |
| DO-178C (DAL A–E) | Airborne software (FAA/EASA) | Language-neutral but C dominates. Ada used in Europe. | Certification process, not a language standard. C + MISRA C is the common approach. |
| IEC 61508 | Industrial safety (SIL 1–4) | C or Ada recommended. C++ restricted. | Functional safety for programmable systems. |
| ISO 26262 (ASIL A–D) | Automotive | C per MISRA C:2012 | Automotive functional safety. |
| EAL 6-7 (Common Criteria) | Security evaluation | C, occasionally formal methods (Haskell/Isabelle for seL4 proof) | seL4's implementation is C; its proof is in Isabelle/HOL. |

**Pattern:** Defense and aerospace use C constrained by MISRA rules. The constraint is the safety layer — not the language choice itself. Ada is the European alternative but has smaller ecosystem. Rust has no DO-178C certification path yet (as of 2025/2026).

### Embedded Systems (IoT, Industrial, Automotive ECUs)

| Platform | Language | Notes |
|----------|----------|-------|
| Zephyr RTOS | C | 1M+ lines, MISRA-C informed |
| FreeRTOS | C | Dominant RTOS, pure C |
| NuttX (used in PX4 drones) | C | POSIX-like embedded, pure C |
| seL4 microkernel | C | Formally verified. Proof in Isabelle/HOL. |
| QNX (automotive, medical) | C/C++ | POSIX-certified RTOS |
| VxWorks (aerospace, defense) | C | DO-178B/C certified RTOS |
| AUTOSAR Classic | C | Generated from models (MATLAB/Simulink → C) |

**Pattern:** Every high-assurance embedded system is C. The pattern is universal: when failure means physical harm, the runtime is C.

---

## Language Decision Matrix for ZAKO

### Core Daemons (CRITICAL + INTERACTIVE class)

| Component | Decision | Rationale |
|-----------|----------|-----------|
| outstack-powerd | **C (MISRA-C subset)** | Safety-critical power governance. Must be auditable, deterministic, minimal. No dynamic allocation in steady-state. |
| telux-ledgerd | **C (MISRA-C subset)** | Integrity-critical. fsync-before-ACK. No memory errors permissible — but static analysis + fuzzing addresses this without Rust's toolchain cost. |
| telux-identd | **C (MISRA-C subset)** | Crypto-critical. Wraps well-audited C libraries (TweetNaCl/libsodium). No language-level complexity needed. |
| telux-sharedb | **C** | I/O-heavy, channel multiplexing. Standard Unix socket/serial programming. C is natural. |
| telux-coverd | **C** | Thin framebuffer driver daemon. Minimal. |

### Protocol Libraries

| Component | Decision | Rationale |
|-----------|----------|-----------|
| bitpads-codec | **C (MISRA-C subset)** | Wire-level codec. Byte manipulation. Must be embeddable in any context (daemon, kernel module, test harness). Pure C with no allocator dependency. |
| bitledger-codec | **C (MISRA-C subset)** | Same as above. Conservation invariant is arithmetic — C is natural. |
| BLAKE3 hash | **C** | Use the reference BLAKE3 C implementation (public domain, optimized). |
| ed25519 | **C** | Use TweetNaCl (single-file, audited) or libsodium (larger but standard). |

### Build & Test Tooling

| Component | Decision | Rationale |
|-----------|----------|-----------|
| Build scripts | **POSIX shell (sh)** | Maximum portability. No bash-isms unless necessary. Android's build system is already Make + shell. |
| Config generators | **Python 3** | For generating configs, processing specs, test orchestration. Not in the runtime path. |
| Test harnesses | **C (unit) + Python (integration)** | C unit tests (e.g., with greatest.h or similar single-header framework). Python for orchestrating multi-daemon integration tests. |
| Static analysis | **External tools** | Cppcheck, Coverity, or PVS-Studio for MISRA-C compliance checking. |
| Fuzz testing | **C + AFL/libFuzzer** | Fuzz the codec and daemon input parsers. Standard practice for C daemons. |

### Android Framework / Apps

| Component | Decision | Rationale |
|-----------|----------|-----------|
| Setup Wizard | **C + native rendering** | Bypasses JVM entirely. Instant launch. First impression of the device should demonstrate ZAKO's performance discipline. |
| T9 IME | **C core + minimal JNI shim** | Android's InputMethodService API requires a Java entry point, but the input logic, character mapping, and rendering can be native C behind a thin JNI bridge. |
| PADS app | **C + native rendering** | Core ZAKO app — must demonstrate radical efficiency. Direct communication with telux-ledgerd via Unix socket. No framework overhead. |
| Exchange app | **C + native rendering** | Same as above. Ledger queries, record display, NLQ engine — all native. |
| Sovereignty dashboard | **C + native rendering** | Same. Island management is a core ZAKO function, not a third-party app. |
| NLQ engine | **C** | Rule-based FSM. Already specified as native. |
| UI toolkit (`libzako-ui`) | **C** | Minimal rendering library for ZAKO native apps. Research sprint will determine whether this targets framebuffer, ANativeWindow, NativeActivity+EGL, or lvgl. |

### Third-Party App Support (APK Ecosystem)

| Component | Decision | Rationale |
|-----------|----------|-----------|
| F-Droid | **Pre-built APK** | Third-party app distribution. Runs on Android's standard runtime. |
| Organic Maps | **Pre-built APK** | Offline maps. Standard Android app. |
| Mull / browser | **Pre-built APK** | Web browsing. Standard Android app. |
| ntfy | **Pre-built APK** | Push notifications. Standard Android app. |

APK support is retained for the third-party ecosystem but ZAKO's own tools do not use it. The distinction: ZAKO apps are native C processes; third-party apps are JVM processes governed by Outstack.

### What ZAKO Does NOT Use

| Language | Why Not |
|----------|---------|
| Perl | No runtime role in modern embedded Linux. Kernel build system still uses it for scripts/checkpatch.pl but ZAKO doesn't need kernel code generation tools. |
| Rust | Toolchain complexity on ARM32. No DO-178C certification path. Binary size overhead. Compile time overhead. The safety benefits are real but achievable through MISRA-C + static analysis + fuzzing for our codebase size. Revisit for v2 if kernel module work expands. |
| Go | GC pauses unacceptable in CRITICAL daemons. Binary size too large for 16GB eMMC budget. |
| C++ | Complexity without proportional benefit for our use case. Feature-creep risk (templates, exceptions, RTTI). MISRA-C++ exists but C subset is simpler to audit. Android framework code is C++ but we don't modify it. |

---

## MISRA-C Subset for ZAKO

ZAKO daemons will follow a practical MISRA-C:2012 subset. Not full MISRA compliance (which is designed for automotive certification with formal tool qualification), but a discipline-focused subset:

### Adopted Rules

1. **No dynamic allocation in steady-state.** `malloc()`/`free()` permitted only during initialization. Once running, all buffers are pre-allocated.
2. **No recursion.** Stack depth must be statically determinable.
3. **No pointer arithmetic beyond array indexing.** No arbitrary pointer manipulation.
4. **All functions have explicit return types.** No implicit int.
5. **All variables initialized at declaration.**
6. **No goto except for error cleanup patterns.** Single-exit-point preferred, but Linux-style error cleanup with goto is acceptable.
7. **No variable-length arrays.** All arrays are compile-time sized.
8. **No implicit type conversions across signedness.** All casts explicit.
9. **All switch statements have default case.**
10. **No undefined behavior relied upon.** Compiler-specific behavior documented where used.

### Tools for Enforcement

- `cppcheck --enable=all --std=c99` on every build
- Clang's `-Weverything` with documented suppressions
- Coverity Scan (free for open source) for weekly deep analysis
- AFL++ fuzzing for all input-parsing code paths

---

## Summary Decision

**ZAKO's language profile:**
- Runtime daemons and protocol libraries: **C (MISRA-C subset)**
- User-facing ZAKO applications: **C (native rendering, bypasses JVM)**
- UI toolkit: **C** (minimal rendering library — lvgl, framebuffer, or ANativeWindow)
- Kernel modules (future): **C (Linux kernel style)**
- Build tooling: **POSIX shell + Python 3**
- Third-party apps: **APK/Kotlin** (F-Droid ecosystem, runs on Android runtime, governed by Outstack)
- T9 IME: **C core + thin JNI shim** (Android IME API requires Java entry point)
- Test: **C (unit) + Python (integration) + AFL (fuzz)**

This profile means ZAKO's entire critical stack — from the kernel through the daemons through the user-visible applications — is C. The JVM is present for third-party APK compatibility but ZAKO's own tools never touch it. The result: radical efficiency on constrained hardware.

---

*Research sources: Linux kernel (kernel.org), systemd (github.com/systemd), MISRA C:2012 (misra.org.uk), DO-178C (RTCA), seL4 (sel4.systems), Android source (android.googlesource.com), BusyBox (busybox.net)*
