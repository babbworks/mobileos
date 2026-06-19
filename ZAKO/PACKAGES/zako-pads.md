# zako-pads — Package Configuration Reference

The PADS Work Record Service. Manages Work Islands, the work record lifecycle (ASSIGN through TRAVEL), period Tallies, counterparty access, Exchange Engine integration for work+payment flows, and the pads-v1 URL compatibility codec.

Normative specification: `ZAKO/SERVICES/PADS-Service-Specification-v1.md`

This document covers the build integration and configuration API — what distributions set in their device tree to configure the package.

---

## Build Integration

```makefile
# In device.mk:
PRODUCT_PACKAGES += ZakoPADS

# Copy distribution config files into system image:
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/zako/pads/pads.conf:system/etc/zako/pads.conf
```

PADS depends on `ZakoTelux` — `telux-ledgerd` and `telux-identd` must be running before PADS processes can operate. The build system enforces this dependency automatically when both packages are included.

---

## pads.conf

Runtime configuration file. Read at daemon startup; reload via `pads-ctl reload`.

### [islands] section

Work Island management.

```ini
[islands]
# Maximum active Work Islands per device (sub_entity 16–31; normative max: 16)
max_active            = 16

# Island name max length (bytes; normative max: 64)
name_max_bytes        = 64

# Default work unit label — shown in Tasks View when no unit convention is set
default_unit_label    = units

# Archive path: archived (inactive) Work Islands are moved here
# They remain accessible in the ledger but are removed from the active working set
archive_path          = /data/zako/pads/archive/
```

### [exchange] section

Exchange Engine integration for work+payment compound flows.

```ini
[exchange]
# Pending invoice TTL (seconds): invoices awaiting counterparty payment
# After this period, invoice is flagged as overdue in the Tasks View
invoice_pending_ttl   = 2592000   # 30 days default

# Currency: base currency unit for EXPENSE and INVOICE records
# This is the denomination of the value_raw field in Financial domain records
# Use ISO 4217 code; conversion is display-only (ledger stores raw units)
base_currency         = ZMW   # Zambian Kwacha for Ya distribution

# Partial payment handling: post partial PAY as Receivable (account_pair=0110)
# and create pending balance record for remainder
partial_payment_track = true
```

### [pads_v1] section

Workpads pads-v1 URL compatibility codec.

```ini
[pads_v1]
# Enable pads-v1 inbound URL decoding (Workpads compatibility)
inbound_decode        = true

# Enable pads-v1 outbound encoding for SEND records to non-ZAKO counterparties
outbound_encode       = true

# pads-v1 scheme prefix
scheme_prefix         = 1pa

# On inbound decode failure: store raw URL in Journal sub-entity as a NOTE record
# Allows Sovereign to inspect and manually process failed decodes
store_failed_decode   = true
```

### [views] section

Service View configuration.

```ini
[views]
# Tasks View: show work items ordered by wall_ts, grouped by Work Island
tasks_view_enabled    = true

# Tallies View: show period-close summaries across all Work Islands
tallies_view_enabled  = true

# Money View: show financial records (EXPENSE, INVOICE, PAY, RECEIVE)
money_view_enabled    = true

# Notes View: show records with note component
notes_view_enabled    = true

# Exchanges View: show records with dest_did set (bilateral exchanges)
exchanges_view_enabled = true

# All views are derived at read time from telux-ledgerd — no separate mutable storage
```

### [process] section

Process class declarations for PADS operations.

```ini
[process]
# User-initiated operations (record creation, island creation, tally generation)
interactive_class     = user-active

# Periodic background operations (counterparty sync, queue processing, period analysis)
background_class      = background

# Low-priority intelligence (work pattern analysis, Learning Engine)
opportunistic_class   = deferred

# CRITICAL operations are delegated to telux-ledgerd and telux-identd
# PADS itself does not claim CRITICAL class
```

---

## init.pads.rc

Provided by the package.

```
# pads-workd: work record daemon (BACKGROUND)
service pads-workd /system/bin/pads-workd
    class main
    user system
    group system
    writepid /dev/cpuset/background/tasks
    onrestart restart pads-workd
```

The interactive PADS operations (record creation, island management) execute in-process via the PADS API exposed through `telux-ledgerd`. `pads-workd` handles only the background operations: outbound queue processing, period analysis, and Learning Engine work pattern analysis.

---

## Runtime Control

```bash
# Show all active Work Islands:
pads-ctl islands

# Show pending outbound queue (SEND records awaiting transmission):
pads-ctl queue

# Show current period status for a Work Island:
pads-ctl period <island-id>

# Show Tasks View summary:
pads-ctl tasks

# Show Tallies View:
pads-ctl tallies

# Show open invoices:
pads-ctl invoices

# Reload config:
pads-ctl reload
```

---

## Distribution Override Notes

The minimum required configuration for a distribution is:

1. `[exchange].base_currency` — set to the primary currency of the target market (ISO 4217 code)
2. `[pads_v1]` — confirm inbound/outbound encoding is appropriate for the target market's Workpads client base

Work Island sub-entity numbering (16–31) is normative and must not be changed. PADS conforms to PADS Service Specification v1 in all distributions.

Do not modify the package source to change behavior. If a Work Island use case genuinely cannot be expressed in the config, open an issue against `zako-pads`.
