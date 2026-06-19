 # Outstack Design Document

**Date:** 2025-01-26
**Status:** Initial Design
**Target:** Embedded/IoT devices with hostile deployment environments

## Overview

Outstack is an Alpine Linux derivative focused on two unified concerns: **security** and **power management**. It treats both as expressions of resource control—a component that can't be power-gated is a component that can't be isolated during a security incident.

## Core Philosophy

### The Two Pillars

Security and power management share the same principle: *control over system resources*.

### Core Tenets

1. **Default deny** - Nothing runs, nothing has power, nothing has access unless explicitly granted
2. **Hierarchical isolation** - CPU, memory, network, and power form independent containment boundaries
3. **Verifiable state** - At any moment, the system can attest exactly what's running and what's consuming power
4. **Graceful degradation** - Security incidents and power exhaustion trigger controlled shutdowns, not crashes

### Relationship to Alpine

Outstack is Alpine-derived, not Alpine-compatible.

**We use:**
- Alpine's `abuild` and `apk` tooling
- Alpine's musl/busybox foundation
- Selective packages from Alpine repos (audited, signed)

**We diverge on:**
- Kernel (custom hardened config)
- Init system (custom, security-aware)
- Core userspace (rebuilt with hardening flags + power instrumentation)

---

## Kernel Approach

**Strategy:** Hardened Config + Per-Board Defconfigs

We track Alpine's kernel package (`linux-lts` or `linux-edge`) and layer our configuration on top. No patching, no forking.

### Base Hardening Config (all boards)

```
# KSPP hardening essentials
CONFIG_STACKPROTECTOR_STRONG=y
CONFIG_FORTIFY_SOURCE=y
CONFIG_HARDENED_USERCOPY=y
CONFIG_SLAB_FREELIST_HARDENED=y
CONFIG_SHUFFLE_PAGE_ALLOCATOR=y
CONFIG_INIT_ON_ALLOC_DEFAULT_ON=y
CONFIG_INIT_ON_FREE_DEFAULT_ON=y

# Disable dangerous features
CONFIG_DEVMEM=n
CONFIG_DEVKMEM=n
CONFIG_KEXEC=n
CONFIG_HIBERNATION=n

# Power management core
CONFIG_PM=y
CONFIG_PM_DEBUG=y
CONFIG_POWERCAP=y
CONFIG_CPU_FREQ=y
CONFIG_CPU_IDLE=y
```

### Directory Structure

```
boards/
├── common/
│   └── hardening.config      # Shared security options
├── rpi4/
│   └── defconfig             # Includes common + RPi4 specifics
├── imx8/
│   └── defconfig
└── generic-x86/
    └── defconfig
```

### Build Integration

- Script merges `common/hardening.config` + `boards/<target>/defconfig`
- Uses Alpine's `abuild` to produce `linux-outstack-<board>` packages
- Kernel updates: pull Alpine's new source, rebuild with our configs, test matrix

### What We're NOT Doing

- No patching kernel source
- No forking
- No custom drivers (use device tree overlays instead)

---

## Security Layers

Defense in depth from boot to runtime.

### Layer 1: Secure Boot Chain

```
┌─────────────────────────────────────────────────────┐
│ Hardware Root of Trust (TPM/eFuse/secure element)   │
└──────────────────────┬──────────────────────────────┘
                       ▼
┌─────────────────────────────────────────────────────┐
│ Bootloader (U-Boot with verified boot / UEFI SB)    │
│ - Verifies kernel signature                         │
└──────────────────────┬──────────────────────────────┘
                       ▼
┌─────────────────────────────────────────────────────┐
│ Kernel + initramfs (dm-verity protected rootfs)     │
│ - Immutable root filesystem                         │
└──────────────────────┬──────────────────────────────┘
                       ▼
┌─────────────────────────────────────────────────────┐
│ Measured boot → Remote attestation capable          │
└─────────────────────────────────────────────────────┘
```

### Layer 2: Runtime Integrity

- **dm-verity**: Read-only root filesystem with hash tree verification
- **IMA/EVM**: Integrity Measurement Architecture for runtime file verification
- **Immutable /usr, mutable /var**: Clear separation - config/data in encrypted partition

### Layer 3: Mandatory Access Control

- **AppArmor** for process confinement (path-based, simpler policies, lighter than SELinux)
- **Landlock** for application self-sandboxing (in-kernel, no policy files, unprivileged)

This combination avoids SELinux complexity while providing meaningful containment.

### Layer 4: Network Hardening

- nftables with default-deny egress
- WireGuard for any external communication
- No listening services by default

---

## Power Management Subsystem

**Philosophy:** Power as a controlled resource. Default-deny for power, just like network and filesystem.

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Policy Layer                          │
│  outstack-powerd (userspace daemon)                     │
│  - Reads power budgets from /etc/outstack/power.conf    │
│  - Monitors via powercap/RAPL, INA sensors, SoC PMICs   │
│  - Enforces budgets, triggers actions on violation      │
└────────────────────────┬────────────────────────────────┘
                         │ sysfs / ioctl
┌────────────────────────▼────────────────────────────────┐
│                   Kernel Interfaces                      │
│  - /sys/class/powercap/* (RAPL, etc)                    │
│  - /sys/devices/.../power/control (runtime PM)          │
│  - /sys/class/devfreq/* (frequency scaling)             │
│  - Device tree power domains                            │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────┐
│                    Hardware                              │
│  PMICs, voltage regulators, clock gates, power rails    │
└─────────────────────────────────────────────────────────┘
```

### Power Domain Configuration

```yaml
# Example: /etc/outstack/power.conf
domains:
  cpu:
    budget_mw: 2000
    governor: powersave
    on_violation: throttle

  wifi:
    budget_mw: 500
    idle_timeout_ms: 5000
    on_violation: power_gate

  gpu:
    budget_mw: 0          # Disabled by default
    on_violation: deny
```

### Sleep State Integration

- `outstack-powerd` coordinates entry into system sleep
- Peripheral power gating before suspend (don't wake what isn't needed)
- Wake sources explicitly whitelisted per power profile
- Fast resume path: restore only what's needed for the wake event

### Power + Security Integration

- Power anomalies (unexpected draw) trigger security alerts
- Compromised peripheral can be power-killed, not just software-disabled
- Power state included in attestation reports

> **TODO:** Deeper design needed for sensor integration, PMIC specifics, wake source handling, and `outstack-powerd` daemon internals.

---

## Package & Build System

### Hybrid Approach

```
┌─────────────────────────────────────────────────────────┐
│                 Package Sources                          │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐ │
│  │   Alpine    │    │  Outstack   │    │  Outstack   │ │
│  │   Repos     │    │   Rebuilt   │    │   Custom    │ │
│  │  (direct)   │    │  (hardened) │    │  (new pkgs) │ │
│  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘ │
│         │                  │                  │         │
│         └────────┬─────────┴─────────┬────────┘         │
│                  ▼                   ▼                  │
│         ┌────────────────────────────────────┐         │
│         │     Outstack Package Repository     │         │
│         │         (apk format)                │         │
│         └────────────────────────────────────┘         │
└─────────────────────────────────────────────────────────┘
```

### Three Package Tiers

| Tier | Source | Examples | Policy |
|------|--------|----------|--------|
| **Passthrough** | Alpine repos directly | `curl`, `jq`, common tools | Trust Alpine's builds, auto-update |
| **Rebuilt** | Alpine APKBUILD, our flags | `musl`, `busybox`, `openssl` | Rebuild with hardening flags, audit |
| **Custom** | Our APKBUILDs | `outstack-powerd`, `outstack-init` | Full control, our maintenance |

### Hardening Flags (Rebuilt Tier)

```sh
export CFLAGS="-O2 -fstack-clash-protection -fcf-protection -fPIE"
export LDFLAGS="-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -pie"
```

### Build Infrastructure

- CI rebuilds hardened tier on Alpine package updates
- Reproducible builds (aim for bit-for-bit, document exceptions)
- Signed packages with Outstack key
- Separate repos per architecture: `outstack/v1/aarch64`, `outstack/v1/x86_64`

---

## Image Build & Deployment

### Storage Layout (A/B with Recovery)

```
┌──────────┬──────────┬──────────┬──────────┬─────────────┐
│ Boot     │ Root A   │ Root B   │ Recovery │ Data        │
│ (FAT32)  │ (ro,     │ (ro,     │ (minimal │ (encrypted, │
│          │ verity)  │ verity)  │ rescue)  │ rw)         │
├──────────┼──────────┼──────────┼──────────┼─────────────┤
│ 64MB     │ ~256MB   │ ~256MB   │ ~64MB    │ Remaining   │
└──────────┴──────────┴──────────┴──────────┴─────────────┘
```

### Update Model

1. Download update → verify signature
2. Write to inactive slot (A→B or B→A)
3. Update bootloader "next slot" pointer
4. Reboot → bootloader tries new slot
5. Success? Mark slot good. Failure? Auto-rollback to previous

### Image Build Pipeline

```
outstack-build/
├── mkimage.sh              # Master build script
├── profiles/
│   ├── minimal.yaml        # Base: kernel + init + shell
│   ├── iot-sensor.yaml     # + networking + telemetry
│   └── gateway.yaml        # + routing + containers
├── boards/
│   ├── rpi4/
│   ├── imx8/
│   └── generic-x86/
└── output/
    └── outstack-<profile>-<board>-<version>.img
```

### Profile YAML Example

```yaml
# profiles/iot-sensor.yaml
base: minimal
packages:
  passthrough:
    - curl
    - jq
  rebuilt:
    - musl
    - busybox
  custom:
    - outstack-init
    - outstack-powerd

power_profile: battery-conscious
security_profile: hardened
network:
  wireguard: true
  services: []           # No listening ports
```

### First Boot Sequence

1. Generate device-unique keys (in secure element if available)
2. Encrypt data partition with device key
3. Phone home for provisioning (optional, WireGuard tunnel)
4. Apply device-specific config from `/data/config`

---

## Summary

| Component | Approach |
|-----------|----------|
| **Philosophy** | Security + power as unified resource control |
| **Kernel** | Alpine kernel, hardened config + per-board defconfigs |
| **Security** | Secure boot → dm-verity → AppArmor/Landlock → nftables |
| **Power** | `outstack-powerd` with budgets, domains, sleep coordination |
| **Packages** | Hybrid: Alpine passthrough + rebuilt hardened + custom |
| **Images** | A/B rootfs, signed updates, profile-based builds |
| **Architectures** | x86_64, aarch64, armv7, RISC-V (as ecosystem matures) |

## Follow-up Design Work Needed

- [ ] Power management internals (sensors, PMIC integration, wake handling)
- [ ] `outstack-powerd` daemon detailed design
- [ ] `outstack-init` specification
- [ ] Board-specific device tree overlay strategy
- [ ] CI/CD pipeline for package rebuilds
- [ ] Attestation and remote provisioning protocol