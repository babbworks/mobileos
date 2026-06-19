# zako-simba — Package Configuration Reference

The SIMBA service integration framework. Manages admission of external services as SIMBA Nodes, enforces process class declarations, validates Service Manifests, and coordinates capability grant issuance with `telux-identd`. Every external service present on a ZAKO device is governed by SIMBA.

Normative specification: `ZAKO/PROTOCOLS/SIMBA-Standard-v1.md`

This document covers the build integration and configuration API — what distributions set in their device tree to configure the package.

---

## Build Integration

```makefile
# In device.mk:
PRODUCT_PACKAGES += ZakoSIMBA

# Copy distribution service registry:
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/zako/simba/simba.conf:system/etc/zako/simba.conf \
    $(LOCAL_PATH)/zako/simba/registry/:system/etc/zako/simba/registry/
```

SIMBA depends on `ZakoTelux` — `telux-identd` must be running for SIMBA to process capability grants. Manifest validation requires `telux-ledgerd` for ATTEST record lookup.

---

## simba.conf

Runtime configuration file. Read at daemon startup; reload via `simba-ctl reload`.

### [admission] section

Service admission policy.

```ini
[admission]
# Registry path: directory of admitted service manifests (signed .manifest files)
registry_path         = /system/etc/zako/simba/registry/

# Distribution attestation DID: the distribution's root DID used to sign ATTEST records
# Set to the distribution's actual root DID during build key generation
distribution_did      = did:key:[DISTRIBUTION_ROOT_PUBLIC_KEY]

# Require distribution ATTEST for all services (cannot be disabled in production builds)
require_attestation   = true

# Allow unsigned manifests in eng builds only
allow_unsigned_eng    = true

# Staged install: new service manifests go to staging area before ATTEST
staged_registry_path  = /data/zako/simba/staging/
```

### [process_classes] section

Permitted process class declarations in service manifests.

```ini
[process_classes]
# Which process classes external services may declare
# CRITICAL is never permitted for external services; any manifest claiming CRITICAL is rejected
permitted_classes     = interactive,background,deferred,opportunistic

# Suspension window: maximum seconds before Outstack escalates an unresponsive node
# BACKGROUND class window
background_suspend_window = 2

# INTERACTIVE class window
interactive_suspend_window = 5

# On unresponsive SIMBA Node: revoke capability grants and notify Sovereign
unresponsive_action   = revoke_and_notify
```

### [capabilities] section

Capability grant policy.

```ini
[capabilities]
# All capability grants require explicit Sovereign approval (cannot be pre-granted)
sovereign_approval_required = true

# Grant presentation: show capability requests grouped by scope at install time
# (not one-at-a-time prompts which cause approval fatigue)
grouped_presentation  = true

# Capability grant format: GRANT records written to Identity sub-entity
# These are normative Telux records — no alternative format
grant_record_type     = 0x08

# Revocation is immediate (normative; cannot be changed)
revocation_immediate  = true
```

### [records] section

Record model enforcement.

```ini
[records]
# Enforce declared record model: reject records with undeclared task_codes
# This is enforced by telux-ledgerd — this setting mirrors the policy for the SIMBA daemon
enforce_declared_model = true

# RESTRICT_FORWARD enforcement: SIMBA Nodes may never bypass this
# SELinux enforces Journal sub-entity access independently
restrict_forward_enforce = true

# On undeclared record submission: reject and emit violation signal
undeclared_record_action = reject_and_signal
```

### [ui] section

UI compliance enforcement.

```ini
[ui]
# Require ZAKO Design System compliance declaration in manifest
require_ui_compliance = true

# Brand mark exception: distribution must explicitly list approved service slugs here
# An approved slug may show a small brand mark in a defined context
brand_mark_exceptions =   # empty by default; fill with comma-separated service slugs as approved
```

---

## Service Registry

The registry at `system/etc/zako/simba/registry/` contains one signed `.manifest` file per admitted service. The filename convention is `[service-slug].manifest`.

### Manifest File Format

```yaml
# [service-slug].manifest
# Signed by the distribution's root DID before inclusion in the system image.

service_did:      did:key:[SERVICE_PROVIDER_PUBLIC_KEY]
service_name:     Service Display Name
provider_name:    Legal provider name
manifest_version: 1.0.0
simba_version:    1.0

process_class:    background
process_class_rationale: Periodic sync of content; no foreground user interaction required
suspension_safe:  true
max_deferral_hours: 24

capabilities_requested:
  - scope: read_island
    island: exchange
    required: false
    rationale: Display exchange history in service UI
  - scope: write_exchange
    required: false
    rationale: Submit exchange records when service mediates transactions

record_types_produced:
  - task_code: 0x19     # RECEIVE
    domain: 00          # Financial
    description: Inbound payment records from service-mediated transactions
  - task_code: 0x1A     # ACK
    domain: 10          # Hybrid
    description: Acknowledgement records for exchange completions

outbound_connections:
  - endpoint: api.service.example
    purpose: content delivery
    data_sent: content requests (no Sovereign PII)
    data_received: content bytes
    records_produced: [RECEIVE]
    offline_capable: true

distribution_attest_did:   did:key:[DISTRIBUTION_ROOT_PUBLIC_KEY]
distribution_attest_hash:  [BLAKE3_HASH_OF_ATTEST_RECORD]
```

---

## init.simba.rc

Provided by the package.

```
service simba-governed /system/bin/simba-governed
    class core
    user system
    group system
    writepid /dev/cpuset/foreground/tasks
    onrestart restart simba-governed
```

---

## Runtime Control

```bash
# List all admitted SIMBA Nodes and their current status:
simba-ctl nodes

# Show manifest for a specific service:
simba-ctl manifest <service-slug>

# Show active capability grants for a service:
simba-ctl grants <service-slug>

# Show process class status for all nodes:
simba-ctl classes

# Show any compliance violations detected:
simba-ctl violations

# Stage a new manifest for review:
simba-ctl stage <path-to-manifest>

# Admit a staged manifest (after distribution attestation):
simba-ctl admit <service-slug>

# Revoke a service's admission:
simba-ctl revoke <service-slug>

# Reload config:
simba-ctl reload
```

---

## Distribution Override Notes

The minimum required configuration for a distribution is:

1. `[admission].distribution_did` — set to the distribution's root DID (derived from the distribution's signing key at build time)
2. `[ui].brand_mark_exceptions` — list any services that have been explicitly approved for a brand mark; leave empty for most distributions
3. Populate the service registry with all pre-admitted service manifests

A distribution shipping no external services (a minimal, ZAKO-native-only build) may ship an empty registry. `simba-governed` will still run but manage zero nodes until services are installed by the Sovereign.

Do not modify the package source to change behavior. If a compliance requirement cannot be expressed in the config or manifest schema, open an issue against `zako-simba`.
