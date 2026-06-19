# Babb OS — Architecture & Technical Approach
*Two-page briefing for technical readers*

---

## The Device

The Cat S22 Flip runs a Qualcomm QM215 (MSM8937) system-on-chip — a quad-core ARM Cortex-A53 at 1.4GHz with an Adreno 308 GPU, 2GB RAM, and 16GB eMMC storage. It is a 32-bit ARM device running a 32-bit kernel, though the binder IPC layer operates in 64-bit mode for compatibility with modern Android. The platform is mature, well-documented, and has a Linux kernel base (4.9 LTS) that is manageable for a small team.

The flip form factor is real hardware: a physical hinge with a SW_LID sensor that the OS reads, an internal 4" primary display and an external 1.44" cover display (functional in stock firmware, driver bring-up pending for initial custom OS release), a physical keypad, and a proximity sensor behind the earpiece. Each of these requires deliberate handling in the OS layer.

---

## The Software Stack

Babb OS is built on the Android Open Source Project (AOSP) at the Android 11 Go level — the same Android version as stock, which means all driver blobs are version-compatible. The stack from bottom to top:

**Bootloader:** The stock LK (Little Kernel) bootloader, unlocked via `fastboot oem unlock`. This changes the device to AVB "orange state" — the bootloader shows an unlock warning on boot but does not prevent loading custom images. dm-verity (partition integrity checking) and FBE (file-based encryption) both continue to function. Re-locking with custom keys is not possible on this platform; orange state is the production posture.

**Kernel:** Linux 4.9 built from Qualcomm CAF (Code Aurora Forum) source at tag `LA.UM.10.6.2.r1-02500-89xx.0`, cross-compiled with Clang for the ARM target. The Xiaomi Redmi Go (also MSM8937, Android 11) serves as the primary reference for device-specific patches. Development uses a prebuilt kernel approach initially, transitioning to a fully compiled kernel once the build environment is stable.

**Vendor HAL layer:** ~2,900 proprietary files extracted from stock Bullitt firmware. These blobs implement hardware interfaces that have no open-source equivalent: GPU driver (Adreno 308), modem (QMI/RIL), camera, audio DSP (LPASS/Hexagon), and security (Keymaster 4.0 / TrustZone). This layer is binary-only and cannot be replaced.

**Android framework:** Unmodified AOSP with Go-edition memory constraints applied. APEX module flattening saves ~200MB of storage. The framework provides telephony, WiFi, Bluetooth, sensors, camera, and all other platform services.

**Application layer:** No Google Mobile Services. No Play Store. A curated set of open-source applications covering every user-facing function: F-Droid (app distribution), AOSP Dialer (calls/USSD/STK), K-9 Mail (email), Organic Maps (offline maps), Aegis (2FA), UnifiedPush + ntfy (notifications without GMS). The Google Setup Wizard is replaced with a Babb-specific first-run experience.

---

## Key Engineering Decisions

**No GMS, no compromise.** Removing Google Mobile Services eliminates ~60–120mW of persistent background power draw, removes all data transmission to Google endpoints, and eliminates the dependency on Google infrastructure for basic phone functions. The tradeoff is that applications requiring GMS APIs must be replaced. Every application in the default stack has been selected to have no GMS dependency.

**Power as a design constraint.** The device has a 1450mAh battery — approximately 5.4Wh. Every subsystem has a documented power budget and configuration target. The LTE modem is configured for eDRX (Extended Discontinuous Reception) with up to 327-second paging cycles when idle. The CPU governor is schedutil with core parking. Display default brightness is 50% with a 30-second timeout. Collectively, these bring the deep idle power floor to under 50mW and the LTE standby envelope to under 100mW — meaningfully extending usable battery life in a market where charging infrastructure is irregular.

**Telephony first.** USSD and STK are the entry points to mobile money, which is the primary financial system in Zambia. These work through standard Android telephony with no modifications required — but they must be tested on live carrier SIMs before any release is considered complete. VoLTE depends on carrier-side provisioning and proprietary IMS blobs; CSFB (2G/3G voice fallback) is the guaranteed baseline.

**Build reproducibility.** The project maintains a device tree, kernel tree, and vendor blob set in separate git repositories under a manifest. Anyone with the manifest can reproduce the build. Signed release packages use the same AVB key hierarchy for every build.

---

## What comes next

The first meaningful milestone is a bootable image that reaches Android home screen on a single Zambia carrier SIM. From there: USSD verification on all three carriers, hardware peripheral validation (keypad, sensors, audio), and the first signed OTA update. The build infrastructure is defined; the document corpus covers every phase of that path.

---

*For the Zambia deployment specifics — carriers, mobile money, language, climate — see `03-zambia.md`. For a full technical and strategic picture, see `05-full-brief.md`.*
