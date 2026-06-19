# TeluxOS Bedrock Purity Assessment
## How Purely Can We Implement Telux and Outstack Bedrock Concepts on the CAT S22 Flip?

*May 31, 2026*

---

The Telux/Outstack bedrock has three declared components:

> **Bedrock Layer:** LSM module (Telux-SEC), immutable audit trail, TPM/HSM key storage

Outstack adds a fourth implicit one: **the power co-processor as a physically separate enforcement domain.**

This document assesses how purely each concept can be realized on the CAT S22 Flip's specific hardware — QM215 (MSM8937), Linux kernel 4.9 CAF, Keymaster 4.0 in TrustZone, LK bootloader in orange state.

---

## 1. HSM / Key Storage — ~95% Pure

This is the strongest one. Keymaster 4.0 in TrustZone is a genuine hardware security boundary. ed25519 private keys for Island sovereignty, DID operations, and ledger signing never appear in normal-world memory. Signing operations happen inside the Secure Execution Environment; the kernel calls Keymaster via a HAL; the result comes back signed. Private key material is never exposed.

The only gap: the bootloader is in orange state, so third-party attestation — proving to an external party that your boot chain is trusted — fails. TrustZone still works correctly for your own cryptographic operations. It just cannot prove itself to outsiders via Android's attestation mechanism. For a sovereign-first device where you are the root of trust rather than Google, this matters less than it sounds.

**Verdict:** Keymaster 4.0 on this hardware is a legitimate bedrock HSM. Use it without apology.

---

## 2. Immutable Audit Trail — ~80% Pure

Two distinct parts:

**dm-verity** (system/vendor/product partitions): genuinely immutable at block level, enforced in kernel, running on the stock device and carried forward into Babb OS. Any tampering with system partition blocks after boot causes a kernel panic. This is real bedrock-level immutability for the OS itself.

**Exchange ledger** (the Telux record chain): currently specified as application-layer SQLite with chain-hashing and Keymaster-backed signing. The cryptographic guarantees are sound — a tampered record breaks the hash chain, the signature is unforgeable. But the database file itself can be deleted or corrupted by root. This is application-layer integrity, not block-level immutability.

The gap is closeable: IMA (Integrity Measurement Architecture) is available on kernel 4.9. Measuring each write to the ledger partition via IMA gets the ledger to approximately 90% purity. Full block-level immutability for a writable ledger would require a custom kernel-mediated append log, which is a meaningful engineering investment but not architecturally blocked.

---

## 3. LSM (Telux-SEC) — 60% as SELinux Extension; ~75% with a Minimal Custom LSM

The prior research document undersold what is actually possible here.

**What the Track 1 document said:** Writing a custom LSM for kernel 4.9 carries significant ongoing maintenance burden — implying it is too costly.

**What is actually true:** A *minimal* Outstack LSM for kernel 4.9 is approximately 200–300 lines of C. The LSM framework in 4.9 exposes `security_bprm_check()` — that is the exec hook Outstack needs. A module that only implements exec gating and Island file permission checks does not need to be a full-featured LSM. The maintenance burden is real but bounded: this is not rewriting SELinux, it is adding one hook.

The implementation shape:

```c
static int outstack_bprm_check(struct linux_binprm *bprm) {
    // read power class from xattr on executable
    // query current outstack mode from kernel ring buffer
    // return -EPERM if class not permitted in current mode
}
```

The harder problem is kernel-to-userspace communication for power mode state. The cleanest solution is a small `/proc/outstack/mode` entry that `outstack-powerd` writes to (gated by `CAP_SYS_ADMIN`) and the LSM reads from. Approximately 50 additional lines.

**What SELinux extension does and does not do:** SELinux enforces Island boundary isolation — which processes can read which files, which services are callable by which domains. That is the access control part of Telux-SEC. What it cannot do is power-gated enforcement or exec-time power class checks. A minimal custom LSM adds exactly those two things.

The correct approach is to run both: SELinux for access policy enforcement, Outstack LSM for power-domain execution gating. Android kernel 4.9 allows multiple LSMs via the `security_hook_heads` framework. Qualcomm-specific caveats apply and must be tested, but this is not blocked in principle.

---

## 4. The Power Co-Processor — Already Present, Not Programmable

This is the most important finding and was not surfaced clearly in the prior documents.

**The QM215 RPM microcontroller is structurally identical to the NXP i.MX8M Mini's Cortex-M4.** It is a physically separate processor, always-on, managing power rails and sleep states independently of the main ARM cores. It is why the modem receives calls while all four Cortex-A53 cores are in Power Collapse. It is why the device wakes from deep sleep in under two seconds. The RPM firmware (`rpm.mbn`) is loaded during boot chain before Linux and runs independently thereafter.

The difference from the i.MX8M Mini M4: you cannot upload custom firmware to the RPM. Qualcomm's RPM firmware is signed and immutable. You can *command* it through the kernel's SPMI bus and `qcom,rpmpd` power domain driver — but you cannot run Outstack power management *on* the RPM itself.

**What you can do:**

- Request power domain state changes via `qcom,rpmpd` — the kernel driver exposes this
- Power-gate the WCNSS (WiFi) subsystem via RPM power domain request — this is real hardware power gating, not software disable
- Power-gate the camera subsystem similarly
- The modem is more complex; RPM manages it in coordination with modem firmware, but PSM (Power Saving Mode) and eDRX are controllable

**What you cannot do:**

- Run the Outstack five-mode state machine on the RPM itself
- Make the RPM execute custom logic
- Override the RPM's own power sequencing decisions

**The practical ceiling:** `outstack-powerd` runs on the A53 cores and commands the RPM via kernel interfaces. The RPM executes the commands. This is one level of indirection from the ideal — Outstack firmware running natively on the co-processor — but the hardware power gating is real. A WiFi chipset with its power domain disabled via `qcom,rpmpd` is physically without power. That is Outstack's physically unforgeable revocation even if the command path went through a daemon rather than RPM firmware.

---

## 5. Hardware Power Gating as Access Revocation — ~65% Pure

Outstack's central claim about power gating:

> Software isolation can be bypassed by software. Hardware power isolation cannot be bypassed by software. A peripheral with no power has no attack surface.

On the CAT S22, this property is partially realized. The `qcom,rpmpd` driver allows requesting that specific power domains be taken down. When the WiFi subsystem is power-gated through this mechanism, the WCNSS chip is physically without power. A compromised process in userspace cannot undo this — the kernel's power domain management, controlled by `outstack-powerd` via `CAP_SYS_ADMIN`, is the only path back.

What weakens purity to 65%: the command that performs the gating originates in the A53 world. A kernel exploit could in principle call `rpmpd` directly and restore power to a gated peripheral. On the i.MX8M Mini, the M4 co-processor makes this decision in a physically isolated context; on the QM215, the RPM enforces it but was commanded by A53. The enforcement is real; the command authority is not fully isolated.

For the modem: modem power gating is more constrained. The modem must coordinate PSM entry with the network and the Linux RIL layer. Full modem power-gating as a security revocation is not safely achievable on this platform.

---

## 6. Bootloader Chain of Trust — ~40% Pure

The LK (Little Kernel) bootloader on the QM215 cannot be re-locked with a custom public key. The OEM key is embedded in the signed aboot image, which is signed by Bullitt Group's key. There is no `fastboot flash avb_custom_key`. The device is permanently in orange state.

What this means:

- dm-verity is active and enforced — the running OS is block-level integrity-checked
- FBE is active — user data is encrypted with hardware-backed keys
- SELinux is enforcing
- Keymaster 4.0 operates correctly in TrustZone
- Third-party attestation fails — no external party can verify the boot chain cryptographically

For a prototype validating the Telux architecture internally, the 40% purity here does not impede any of the sovereignty, ledger, or Island concepts. For a device making external security claims — "this device's OS has not been tampered with" — the orange state is a hard ceiling that requires different hardware to overcome.

---

## Summary Table

| Bedrock Concept | Purity on CAT S22 | Mechanism | Limiting Factor |
|-----------------|:-----------------:|-----------|-----------------|
| HSM / key storage | **~95%** | Keymaster 4.0 in TrustZone | Orange state attestation only |
| Immutable OS | **~90%** | dm-verity (block-level, active) | None — already working |
| Immutable ledger | **~75%** | Chain-hash + Keymaster signing + IMA | App-layer; not block-level |
| Island isolation (access) | **~75%** | SELinux type enforcement | No cgroups v2, no user namespaces |
| Exec gating (power class) | **~75%** | Minimal custom LSM (~300 lines) | Kernel module maintenance burden |
| Hardware power gating | **~65%** | `qcom,rpmpd` via kernel driver | RPM firmware closed; A53 commands it |
| Power co-processor | **~50%** | RPM present and commandable | Cannot run Outstack on RPM directly |
| Bootloader chain of trust | **~40%** | dm-verity active but orange state | LK cannot re-lock with custom keys |

---

## The Honest Ceiling

The CAT S22 gives genuine bedrock for the cryptographic and immutability properties: TrustZone-backed keys, dm-verity immutable OS, real hardware power-domain gating for peripherals. These are not simulated. They are hardware properties of this specific chip.

The ceiling is the RPM firmware and the bootloader. The RPM is already doing exactly what Outstack's bedrock power layer needs — it is a separate always-on processor managing hardware sleep and wake — but it is commanded through a kernel API rather than running Outstack logic natively. And the orange state bootloader means external attestation is unavailable: you are sovereign over the device to yourself, but cannot prove it to others.

**For a prototype that validates the architecture:** this is sufficient. The cryptographic guarantees of sovereignty, exchange records, and DID identity are sound. The power management is real hardware gating even if the daemon runs on A53. The LSM gap is closeable with a focused kernel module smaller than many Android HAL implementations.

**For a production system making external security claims:** the open hardware track is the correct answer — specifically the NXP i.MX8M Mini variant, where the M4 co-processor runs Outstack firmware natively and the bootloader accepts custom signing keys.

**The right framing:** the CAT S22 is not a compromise prototype that approximates the Telux architecture. It is a prototype that *realizes* the Telux architecture at the cryptographic and policy layers, with hardware power gating as a genuine (if commanded rather than autonomous) property, and with a known, bounded gap at the bootloader attestation layer. That gap is well understood, documented, and has a clear upgrade path. Everything else is buildable on this hardware today.

---

*Cross-reference: `TeluxOS-AOSP-Prototype.md` for the full integration architecture. `TeluxOS-OpenHardware-Prototype.md` for the hardware track that removes the RPM and bootloader ceilings.*
