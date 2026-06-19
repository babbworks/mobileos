# Health Service Specification
## Version 1.0

*June 1, 2026*

---

> Health data is the most personal class of record a device can hold. It is also among the most actionable: a blood pressure reading that crosses a threshold, a medication dose missed, a sleep deficit accumulating across a week. HOME treats health records as sovereign by default — they do not leave the device without an explicit action, they are never accessible to any service without a capability grant, and the most sensitive categories carry RESTRICT_FORWARD at the wire level. The health record is yours. What you do with it is yours.

---

## 1. Purpose and Scope

This document is a service specification within the HOME Standard. It defines the Health service: the Health Island model, measurement record types, compound patterns for multi-value readings, privacy controls, Learning Engine integration for trend analysis, and device sensor integration.

In scope:
- Health Island structure (sub_entity=4, file_sep=4)
- All measurement record types and their wire encoding
- Compound patterns for multi-value readings (blood pressure, etc.)
- Privacy controls: RESTRICT_FORWARD for sensitive categories
- Health period Tally records
- Device and wearable sensor integration path
- Learning Engine integration for health trend analysis
- Conformance requirements

Out of scope:
- Clinical decision support or medical advice logic
- External health system integrations (FHIR, HL7)
- Prescription management or pharmacy integrations

---

## 2. Health Island

### 2.1 Structure

The Health Island occupies sub_entity=4, file_sep=4 in the Personal Island. All health records are written to this sub-entity. The Health Island has no counterparty write access by default — health records are self-authored by the Sovereign (or by HOME sensor daemons acting on the Sovereign's behalf).

A counterparty (clinician, caregiver, health monitoring service) may be granted read access to the Health Island via a capability grant. Write access to the Health Island is never granted to external parties; clinical orders are expressed as inbound exchange records in the Exchange sub-entity, not as direct writes to Health Island records.

### 2.2 record_sep Assignments in Health Island

| record_sep | Category |
|------------|----------|
| 0 | Cardiovascular (blood pressure, heart rate, SpO2) |
| 1 | Metabolic (weight, glucose, BMI) |
| 2 | Activity (steps, exercise, distance) |
| 3 | Sleep and recovery |
| 4 | Nutrition (intake, hydration) |
| 5 | Medications and supplements |
| 6 | Reproductive and maternal health |
| 7 | Mental and emotional health |
| 8 | Respiratory |
| 9 | Environmental exposure (temperature, air quality — sourced from Environment domain) |
| 10 | Clinical orders (inbound; reference to Exchange sub-entity) |
| 11 | Period close (Tally records) |
| 12–15 | Reserved |

### 2.3 Privacy Defaults

All Health Island records carry `restrict_fwd=0` by default except the following categories, which carry `restrict_fwd=1` by default:

- record_sep=6 (Reproductive and maternal health)
- record_sep=7 (Mental and emotional health)
- Any record where the Sovereign explicitly marks it private at creation time

Records with `restrict_fwd=1` are stored in the Health Island like any other record. The restriction governs sharing only — they are readable by the Sovereign and by any process with a Health Island read capability grant, subject to the standard SELinux constraint on the Journal sub-entity (Journal records, sub_entity=3, are categorically prohibited from `telux-sharedb`; Health records at sub_entity=4 are not subject to the same categorical prohibition, but individual record privacy is enforced by the `restrict_fwd` flag).

---

## 3. Measurement Records

### 3.1 Standard Measurement Record

All health measurements use task_code=0x2C (MEASURE):

```
task_code     = 0x2C   MEASURE
account_pair  = 0000   (Observed — Health domain)
domain        = 11; domain_ext = 0x03  Health
file_sep      = 4      Health sub-entity
record_sep    = <category slot>
value         = measurement_value (scaled per quantity type)
quantity_type = <from Health domain codebook: 0x00–0x14>
wall_ts       = measurement_epoch
source_did    = sovereign_did  [or sensor_daemon_did for automated readings]
```

### 3.2 Blood Pressure (Compound)

Blood pressure requires two simultaneous values. This always uses the compound pattern defined in HOME Wire Conventions §7.2 (Pattern C):

```
Record 1 — Systolic (Completeness=1, Partial):
  task_code     = 0x2C   MEASURE
  account_pair  = 0000   Observed
  quantity_type = 0x00   Systolic blood pressure
  value         = systolic_mmhg
  record_sep    = 0      Cardiovascular
  wall_ts       = reading_epoch

Record 2 — Diastolic (1111, sub_type=00, Completeness=0):
  account_pair  = 1111
  sub_type      = 00
  quantity_type = 0x01   Diastolic blood pressure
  value         = diastolic_mmhg
```

The shared wall_ts from Record 1 applies to both values. The pair is stored atomically.

### 3.3 Threshold Alert

When a measurement exceeds a Sovereign-defined threshold, HOME generates an ALERT record immediately following the MEASURE record:

```
task_code     = 0x2F   ALERT
account_pair  = 0110   (Threshold — Health domain)
domain        = 11; domain_ext = 0x03
value         = threshold_value_that_was_exceeded
note          = <which threshold; direction: above/below>
```

The ALERT record references the triggering MEASURE record's frame_hash in its note component. ALERT records with Health domain are always promoted to INTERACTIVE priority by `outstack-powerd`.

### 3.4 Medication Records

Medication events use task_code=0x2C MEASURE with quantity_type=0x0B (Medication dose):

**Administered dose:**
```
task_code     = 0x2C   MEASURE
account_pair  = 0011   (Administered — Health domain)
quantity_type = 0x0B   Medication dose (mg)
value         = dose_mg × 10
note          = <medication name; route; form>
```

**Prescribed dose (clinician order reference):**
```
task_code     = 0x2C   MEASURE
account_pair  = 0010   (Prescribed — Health domain)
quantity_type = 0x0B
value         = prescribed_dose_mg × 10
note          = <medication name; prescribing entity DID; order reference>
```

### 3.5 Self-Reported Records

Readings entered manually by the Sovereign without a device sensor use account_pair=0100 (Self-reported — Health domain):

```
account_pair  = 0100   Self-reported
```

Device-sensor readings use account_pair=0000 (Observed). Learning Engine inferred or estimated values use account_pair=1010 (Estimated). This distinction allows the Sovereign and any counterparty to distinguish observed measurements from estimates at a glance.

### 3.6 Reproductive and Maternal Health

Records in record_sep=6 use the dedicated reproductive health quantity types (0x10–0x14 in the Health domain codebook):

| Quantity Type | Measurement |
|--------------|-------------|
| 0x10 | Menstrual cycle day |
| 0x11 | Dilation / cervical (mm) |
| 0x12 | Fetal heart rate (BPM) |
| 0x13 | Contraction duration (seconds) |
| 0x14 | Contraction interval (minutes) |

All records in record_sep=6 carry `restrict_fwd=1` by default. Contraction tracking (0x13 + 0x14) uses a compound group to capture both values simultaneously:

```
Record 1 — Contraction duration (Completeness=1):
  quantity_type = 0x13
  value         = duration_seconds

Record 2 — Interval (1111, sub_type=00, Completeness=0):
  quantity_type = 0x14
  value         = interval_minutes
```

---

## 4. Health Period Tallies

A health period Tally summarises readings over a defined period. Health Tallies use a multi-leg compound record to capture summaries across categories:

```
Record 1 — Cardiovascular summary (group_sep=63, Completeness=1):
  task_code     = 0x15   COMMIT
  account_pair  = 1101   State Commit
  domain        = 11; domain_ext = 0x03
  record_sep    = 11     Period close
  group_sep     = 63     Tally
  value         = avg_heart_rate_bpm

Record 2 — Activity summary (1111, sub_type=00, Completeness=1):
  task_code     = 0x15   COMMIT
  quantity_type = 0x08   Step count
  value         = total_steps_in_period

Record 3 — Sleep summary (1111, sub_type=00, Completeness=1):
  quantity_type = 0x09   Sleep duration
  value         = avg_sleep_minutes_per_night

Record 4 — Period anchor (1111, sub_type=00, Completeness=0):
  task_code     = 0x15   COMMIT
  value         = period_duration_days
  note          = <period label: "Week ending 2026-06-07">
```

The Tally record is the last record in its batch (group_sep=63 with Completeness=1 on the first leg; the compound group closes at Record 4 with Completeness=0).

---

## 5. Device and Sensor Integration

### 5.1 Sensor Daemon

HOME sensor integration is handled by a dedicated sensor daemon (`home-sensord`) that reads from connected hardware (wearables, Bluetooth sensors, on-device sensors) and submits measurement records to `telux-ledgerd`. The sensor daemon:

- Operates at BACKGROUND process class for routine sampling
- Escalates to INTERACTIVE process class when a threshold alert is triggered
- Is suspended in Conservation mode and gated in Critical Reserve
- Produces CALIBRATE records (task_code=0x04) when a sensor undergoes a calibration event

The sensor daemon signs records with its own registered DID (a sub-entity DID derived from the device Sovereign's primary key). Records produced by the sensor daemon have `source_did = sensor_daemon_did` and carry `account_pair=0000` (Observed).

### 5.2 Wearable Integration

Wearables connected via Bluetooth LE deliver readings to the sensor daemon using the Bluetooth LE framing defined in HOME Transmission Adapters (forthcoming). The wearable's readings are decoded from BLE advertisements or GATT characteristics, wrapped in BitPads frames, and submitted to `telux-ledgerd` by the sensor daemon.

### 5.3 Environment Sensor Crossover

Environment domain sensors (ambient temperature, air quality, UV index) that are relevant to health context may produce records in the Health Island (record_sep=9) in addition to their primary Environment domain ledger entries. These crossover records use:

```
domain        = 11; domain_ext = 0x0A  Environment
record_sep    = 9      Environmental exposure (Health Island)
task_code     = 0x28   OBSERVE
```

---

## 6. Learning Engine Integration

### 6.1 Health Trend Analysis

The Learning Engine analyses Health Island records to detect trends, flag anomalies, and provide the Sovereign with longitudinal context. This analysis runs at OPPORTUNISTIC process class in Full Power mode only.

Analysis records produced by the Learning Engine are stored in the Health Island using task_code=0x2D ESTIMATE or 0x2E COMPARE:

**Trend record:**
```
task_code     = 0x2E   COMPARE
account_pair  = 1000   (Trend — Health domain)
value         = trend_direction_and_magnitude  (signed integer; positive = increasing)
note          = <human-readable trend description>
```

**Anomaly flag:**
```
task_code     = 0x2F   ALERT
account_pair  = 1000   (Trend — Health domain)
note          = <anomaly description and confidence>
```

Learning Engine health records carry `source_did = learning_engine_did` (a registered sub-entity DID) and are distinguished from direct measurements by account_pair (Estimated or Trend rather than Observed or Self-reported).

---

## 7. Conformance Requirements

1. **Health Island is access-controlled.** No process reads Health Island records without a capability grant from the Sovereign. The grant is scoped to read only; external write access is not available.

2. **Reproductive and mental health records default to RESTRICT_FORWARD=1.** Any implementation that permits these records to leave the device without an explicit Sovereign override is non-conforming.

3. **Blood pressure always uses compound pattern.** Single-record blood pressure storage (one record for systolic only) is a protocol error in a conforming implementation.

4. **Sensor daemon records are distinguishable.** Sensor-daemon-generated records carry account_pair=0000 (Observed) and a registered sensor daemon DID. Manually-entered records carry account_pair=0100 (Self-reported).

5. **ALERT records are INTERACTIVE priority.** A Health ALERT record triggers an INTERACTIVE priority signal regardless of current Outstack mode. It is never deferred by power gating.

6. **Tally values are verified.** Health period Tally records are verified against the period's measurement records before storage. A Tally that cannot be verified (because its value does not match the computed aggregate) is stored with an `unverified_tally` flag rather than rejected — health aggregates involve averaging which may not be exactly reconstructable.

---

*Health Service Specification v1.0 — June 1, 2026*  
*Sub-protocol of HOME Standard v1.x*  
*Cross-reference: HOME Codebook Standard v1.0 §6.1 and §7.1, HOME Wire Conventions v1.0 §7.2 Pattern C*
