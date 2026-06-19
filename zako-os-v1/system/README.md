# ZAKO OS v1 — System Engineering Directory

Active development workspace for building ZAKO OS toward an operational sovereign mobile operating system.

## Purpose

This directory holds all planning, research, architecture, and implementation coordination for ZAKO OS v1. It is the single source of truth for the engineering effort.

## Structure

```
system/
├── README.md                    ← this file
├── 00-project-plan.md           ← master plan and schedule
├── 01-component-inventory.md    ← every distinct system element
├── 02-language-research.md      ← C purity analysis, language choices per component
├── 03-aosp-linkage-points.md    ← AOSP elements requiring customization
├── components/                  ← per-component engineering projects
│   ├── outstack-powerd/
│   ├── telux-ledgerd/
│   ├── telux-identd/
│   ├── telux-sharedb/
│   ├── bitpads-codec/
│   ├── bitledger-engine/
│   └── ...
├── research/                    ← research outputs (language, prior art, defense sector)
└── decisions/                   ← architectural decision records
```

## Tracking

All work is tracked in workwarrior:
- Instance: `~/wwv02`
- Profile: `zako-os-v1`
- Task tags: `+outstack`, `+telux`, `+bitpads`, `+kernel`, `+aosp`, `+research`, `+code`, `+architecture`, `+planning`

## Reference

- ZAKO Standard: `MobileOS/ZAKO/CORE/`
- Protocols: `MobileOS/ZAKO/PROTOCOLS/`
- Distribution profile: `MobileOS/ZAKO/DISTRIBUTIONS/PROFILES/Ya-CatS22.md`
- Implementation plan: `MobileOS/ZAKO/BabbCat-Implementation-Plan.md`
- Distribution checklist: `MobileOS/ZAKO/DISTRIBUTIONS/distribution-checklist.md`
