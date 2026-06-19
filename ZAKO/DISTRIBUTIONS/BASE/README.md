# ZAKO Distribution BASE Seed

This folder is the universal starting point for every ZAKO distribution. When you begin work on a new distribution for a new device, copy this entire folder into the distribution's root directory and customize each file.

---

## What BASE Is

BASE contains the documentation, procedures, and templates that are identical (or nearly identical) across all ZAKO distributions regardless of device. These are the files that would otherwise be duplicated from one distribution to the next with only surface-level changes.

BASE is **not** a substitute for reading the relevant ZAKO specifications. It assumes the builder has read:
- `ZAKO/CORE/ZAKO-Architecture-and-Vision.md`
- `ZAKO/CORE/ZAKO-Standard-v1.md`
- `ZAKO/PROTOCOLS/Telux-Protocol-v1.md`
- `ZAKO/PROTOCOLS/Outstack-Protocol-v1.md`

---

## Folder Contents

```
BASE/
├── README.md                      ← this file
├── briefings/
│   └── 01-vision-template.md      ← fill in to become the distribution's vision brief
├── build/
│   └── build-environment.md       ← AOSP host setup — identical across all distributions
├── os/
│   ├── de-googling.md             ← GMS removal — universal; adapt captive portal + DNS
│   └── security-model.md          ← Security baseline — universal; fill in device specifics
└── project/
    ├── contributor-guide.md       ← Universal contribution process — adapt component names
    ├── known-issues.md            ← Empty template — fill in as issues are discovered
    ├── regression-log.md          ← Empty template — fill in during testing
    ├── release-process.md         ← Universal release workflow — adapt commands + paths
    └── test-plan-template.md      ← Universal test structure — fill in device-specific sections
```

---

## How to Start a New Distribution

**Step 1.** Create a new distribution folder alongside the existing ones:
```
MobileOS/
├── ZAKO/
├── CatFlip/        ← Ya distribution
└── [NewDistro]/    ← your new distribution folder
```

**Step 2.** Copy BASE into the new folder:
```bash
cp -r MobileOS/ZAKO/DISTRIBUTIONS/BASE/* MobileOS/[NewDistro]/
```

**Step 3.** Create a distribution profile:
```bash
cp MobileOS/ZAKO/DISTRIBUTIONS/distribution-profile-template.md \
   MobileOS/ZAKO/DISTRIBUTIONS/PROFILES/[DistroName]-[DeviceCodename].md
```
Fill in all `[REQUIRED]` fields before any build work begins.

**Step 4.** Work through `ZAKO/DISTRIBUTIONS/distribution-checklist.md` phase by phase.

---

## What BASE Does NOT Contain

These are device-specific and must be created fresh for each distribution:

- `hardware/` — device-profile, partition-map, bootchain, fastboot-edl-reference
- `kernel/` — kernel-source, kernel-build, kernel-modules, device-tree, kernel-patches
- `bootloader/` — bootloader-unlock, custom-recovery, avb-signing
- `os/flip-form-factor.md` or other device-specific OS features
- `repos/` — actual kernel clones, firmware dumps, device trees
- `project/BabbCat-Implementation-Plan.md` — distribution implementation plan
- `project/research-plan.md` — device research phases
- `project/gaps-and-blind-spots.md` — gaps analysis for this device

---

## Customization Markers

BASE files use `[PLACEHOLDER]` markers for content that must be replaced. Search for `[` after copying to find all placeholders:

```bash
grep -r '\[' MobileOS/[NewDistro]/ --include="*.md" -l
```

Common placeholders:
- `[DISTRO_NAME]` — distribution name (e.g., "Ya")
- `[DEVICE_NAME]` — device model name (e.g., "Cat S22 Flip")
- `[DEVICE_CODENAME]` — short build code (e.g., "S22FLIP")
- `[DEVICE_VENDOR]` — AOSP vendor path segment (e.g., "cat")
- `[AOSP_BRANCH]` — AOSP git branch/tag (e.g., "android-11.0.0_r46")
- `[MARKET]` — target region or country
- `[OTA_SERVER]` — OTA update server URL
