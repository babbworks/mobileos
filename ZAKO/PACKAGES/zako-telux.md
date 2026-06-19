# zako-telux — Package Configuration Reference

The Telux exchange stack. Manages the sovereign identity layer (DIDs, Islands, capability grants), the exchange ledger daemon, the sharing daemon, and all carrier-layer transport. The daemon triad — `telux-ledgerd`, `telux-identd`, `telux-sharedb` — provides the core exchange infrastructure for every ZAKO distribution.

Normative specification: `ZAKO/PROTOCOLS/Telux-Protocol-v1.md`

This document covers the build integration and configuration API — what distributions set in their device tree to configure the package.

---

## Build Integration

```makefile
# In device.mk:
PRODUCT_PACKAGES += ZakoTelux

# Copy distribution config files into system image:
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/zako/telux/telux.conf:system/etc/zako/telux.conf \
    $(LOCAL_PATH)/zako/telux/carriers/airtel-zm.conf:system/etc/zako/carriers/airtel-zm.conf \
    $(LOCAL_PATH)/zako/telux/carriers/mtn-zm.conf:system/etc/zako/carriers/mtn-zm.conf \
    $(LOCAL_PATH)/zako/telux/carriers/zamtel.conf:system/etc/zako/carriers/zamtel.conf
```

The package provides its own SELinux policy (`telux.te`). Journal records (sub_entity=3) are protected by SELinux policy independently of capability grants — no configuration can override this protection.

---

## telux.conf

Runtime configuration file. Read at daemon startup; reload via `telux-ctl reload` without restarting.

### [identity] section

DID method and key storage configuration.

```ini
[identity]
# DID method: did:key (default; self-issued; no external registry)
# did:web is planned for future distributions with domain-hosted identities
did_method            = did:key

# Key backend: hardware (Keymaster/TrustZone) or software (encrypted at rest)
# Always prefer hardware where available
key_backend           = hardware

# Keymaster service name (must match what the kernel provides)
keymaster_service     = android.hardware.keymaster@4.0-service-qti

# Key rotation: minimum days between rotations (0 = no minimum; Sovereign may rotate at will)
key_rotation_min_days = 0

# Identity lock: refuse new signing operations when screen locked
identity_lock_on_screen_lock = true
```

### [islands] section

Island model configuration.

```ini
[islands]
# Personal Island is always created at provisioning; this controls the name
personal_island_name  = Personal

# Maximum active Work Islands (sub_entity 16–31; max 16)
work_islands_max      = 16

# Island genesis epoch base: seconds since UNIX epoch to use as provisioning anchor
# Default: 0 means use actual device provisioning time
island_genesis_epoch  = 0

# Sub-entity static assignments (these are normative; do not change)
# sub_entity 0  = System
# sub_entity 1  = Identity
# sub_entity 2  = Exchange
# sub_entity 3  = Journal (SELinux-protected; inaccessible to SIMBA Nodes)
# sub_entity 4  = Health
# sub_entity 5  = Academy
# sub_entity 6  = People
# sub_entity 7  = Places
# sub_entity 8–15  = Reserved
# sub_entity 16–31 = Work Islands (PADS)
```

### [ledger] section

Ledger daemon configuration.

```ini
[ledger]
# Path to the main sovereign ledger (BitPads format)
path                  = /data/zako/ledger/sovereign.bpd

# Fsync policy: every_record (default; ensures durability on each write)
# batch_N (fsync every N records) — only for bench/test; never production
fsync_policy          = every_record

# Chain hash algorithm (do not change; normative in ZAKO Wire Conventions)
chain_hash_algo       = blake3

# Maximum pending records in signing queue (held unsigned while identity-locked)
signing_queue_max     = 256

# Reject batch if conservation invariant fails
conservation_enforce  = true

# Emit LEDGER_ACK signal on successful write (slot C0, category 0x0)
emit_ack_signal       = true

# Emit LEDGER_REJECT signal on failure (slot C0, category 0x1)
emit_reject_signal    = true
```

### [exchange] section

Exchange Engine configuration.

```ini
[exchange]
# Maximum time Exchange Engine holds a pending first leg (seconds)
# After this time, first leg is flagged pending-expired and Sovereign is notified
pending_leg_ttl       = 86400   # 24 hours default

# Exchange Engine records run at CRITICAL priority (cannot be overridden here)
# See Outstack process class configuration for CRITICAL definition

# Minimum batch size for Exchange Engine posting (always atomic — do not change)
atomic_posting        = true
```

### [carrier] section

Carrier transport layer configuration. Per-carrier detail is in individual carrier config files (see below).

```ini
[carrier]
# Carrier config directory
carrier_conf_dir      = /system/etc/zako/carriers/

# Default carrier selection policy: auto (telux-sharedb selects based on mode and availability)
# manual: Sovereign must explicitly choose carrier for each exchange
carrier_selection     = auto

# IP carrier preference in Standard and above (preferred for throughput efficiency)
ip_preferred_above    = standard

# BLE permitted in Critical and above
ble_min_mode          = critical

# QR always permitted (no radio transmit cost)
qr_always_permitted   = true

# SMS: used when IP and BLE unavailable; highest radio cost
sms_fallback          = true

# pads-v1 URL scheme prefix (compatibility with Workpads clients)
pads_v1_scheme        = 1pa
```

### [sharedb] section

Sharing daemon configuration.

```ini
[sharedb]
# Outbound queue path (SEND records with status=Pending)
outbound_queue_path   = /data/zako/ledger/sovereign.bpd   # same ledger; filtered by status

# Maximum outbound queue depth (pending transmission records)
outbound_queue_max    = 1024

# RESTRICT_FORWARD enforcement (cannot be disabled; included here for documentation)
restrict_forward_enforce = true

# pads-v1 inbound decode: accept inbound Workpads URLs and decode into BitPads frames
pads_v1_inbound       = true
```

---

## Carrier Config Files

One file per carrier in `system/etc/zako/carriers/`. Named `[carrier-slug].conf`. `telux-sharedb` loads all files in this directory at startup.

```ini
# Example: airtel-zm.conf
[carrier]
name            = Airtel Zambia
mcc_mnc         = 64504
slug            = airtel-zm

# APN for data transport
apn_name        = airtelweb
apn_type        = default,mms,supl

# SMS gateway for Telux carrier layer
sms_gateway     = direct   # use device modem directly; no external gateway

# Mobile money USSD (informational; used by PADS display; not automated)
mobile_money_ussd = *778#
mobile_money_name = Airtel Money

# VoLTE
volte_enabled   = false   # under investigation — see gaps-and-blind-spots.md

# RCS
rcs_enabled     = false

# eDRX configuration (passed to RIL at mode entry by Outstack)
# These values are set in outstack.conf [radio]; carrier file documents carrier support
edrx_supported  = true
psm_supported   = false   # Airtel Zambia does not support PSM as of 2026-06
```

---

## init.telux.rc

Provided by the package. Starts the daemon triad at boot.

```
# telux-ledgerd: ledger daemon (CRITICAL)
service telux-ledgerd /system/bin/telux-ledgerd
    class core
    user system
    group system radio keystore
    writepid /dev/cpuset/foreground/tasks
    onrestart restart telux-ledgerd

# telux-identd: identity daemon (CRITICAL)
service telux-identd /system/bin/telux-identd
    class core
    user system
    group system keystore
    writepid /dev/cpuset/foreground/tasks
    onrestart restart telux-identd

# telux-sharedb: sharing daemon (starts after ledgerd and identd are ready)
service telux-sharedb /system/bin/telux-sharedb
    class main
    user system
    group system radio net_admin
    writepid /dev/cpuset/foreground/tasks
    onrestart restart telux-sharedb
```

---

## Runtime Control

```bash
# Query daemon status:
telux-ctl status

# Show identity (Sovereign DID and key fingerprint):
telux-ctl identity

# Show all Islands and their current state:
telux-ctl islands

# Show capability grants currently active:
telux-ctl grants

# Show outbound queue depth and pending records:
telux-ctl queue

# Show carrier availability and current selection:
telux-ctl carrier

# Show ledger tail (last N records):
telux-ctl ledger tail 20

# Reload config without restart:
telux-ctl reload

# Force carrier selection (testing only):
telux-ctl carrier set sms
```

---

## Distribution Override Notes

The minimum required configuration for a distribution is:

1. `[identity]` — confirm `keymaster_service` name matches the kernel/HAL this device provides
2. `[ledger]` — storage path appropriate for this device's partition layout
3. `[carrier]` — carrier config files for all carriers in the target market
4. APN config — in `telephony/apns-conf.xml`, not in telux.conf (Telux reads APNs from the telephony stack)

Do not modify the package source to change behavior. If a protocol requirement genuinely cannot be expressed in the config, open an issue against `zako-telux`.
