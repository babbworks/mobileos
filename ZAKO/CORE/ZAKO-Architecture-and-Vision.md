└─────────────────────────────────────────────────────────────┘
```

The Bedrock Layer is hardware and kernel. It cannot be overridden by userspace software. It provides the guarantees that make the Submerged Layer's promises credible. A key stored in TrustZone cannot be extracted by application code. A system partition integrity-checked by dm-verity cannot be silently modified. A peripheral power-gated by the hardware power domain controller cannot be accessed by software running on the main processor. These are physical facts, not policy.

The Submerged Layer is where ZAKO's intelligence lives. Outstack governs the device's power state and enforces the relationship between power mode and process execution. Telux manages Islands, newgroups, sovereignty grants, and the exchange ledger. The identity service manages the DID ecosystem that gives every entity — human, machine, AI, or service — a self-sovereign identity. The Submerged Layer reads from the Bedrock Layer and expresses policy upward.

The Visible Layer is the surface: the interfaces that humans and AI entities use to interact with the system, the telephony stack that makes calls work, the application runtime that hosts the tools people need. The Visible Layer is built on AOSP — the Android Open Source Project, the most widely deployed and most comprehensively tested open mobile application platform in the world. ZAKO uses AOSP not because Android is perfect but because it is proven, because it runs on the hardware that exists, because its build infrastructure is mature, and because its open-source components can be audited, modified, and maintained without dependency on any single commercial entity.

---

## Part III: Why AOSP

The decision to build ZAKO on AOSP requires explanation, because the association of Android with Google is so strong that choosing AOSP as a substrate feels like a contradiction for a sovereignty-first operating system.

It is not a contradiction. It is a careful architectural choice.

AOSP — the Android Open Source Project — is a large body of open-source code licensed under the Apache License 2.0 (application layer) and GPL v2 (kernel layer). It includes: a complete Linux-based kernel build infrastructure, a hardware abstraction layer specification (HIDL/AIDL), a telephony framework that handles voice calls, SMS, and data connectivity across any carrier-compliant modem, a display compositing system (SurfaceFlinger), a window management system, an application runtime (ART), a package management system, a permission model, a security framework (SELinux, dm-verity, Android Verified Boot, File-Based Encryption), and several thousand other components.

None of these components require Google's involvement to function. They require only the appropriate hardware support layer and the correct build configuration. The entire AOSP codebase is available at android.googlesource.com under open licenses. It can be cloned, built, modified, and deployed without Google's permission, participation, or knowledge.

What AOSP does not include is Google Mobile Services (GMS): the Play Store, Play Services, Google Maps, the Firebase Cloud Messaging infrastructure, the Google Account system, and the dozens of background processes that constitute Google's relationship with the device. GMS is Google's proprietary addition to the open base. ZAKO does not include GMS. ZAKO does not include any component of GMS. ZAKO's build system is configured to explicitly exclude GMS components and to replace their functions with open alternatives or with Telux-native alternatives.

The removal of GMS is not a compromise ZAKO makes reluctantly. It is ZAKO's founding design decision. A device running GMS cannot be sovereign, because GMS's persistent connections to Google's infrastructure mean that the device continuously reports to a party that is not the device's user, that the power budget is continuously allocated to that reporting, and that the user's applications and their data are continuously mediated by Google's identity and permission infrastructure. Removing GMS removes all of this. ZAKO operates in a world where the device's only relationships are the ones its sovereign has explicitly authorized.

What GMS removal requires ZAKO to replace:

**Push notifications:** GMS uses Firebase Cloud Messaging (FCM), a persistent connection to Google's servers that routes all application push notifications. ZAKO uses UnifiedPush — an open protocol that allows each application to receive notifications via a provider of the user's choice, including self-hosted providers. The notification path belongs to the user's Island, not to Google's servers.

**Application distribution:** Google Play is replaced with F-Droid, installed as a privileged system application so it can perform silent updates without additional user permission grants. F-Droid serves only applications whose source code is publicly available and auditable. ZAKO additionally supports direct APK installation and organizational application distribution outside any app store.

**Location services:** Google's Fused Location Provider is replaced with the AOSP standard GPS and network location providers. Maps are provided by applications using OpenStreetMap data.

**Authentication:** The Google Account infrastructure is replaced with the Telux identity layer: W3C DID-based identities backed by hardware keys. Applications that require authentication receive capability tokens from the island's identity service rather than from Google's OAuth endpoints.

**Device attestation:** Google's SafetyNet and Play Integrity APIs are not available. Attestation for ZAKO devices is provided by the hardware security module's direct attestation certificate, scoped to the Island's trust model. This is a narrower attestation claim than SafetyNet — it attests the key's hardware provenance, not the entire boot chain's Google-verified status — but it is honest about what it can verify.

The principle that guides every GMS replacement decision is: the replacement must be more sovereign, not merely different. F-Droid is not chosen because it is "better than" the Play Store on features. It is chosen because its application catalog is built from source code that anyone can audit, and because the distribution relationship does not require any user data to be transmitted to a central commercial party.

### The Upstream Compact

ZAKO is committed to maximum upstream alignment for every component it depends on. This is not merely a philosophical preference. It is a practical survival strategy.

Every deviation from upstream — every patch that is not submitted to and accepted by an upstream project — is a maintenance obligation that ZAKO carries indefinitely. A patch to the Linux kernel that adds Outstack's execution gating hook must be rebased every time the kernel moves forward. A patch to AOSP's telephony framework that adds Telux routing must be rebased every time AOSP moves forward. If these patches are not maintained, the gap between ZAKO's kernel and the upstream kernel grows until forward-porting becomes a multi-person project.

ZAKO's approach to this is to distinguish between two kinds of modification:

**Additions:** New system services (Outstack, Telux daemons), new SELinux domains, new device configurations, new applications, new OpenRC scripts — anything that adds to the system without modifying existing components. Additions do not create rebasing obligations. They live alongside the upstream code and require only integration testing as upstream moves forward.

**Modifications:** Changes to existing kernel code, existing AOSP framework code, or existing system service behavior. Every modification is considered a debt. ZAKO carries it temporarily and works to either upstream it (submit to the mainline kernel or AOSP project) or eliminate the need for it.

The target state is: ZAKO is a set of packages layered on top of unmodified AOSP and an unmodified LTS kernel. The kernel configuration (`defconfig`) is ZAKO's own. The SELinux policy is ZAKO's own. The system services are ZAKO's own. The AOSP framework and kernel source are pulled from upstream at every release without modification.

This target is not fully achievable on all hardware. Some platforms require out-of-tree kernel modules for hardware support. ZAKO accepts these where necessary and tracks them as explicit technical debt with a plan for resolution (either upstream contribution or hardware transition to a platform with better mainline support). The debt is named and owned, not accumulated silently.

---

## Part IV: Outstack — The Beating Heart

Outstack is not a power management system. Power management systems manage power. Outstack governs everything through power.

The distinction matters. A power management system attempts to reduce battery consumption by slowing down processes, dimming displays, and suspending background tasks. It operates at the margins of the system's behavior, trying to recover efficiency from a system designed with different priorities. Outstack is not a margin operation. Outstack is the organizing principle of the entire runtime. Every process, every service, every Island, every exchange operation is understood by Outstack in terms of its power demands, its power budget allocation, and its behavior when power is scarce.

This is not unprecedented in computing history. The aerospace industry has operated this way for decades. A spacecraft running on radioisotope thermoelectric generators (RTGs) cannot assume infinite power. Every subsystem has a power budget. Every operation is evaluated against that budget. The system is designed from the beginning to degrade gracefully as power constraints tighten, shedding lower-priority functions while preserving core capabilities. Spacecraft are the most reliable computing systems humans have ever built, and they are reliable in part because they were designed around power constraints rather than treating them as an afterthought.

ZAKO brings this discipline to terrestrial mobile devices. The specific power constraints are different — a 3000mAh lithium cell rather than an RTG — but the design discipline is the same.

### The Five Modes

ZAKO's runtime exists in one of five system-wide power modes at all times. These are not power profiles that the user selects. They are operational states that the Outstack daemon determines from objective measurements of the device's actual power situation.

**FULL:** The device is connected to external power, or the battery is above 80%. No power constraints apply. All services operate normally. This is the mode for charging, for intensive tasks, for environments where power is reliably available.

**NORMAL:** Battery between 60% and 80%. Normal operation with mild background efficiency measures active. eDRX enabled on the modem. Background sync intervals extended. No user-visible impact.

**CONSERVE:** Battery between 20% and 60%. Background services for lower-priority Islands are suspended. Deferred and opportunistic process classes receive reduced CPU allocation. The modem uses longer eDRX cycles. The display brightness ceiling is lowered if the current brightness exceeds it. The user retains full access to active Islands and their primary functions.

**CRITICAL:** Battery between 5% and 20%. Only processes in the CRITICAL and INTERACTIVE power classes continue to execute. Deferred and background processes are frozen, not killed — their state is preserved, and they will resume when the mode improves. The modem enters maximum eDRX. WiFi is suspended between active connections. The cover display is blanked. All Islands except the primary sovereign Island are suspended.

**EMERGENCY:** Battery below 5%. The system enters a survival posture. Only processes classified CRITICAL execute. The display duty cycle is reduced. The modem maintains basic paging capability (incoming calls and SMS will be received). The system records its current state — all Island membership, open capability tokens, pending ledger entries — to durable storage. If the device loses power entirely, this state will be restored on the next boot.