# Babb OS — A Complete Account
*From a locked phone to a purpose-built operating system*

---

## Prologue: The Locked Phone

It started with a simple problem. A Cat S22 Flip — a ruggedized flip phone running Android Go — was locked behind a PIN that had been forgotten. The phone was physically fine. The hardware was intact. The software was just inaccessible.

The recovery was straightforward: hold Power and Volume Up at boot, reach the stock Android recovery menu, wipe the device. The phone came back to life, blank and clean, running the factory Android 11 Go install that Bullitt Group had shipped it with.

But that moment of looking at a freshly reset device — knowing that the stock software could be entirely replaced if you understood the stack deeply enough — is where this project actually began.

A reset phone is just a clean surface. The question is: what do you put on it?

---

## Part One: What's Inside

Before writing a single line of software, the first task is understanding the hardware. Not in the marketing sense — not "4G LTE, 4.0-inch display, 2GB RAM" — but in the engineering sense. What chip is this? What version of what kernel? What bootloader? What partition layout? What blobs does the firmware require to boot? Where does the chain of trust begin and where does it end?

The Cat S22 Flip is built on the Qualcomm QM215, which is the commercial product name for what Qualcomm internally calls the MSM8937. The distinction matters: MSM8937 is the SoC identifier used throughout the kernel source, device trees, firmware paths, and build system. Every document in the kernel tree, every CAF (Code Aurora Forum) tag, every reference design uses MSM8937. Learning to think in this identifier is the first lesson.

The chip is a 32-bit ARM Cortex-A53 design — four cores at 1.4GHz, Adreno 308 GPU, built on a 28nm process. It is not a new chip; it was released in 2015 and has been used in hundreds of budget Android devices since. This is actually good news for a custom OS project. A chip that has been in the wild for a decade has a vast community knowledge base, a mature kernel support story, and multiple reference devices. The Qualcomm MSM8937 is one of the best-documented budget SoCs in the Android ecosystem. The Xiaomi Redmi Go, the Motorola Moto E5 Play, and several other devices have active custom ROM communities built on this exact chip.

### The Boot Chain

Understanding how the device boots is fundamental to everything else. On the MSM8937, the boot sequence follows a strict chain:

The **Primary Boot Loader (PBL)** is burned into ROM on the SoC itself. It cannot be changed. It runs before anything else and its only job is to load the next stage from eMMC.

The next stage is **SBL1** — the Secondary Bootloader — stored in the `sbl1` partition. SBL1 initializes RAM, sets up the clock tree, and loads the QSEE (Qualcomm Secure Execution Environment), which is TrustZone — a separate ARM execution mode that runs alongside the normal world and handles cryptographic operations, key management, and secure attestation.

After TrustZone is initialized, the **RPM** (Resource Power Manager) firmware comes up. RPM is a separate microcontroller on the SoC that manages power rails, clocks, and sleep states completely independently of the main ARM cores. It is why the modem can stay connected and the device can receive calls while the main CPU is in deep power collapse.

Then comes **aboot** — the Application Bootloader, implemented as LK (Little Kernel). This is the bootloader you interact with when you hold Volume Down at boot. It presents the fastboot protocol over USB, implements the Verified Boot chain, and loads the Linux kernel from the `boot` partition. aboot is where OEM unlock happens. It is where AVB (Android Verified Boot) is checked. It is where the kernel command line is assembled. aboot is the gatekeeper.

Then: **Linux kernel 4.9**, the `init` process, mounting the vendor and system partitions, bringing up HAL services, and eventually the Android runtime.

Understanding this chain answers the question that every custom ROM builder asks first: where can I insert myself? The answer for the MSM8937 is: you can replace everything from aboot's payload onward. You cannot replace PBL, SBL1, or the TrustZone trustlets — those are cryptographically locked by Qualcomm and Bullitt. But the kernel, the recovery, the system image, the vendor image — everything that aboot loads — is replaceable.

### The Partition Table

The Cat S22 Flip uses a dynamic partition scheme on top of eMMC. There is no A/B (seamless update) slot system; this is an A-only device, which simplifies the partition map but means OTA updates require recovery-mode sideloading rather than background installation.

The partition structure has two tiers. The first tier is the traditional fixed partition table: `sbl1`, `rpm`, `tz` (TrustZone), `aboot`, `boot` (32MB), `dtbo` (8MB), `vbmeta` (a few kilobytes — just the verification metadata), `recovery` (32MB), `userdata`, and one giant `super` partition at 8.5GB.

The second tier is inside `super`. Rather than fixed logical partitions, `super` uses Android's dynamic partition scheme — a software-defined volume manager (`dm-linear`) that carves `super` into logical volumes at boot. The volumes are `system`, `vendor`, and `product`. Their sizes are not fixed in the partition table; they are defined in the `super` partition metadata and can be adjusted at flash time. This is important for the custom OS build: the logical partition sizes need to match what the build system produces, not what the stock firmware used.

This partition intelligence — the exact sizes, the filesystem types, the partition names, the confirmed fact that recovery is 32MB not 64MB as an auto-generated TWRP tree incorrectly assumed — is the kind of ground truth that prevents hours of debugging later. A recovery image built for a 64MB partition that tries to flash into a 32MB slot does not fail gracefully. It corrupts the partition. This one correct number, verified against the actual firmware dump, is worth significant time.

### The Vendor Blob Inventory

The MSM8937 runs Linux in the "normal world" but depends heavily on proprietary firmware and HAL (Hardware Abstraction Layer) binaries in the vendor partition to make the hardware work. There is no open-source GPU driver for the Adreno 308. There is no open-source QMI modem driver. There is no open-source Keymaster implementation for this platform. These are binary blobs — compiled ARM code, often for multiple ABI targets, without source.

The vendor partition for the Cat S22 Flip contains approximately 2,900 files. The cataloguing of these — understanding which blobs serve which hardware function, which are boot-critical versus enhancement-only, which have known open alternatives — is not glamorous work, but it is the map of what you depend on and cannot replace. Among the 2,900 files are 56 firmware binary files (loaded into hardware subsystems at boot), 27 HAL binary implementations, the GPU kernel module and userspace EGL/GLES libraries, the complete audio DSP firmware, and the Keymaster 4.0 TrustZone trustlet.

One discovery during this cataloguing stood out: the presence of AW881xx firmware files from Awinic, a Chinese semiconductor company. The `aw881xx_*.bin` files revealed that the Cat S22 Flip uses an Awinic smart amplifier for its speaker — not a commodity audio component. Smart amplifiers have DSP firmware that must be loaded at runtime. If the audio machine driver doesn't correctly identify the I2C address and load the firmware, the speaker initializes at reduced capability or not at all. This was not in any prior research on the device — it emerged from methodically reading the vendor partition.

This is the rhythm of hardware bring-up: patient archaeology surfaces facts that change the engineering plan.

---

## Part Two: The Bootloader Decision

Once you understand the boot chain, you face the central decision of any custom OS project: how do you establish trust between the hardware and the software you're putting on it?

On modern Qualcomm devices — Snapdragon 4-series and above, using UEFI ABL — the answer is elegant: generate your own RSA-4096 signing key pair, flash the public key to a dedicated `avb_custom_key` partition, and re-lock the bootloader. The device then boots only images signed with your key. The chain of trust is reestablished under your authority. This is what GrapheneOS does with Pixel devices, and it is the gold standard.

On the MSM8937, this is not possible. The bootloader is LK (Little Kernel), an older codebase predating the UEFI ABL architecture. LK implements its own Verified Boot check but does not expose a mechanism to flash a custom public key. The OEM key is embedded in the signed aboot image, which is signed by Bullitt Group's key, which we do not have. There is no `fastboot flash avb_custom_key`. There is no path to re-locking the bootloader under our authority.

This is not a failure of the project — it is a known characteristic of this platform generation. The path forward is to unlock the bootloader, accept "orange state" (the AVB term for an unlocked device), and understand exactly what that does and does not compromise.

### What Orange State Means in Practice

Unlocking the bootloader via `fastboot oem unlock` does several things:

It wipes userdata — required, and expected. It sets a flag in the `devinfo` partition (not an eFuse, which would be permanent) that tells aboot the device is in orange state. On subsequent boots, aboot shows a warning screen: the orange padlock, the "Your device software can't be verified" message, a 5-second countdown. Then it boots normally.

Critically: orange state does not disable dm-verity, the system that continuously checks the cryptographic integrity of the system, vendor, and product partitions at the block level. dm-verity runs whether or not the bootloader is locked. Any tampering with system partition blocks after boot is detected and causes a kernel panic. The integrity of the running OS is maintained even in orange state.