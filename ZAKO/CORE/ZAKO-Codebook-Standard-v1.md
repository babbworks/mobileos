# ZAKO Codebook Standard
## Version 1.0

*June 1, 2026*

---

> This document is a sub-protocol of the ZAKO Standard. The ZAKO Standard references this document by version number. Changes to codebook entries, quantity types, or account pair reinterpretations are made here without requiring a ZAKO Standard revision. All ZAKO distributions must implement the codebooks defined in the version of this document referenced by their ZAKO Standard version.

---

## 1. Purpose and Scope

This document defines the complete codebook system for ZAKO records. A codebook is a shared lookup table that gives structured semantic meaning to compact numeric values in the BitPads wire format. ZAKO uses codebooks at four positions in every record:

- **Task Code** (6-bit field in the Full Record task component): the action verb
- **Domain** (2-bit Layer 1 field + optional custom extension byte): the semantic context
- **Quantity Type** (6-bit Layer 2 field): the unit of measurement per domain
- **Account Pair** (4-bit Layer 3 field): the relationship archetype per domain

Every ZAKO record is the intersection of values drawn from these four codebooks. The combination expresses any record type ZAKO needs without requiring schema lookups or variable-length type identifiers.

This document defines the normative values for each codebook. Distribution-defined extension ranges are described in §8.

---

## 2. Versioning and Ratification

This document carries a version number independent of the ZAKO Standard version. The ZAKO Standard references it as:

```
ZAKO Codebook Standard v<major>.<minor>
```

**Minor version increments** (e.g., v1.0 → v1.1): additions only. New entries are appended to existing codebooks within defined extension ranges, or new quantity types added to existing custom domains. No existing entry changes meaning. All v1.x implementations are backward compatible.

**Major version increments** (e.g., v1.x → v2.0): may include redefinitions. A major version change requires a corresponding ZAKO Standard update to reference the new major version. Major versions are rare.

**Reserved entries** are placeholders whose values are defined as "reserved for future use." Implementations must not assign meaning to reserved entries and must treat records carrying reserved values as unknown-type records (store and preserve; do not process semantically).

---

## 3. Task Code Verb Codebook

The task_code field is 6 bits, providing 64 values (0x00–0x3F). Every value is an action verb. The domain field provides the noun context. A ZAKO implementation must recognise all 64 verbs and correctly route records to the appropriate Service View based on the verb.

### 3.1 System Verbs (0x00–0x07)

Records with system verbs originate from ZAKO system daemons (outstack-powerd, telux-identd, telux-ledgerd). They are written to the Personal Island system sub-entity (sub_entity=0).

| Code | Verb | Meaning |
|------|------|---------|
| 0x00 | POWER_CHANGE | Power level, source, or draw change event |
| 0x01 | MODE_CHANGE | Outstack system mode transition |
| 0x02 | GATE | Hardware or process gating applied |
| 0x03 | RESTORE | Gate lifted; resource or process restored |
| 0x04 | CALIBRATE | Sensor, measurement, or system calibration event |
| 0x05 | ASSIGN | Resource or process class assignment |
| 0x06 | SUSPEND | Process or service suspended by Outstack |
| 0x07 | RESUME | Process or service resumed by Outstack |

### 3.2 Identity Verbs (0x08–0x0F)

Records with identity verbs relate to sovereignty operations: DID management, capability grants, attestation. They are written to the Personal Island identity sub-entity (sub_entity=1).

| Code | Verb | Meaning |
|------|------|---------|
| 0x08 | GRANT | Capability granted to an entity |
| 0x09 | REVOKE | Capability revoked from an entity |
| 0x0A | DELEGATE | Capability delegated to a sub-entity |
| 0x0B | EXPIRE | Time-based expiry of a grant or agreement |
| 0x0C | CREATE | Entity, Island, or identity created |
| 0x0D | BIND | DID or key bound to an entity or device |
| 0x0E | ATTEST | Signed assertion made by a Sovereign |
| 0x0F | VERIFY | Assertion checked and confirmed |

### 3.3 Structural Verbs (0x10–0x17)

Records with structural verbs relate to Island and chain lifecycle: opening and closing contexts, membership events, state commits, corrections.

| Code | Verb | Meaning |
|------|------|---------|
| 0x10 | OPEN | Session, chain, or Island context opened |
| 0x11 | CLOSE | Session, chain, or Island context closed |
| 0x12 | JOIN | Entity joined an Island or newgroup |
| 0x13 | LEAVE | Entity voluntarily left an Island or newgroup |
| 0x14 | REMOVE | Entity removed from Island by Sovereign |
| 0x15 | COMMIT | State committed; period or chain locked (Tally) |
| 0x16 | AMEND | Prior record amended by a new superseding record |
| 0x17 | DISPUTE | Record or chain formally disputed |

### 3.4 Exchange Verbs (0x18–0x1F)

Records with exchange verbs involve a counterparty. They are Exchange Engine records. A destination_did must be present in the participants block for all exchange verbs except ACK (which references a prior record's frame_hash).

| Code | Verb | Meaning |
|------|------|---------|
| 0x18 | SEND | Record or value transmitted to counterparty |
| 0x19 | RECEIVE | Record or value received from counterparty |
| 0x1A | ACK | Receipt acknowledged by receiving party |
| 0x1B | INVOICE | Payment requested from counterparty |
| 0x1C | PAY | Payment made to counterparty |
| 0x1D | QUOTE | Estimate or quote issued to counterparty |
| 0x1E | AGREE | Both parties committed to stated terms |
| 0x1F | COMPLETE | Exchange fully resolved; all obligations met |

### 3.5 Work Verbs (0x20–0x27)

Records with work verbs are PADS Work Record Service records. They constitute the work exchange lifecycle from assignment through completion.

| Code | Verb | Meaning |
|------|------|---------|
| 0x20 | ASSIGN | Work assigned (to self or counterparty) |
| 0x21 | START | Work begun |
| 0x22 | PROGRESS | Progress update on active work item |
| 0x23 | FINISH | Work item completed |
| 0x24 | INSPECT | Inspection performed at site or on output |
| 0x25 | REPORT | Report formally submitted |
| 0x26 | EXPENSE | Expense incurred and recorded |
| 0x27 | TRAVEL | Travel event associated with work |

### 3.6 Observe Verbs (0x28–0x2F)

Records with observe verbs capture events, observations, annotations, and personal entries. The distinction between NOTE and JOURNAL is enforced at the SELinux level, not only at the application level.

| Code | Verb | Meaning |
|------|------|---------|
| 0x28 | OBSERVE | Measurement, sensing, or witnessing event |
| 0x29 | NOTE | Shareable annotation; may be linked to another record |
| 0x2A | JOURNAL | Private personal entry; RESTRICT_FORWARD is always set |
| 0x2B | CAPTURE | Personal accumulation for Learning Engine |
| 0x2C | MEASURE | Quantified observation with explicit unit |
| 0x2D | ESTIMATE | Calculated or inferred value |
| 0x2E | COMPARE | Delta or contrast record between two prior records |
| 0x2F | ALERT | Threshold breach or attention event |

**JOURNAL invariant:** Any record with task_code=JOURNAL (0x2A) must carry RESTRICT_FORWARD=1 in Meta Byte 2. A ZAKO implementation that receives or generates a JOURNAL record without this flag set must treat it as a malformed record and refuse to process or store it. The flag is not optional for JOURNAL records.

### 3.7 Schedule Verbs (0x30–0x37)

Records with schedule verbs relate to the Schedule domain (custom domain 0x08). They govern time-based event management.

| Code | Verb | Meaning |
|------|------|---------|
| 0x30 | SCHEDULE | Event created and time assigned |
| 0x31 | RESCHEDULE | Event time or location changed |
| 0x32 | CANCEL | Event cancelled |
| 0x33 | REMIND | Reminder issued to participant(s) |
| 0x34 | TRIGGER | Scheduled event fired; action initiated |
| 0x35 | RECUR | Recurring instance of a repeating event |
| 0x36 | ATTEND | Attendance at event confirmed or recorded |
| 0x37 | DELEGATE | Event responsibility transferred to another entity |

### 3.8 Learning Verbs (0x38–0x3F)

Records with learning verbs are Learning Engine records. They cover AI interactions, automation events, and knowledge accumulation.

| Code | Verb | Meaning |
|------|------|---------|
| 0x38 | QUERY | Question or information request made |
| 0x39 | RESPOND | Response to a query provided |
| 0x3A | ACT | Autonomous action taken by agent or automation |
| 0x3B | DEFER | Action deferred pending human input |
| 0x3C | LEARN | Learning event recorded in Academy |
| 0x3D | TEACH | Teaching or knowledge-sharing event |
| 0x3E | RECOMMEND | Recommendation issued by system or peer |
| 0x3F | AUTOMATE | Automation rule created, modified, or applied |

---

## 4. Domain Declarations

The domain field is 2 bits in the Layer 1 session header. Custom domains (domain=11) carry an additional extension byte immediately following the Layer 1 header.

```
domain = 00   Financial     Monetary flows; BitLedger financial account pairs
domain = 01   Engineering   Physical flows; BitLedger Universal Domain archetypes
domain = 10   Hybrid        Simultaneous financial and engineering interpretation
domain = 11   Custom        Extension byte declares specialist domain
```

### 4.1 Custom Domain Extension Byte Values

| Value | Domain Name | Primary Services |
|-------|-------------|-----------------|
| 0x03 | Health / Biometric | Health Island |
| 0x04 | Academy / Education | Academy Island |
| 0x05 | Location / Place | Places Island |
| 0x06 | Social / People | People Island |
| 0x07 | Legal / Agreement | Agreements Island |
| 0x08 | Schedule / Calendar | Schedule View |
| 0x09 | Media / Content Reference | Academy, Notes |
| 0x0A | Environment / Sensor | System, Health |
| 0x0B–0xEF | Reserved | — |
| 0xF0–0xFF | Distribution-defined | See §8.2 |

---

## 5. Standard Domain Account Pair Codebooks

### 5.1 Financial Domain Account Pairs (domain=00)

The 4-bit account pair field in financial domain encodes the double-entry accounting relationship between the two sides of a transaction.

| Code | Pair Name | Debit Side | Credit Side |
|------|-----------|-----------|-------------|
| 0000 | Op Income / Asset | Asset increases | Operating income recognised |
| 0001 | Asset / Liability | Asset increases | Liability incurred |
| 0010 | Liability / Asset | Liability reduced | Asset reduced |
| 0011 | Op Expense / Asset | Operating expense recognised | Asset decreases |
| 0100 | Asset / Equity | Asset increases | Equity increases |
| 0101 | Equity / Asset | Equity decreases | Asset decreases |
| 0110 | Receivable / Income | Receivable created | Income recognised |
| 0111 | Asset / Receivable | Asset increases (cash in) | Receivable cleared |
| 1000 | Expense / Payable | Expense recognised | Payable created |
| 1001 | Payable / Asset | Payable cleared | Asset decreases (cash out) |
| 1010 | Reserve / Equity | Reserve created from equity | Equity decreases |
| 1011 | Asset / Reserve | Asset funded from reserve | Reserve released |
| 1100 | Tax / Payable | Tax expense recognised | Tax payable created |
| 1101 | State Commit | Period snapshot; no flow; Tally lock |
| 1110 | Correction / Void | Inference suspended; record voided |
| 1111 | Compound Continuation | Requires compound mode ON at session |

### 5.2 Engineering Domain Account Pairs (domain=01)

In engineering domain, the account pair field encodes the relationship archetype between two nodes in any conserved system. These archetypes are drawn from the BitLedger Universal Domain v1.0 specification.

| Code | Archetype | Flow Meaning |
|------|-----------|-------------|
| 0000 | Source → Sink | Direct one-way transfer between nodes |
| 0001 | Parent → Child | Hierarchical allocation from superior to subordinate |
| 0010 | Debtor → Creditor | Obligation incurred; debt created |
| 0011 | Mutual Exchange | Balanced bilateral transfer; both sides move |
| 0100 | Loss / Dissipation | Quantity leaves system to environment |
| 0101 | Generation / Input | Quantity enters system from external source |
| 0110 | Reservation / Escrow | Committed but not yet transferred |
| 0111 | Repayment / Return | Fulfilling a prior obligation or reservation |
| 1000 | Transformation | Quantity changes form within system |
| 1001 | Distribution | One source to multiple sinks |
| 1010 | Aggregation | Multiple sources to one sink |
| 1011 | Internal Transfer | Movement within one entity |
| 1100 | Obligation Transfer | Debt or obligation reassigned |
| 1101 | State Commit | Snapshot of balance; no flow; period marker |
| 1110 | Correction / Void | Telemetry correction; sensor recalibration |
| 1111 | Compound Continuation | Requires compound mode ON at session |

---

## 6. Custom Domain Quantity Type Codebooks

In custom domains, the 6-bit quantity type field (which carries currency codes in financial domain) carries the unit of measurement specific to that domain. These codebooks define what the `value` field means.

### 6.1 Health Domain (0x03)

| Code | Quantity | Unit | Encoding |
|------|----------|------|----------|
| 0x00 | Systolic blood pressure | mmHg | Integer |
| 0x01 | Diastolic blood pressure | mmHg | Integer |
| 0x02 | Heart rate | BPM | Integer |
| 0x03 | Body weight | grams | Integer |
| 0x04 | Body height | millimetres | Integer |
| 0x05 | Blood glucose | mg/dL | value × 10 |
| 0x06 | Blood oxygen (SpO2) | percent | value × 10 |
| 0x07 | Body temperature | °C | value × 100 |
| 0x08 | Step count | steps | Integer |
| 0x09 | Sleep duration | minutes | Integer |
| 0x0A | Caloric intake / expenditure | kcal | Integer |
| 0x0B | Medication dose | mg | value × 10 |
| 0x0C | Pain level | 0–100 scale | Integer |
| 0x0D | Mood score | 0–100 scale | Integer |
| 0x0E | Respiratory rate | breaths/min | Integer |
| 0x0F | Hydration | millilitres | Integer |
| 0x10 | Menstrual cycle day | day of cycle | Integer |
| 0x11 | Dilation / cervical | millimetres | Integer |
| 0x12 | Fetal heart rate | BPM | Integer |
| 0x13 | Contraction duration | seconds | Integer |
| 0x14 | Contraction interval | minutes | Integer |
| 0x15–0x2F | Reserved | — | — |
| 0x30–0x3F | Distribution-defined health quantities | — | See §8.3 |

### 6.2 Academy Domain (0x04)

| Code | Quantity | Unit | Encoding |
|------|----------|------|----------|
| 0x00 | Lesson completion | percent | value × 10 |
| 0x01 | Assessment score | percent | value × 10 |
| 0x02 | Time engaged | minutes | Integer |
| 0x03 | Points or XP earned | points | Integer |
| 0x04 | Streak | consecutive days | Integer |
| 0x05 | Mastery level | 0–50 scale | Integer |
| 0x06 | Resources accessed | count | Integer |
| 0x07 | Certificate or milestone | binary (0 or 100) | Integer |
| 0x08 | Revision count | count | Integer |
| 0x09 | Collaborative sessions | count | Integer |
| 0x0A | Content items completed | count | Integer |
| 0x0B | Instructor rating given | 0–50 scale | Integer |
| 0x0C–0x2F | Reserved | — | — |
| 0x30–0x3F | Distribution-defined academy quantities | — | See §8.3 |

### 6.3 Location Domain (0x05)

| Code | Quantity | Unit | Encoding |
|------|----------|------|----------|
| 0x00 | Latitude | degrees | value × 1,000,000; direction bit = N(0)/S(1) |
| 0x01 | Longitude | degrees | value × 1,000,000; direction bit = E(0)/W(1) |
| 0x02 | Altitude | metres | value × 10; direction bit = above(0)/below(1) sea level |
| 0x03 | Position accuracy | metres | value × 10 |
| 0x04 | Bearing | degrees | value × 10 (0–3600) |
| 0x05 | Speed | km/h | value × 10 |
| 0x06 | Area | square metres | Integer |
| 0x07 | Distance from reference | metres | Integer |
| 0x08 | Duration at place | minutes | Integer |
| 0x09 | Visit count | count | Integer |
| 0x0A | Elevation gain | metres | value × 10 |
| 0x0B–0x2F | Reserved | — | — |
| 0x30–0x3F | Distribution-defined location quantities | — | See §8.3 |

**Latitude/Longitude encoding note:** A Place definition is always a compound record. Latitude (0x00) is Record A; longitude (0x01) is the first 1111 continuation. The direction bit in the Layer 3 value block carries N/S for latitude and E/W for longitude. Implementations must not use a separate sign field; the direction bit is the sign.

### 6.4 Social Domain (0x06)

| Code | Quantity | Unit | Encoding |
|------|----------|------|----------|
| 0x00 | Trust level | 0–100 scale | Integer |
| 0x01 | Interaction count | count | Integer |
| 0x02 | Relationship age | days | Integer |
| 0x03 | Shared Islands | count | Integer |
| 0x04 | Outstanding mutual obligations | count | Integer |
| 0x05 | Introductions made | count | Integer |
| 0x06 | Shared records (exchanged) | count | Integer |
| 0x07–0x2F | Reserved | — | — |
| 0x30–0x3F | Distribution-defined social quantities | — | See §8.3 |

### 6.5 Agreement Domain (0x07)

| Code | Quantity | Unit | Encoding |
|------|----------|------|----------|
| 0x00 | Term count | count | Integer |
| 0x01 | Disputed value | currency units | DECIMAL_POS from Layer 2 |
| 0x02 | Agreement duration | days | Integer |
| 0x03 | Obligation count | count | Integer |
| 0x04 | Party count | count | Integer |
| 0x05 | Amendment revision number | integer | Integer |
| 0x06 | Penalty value | currency units | DECIMAL_POS from Layer 2 |
| 0x07 | Notice period | days | Integer |
| 0x08–0x2F | Reserved | — | — |
| 0x30–0x3F | Distribution-defined agreement quantities | — | See §8.3 |

### 6.6 Schedule Domain (0x08)

| Code | Quantity | Unit | Encoding |
|------|----------|------|----------|
| 0x00 | Event duration | minutes | Integer |
| 0x01 | Lead time before event | minutes | Integer |
| 0x02 | Recurrence interval | days | Integer |
| 0x03 | Participant count | count | Integer |
| 0x04 | Priority level | 0–100 scale | Integer |
| 0x05 | Alarm count | count | Integer |
| 0x06 | Buffer time after event | minutes | Integer |
| 0x07–0x2F | Reserved | — | — |
| 0x30–0x3F | Distribution-defined schedule quantities | — | See §8.3 |

### 6.7 Media Domain (0x09)

| Code | Quantity | Unit | Encoding |
|------|----------|------|----------|
| 0x00 | File size | kilobytes | Integer |
| 0x01 | Duration | seconds | Integer |
| 0x02 | Resolution width | pixels | Integer |
| 0x03 | Resolution height | pixels | Integer |
| 0x04 | Bitrate | kbps | Integer |
| 0x05 | Page count | count | Integer |
| 0x06 | Version number | integer | Integer |
| 0x07–0x2F | Reserved | — | — |
| 0x30–0x3F | Distribution-defined media quantities | — | See §8.3 |

### 6.8 Environment Domain (0x0A)

| Code | Quantity | Unit | Encoding |
|------|----------|------|----------|
| 0x00 | Ambient temperature | °C | value × 100; direction=above(0)/below(1) 0°C |
| 0x01 | Relative humidity | percent | value × 10 |
| 0x02 | Atmospheric pressure | millibars | value × 10 |
| 0x03 | Light level | lux | Integer |
| 0x04 | UV index | 0–110 scale | Integer |
| 0x05 | Air quality index | AQI | Integer |
| 0x06 | CO2 concentration | ppm | Integer |
| 0x07 | Noise level | dB SPL | value × 10 |
| 0x08 | Wind speed | km/h | value × 10 |
| 0x09 | Rainfall | mm/hour | value × 10 |
| 0x0A | Soil moisture | percent | Integer |
| 0x0B–0x2F | Reserved | — | — |
| 0x30–0x3F | Distribution-defined environment quantities | — | See §8.3 |

---

## 7. Custom Domain Account Pair Codebooks

In custom domains the account_pair field carries domain-specific relationship archetypes. In all custom domains, codes 1101 (State Commit / Tally), 1110 (Correction/Void), and 1111 (Compound Continuation, when compound mode is active) retain their universal meanings.

### 7.1 Health Domain Account Pairs

| Code | Name | Meaning |
|------|------|---------|
| 0000 | Observed | Measurement recorded from device or manual entry |
| 0001 | Target | Goal or reference value set by user or clinician |
| 0010 | Prescribed | Clinician order; treatment or medication directed |
| 0011 | Administered | Medication given or treatment applied |
| 0100 | Self-reported | User-entered value without device measurement |
| 0101 | Verified | Clinician-confirmed reading |
| 0110 | Threshold | Value exceeded a defined upper or lower limit |
| 0111 | Normal | Value confirmed within normal range |
| 1000 | Trend | Direction of change expressed over a period |
| 1001 | Aggregate | Summary or average over a defined period |
| 1010 | Estimated | Calculated or algorithmically inferred value |
| 1011 | Shared | Record shared with another named entity |
| 1100 | Linked | Record associated with another record by frame_hash |
| 1101 | State Commit | Health period snapshot; Tally locked |
| 1110 | Correction / Void | Prior reading corrected or voided |
| 1111 | Compound Continuation | — |

### 7.2 Academy Domain Account Pairs

| Code | Name | Meaning |
|------|------|---------|
| 0000 | Consumed | Content viewed, opened, or started |
| 0001 | Completed | Lesson, module, or course finished |
| 0010 | Assessed | Test, quiz, or evaluation taken |
| 0011 | Mastered | Concept or skill demonstrated at mastery level |
| 0100 | Revisited | Previously completed content reviewed |
| 0101 | Assigned | Content assigned by Academy system or instructor |
| 0110 | Progress | Mid-activity update; partial completion |
| 0111 | Certified | Achievement, badge, or certificate earned |
| 1000 | Reset | Progress reset by user or system |
| 1001 | Recommended | Content suggested by Academy or peer entity |
| 1010 | Collaborative | Activity completed with another entity |
| 1011 | Taught | Sovereign taught this content to another entity |
| 1100 | Blocked | Content access blocked pending prerequisite |
| 1101 | State Commit | Learning period snapshot; Tally locked |
| 1110 | Correction / Void | — |
| 1111 | Compound Continuation | — |

### 7.3 Location Domain Account Pairs

| Code | Name | Meaning |
|------|------|---------|
| 0000 | Current | Live position fix from sensor |
| 0001 | Arrived | Sovereign or entity entered a named place |
| 0010 | Departed | Sovereign or entity left a named place |
| 0011 | Defined | Place created, named, and stored |
| 0100 | Visited | Historical visit recorded after the fact |
| 0101 | Waypoint | Navigation reference or intermediate point |
| 0110 | Geofence | Boundary defined around a geographic area |
| 0111 | Proximity | Proximity event; entity detected near a place |
| 1000 | Associated | Record linked to this place by context |
| 1001 | Route | Path between two defined places |
| 1010 | Aggregate | Visit count or duration summary |
| 1011 | Shared | Place record shared with another entity |
| 1100 | Revised | Place definition updated |
| 1101 | State Commit | Location period snapshot |
| 1110 | Correction / Void | — |
| 1111 | Compound Continuation | — |

### 7.4 Social Domain Account Pairs

| Code | Name | Meaning |
|------|------|---------|
| 0000 | Created | New contact record established |
| 0001 | Updated | Contact information changed |
| 0010 | Merged | Two contact records unified into one |
| 0011 | Introduced | Entity A introduced to Entity B through this record |
| 0100 | Trusted | Entity explicitly marked as trusted by Sovereign |
| 0101 | Mutual | Bilateral relationship confirmed by both parties |
| 0110 | Blocked | Contact or access blocked |
| 0111 | Following | One-way relationship declared |
| 1000 | Organization | Person linked to an Island or business entity |
| 1001 | DID Bound | DID associated with this contact record |
| 1010 | DID Verified | DID confirmed through a successful interaction |
| 1011 | Inactive | Contact marked dormant |
| 1100 | Referred | Contact introduced via an external referral |
| 1101 | State Commit | Relationship snapshot |
| 1110 | Correction / Void | — |
| 1111 | Compound Continuation | — |

### 7.5 Agreement Domain Account Pairs

| Code | Name | Meaning |
|------|------|---------|
| 0000 | Proposed | Draft terms transmitted to counterparty |
| 0001 | Accepted | Party confirmed and signed the terms |
| 0010 | Rejected | Party declined the terms |
| 0011 | Counter | Revised terms returned to proposing party |
| 0100 | Active | Agreement entered into force |
| 0101 | Breached | An obligation was not met by a party |
| 0110 | Amended | Terms changed by mutual consent of all parties |
| 0111 | Completed | All obligations fulfilled; Agreement closed |
| 1000 | Arbitrated | Dispute referred to named arbitrator or process |
| 1001 | Resolved | Dispute settled; outcome recorded |
| 1010 | Suspended | Agreement temporarily paused by mutual consent |
| 1011 | Expired | Agreement lapsed by time without completion |
| 1100 | Enforced | Term or obligation formally enforced |
| 1101 | State Commit | Agreement snapshot at defined interval |
| 1110 | Correction / Void | — |
| 1111 | Compound Continuation | — |

### 7.6 Schedule Domain Account Pairs

| Code | Name | Meaning |
|------|------|---------|
| 0000 | Scheduled | Event created and time assigned |
| 0001 | Confirmed | Attendance confirmed by a participant |
| 0010 | Declined | Attendance declined by a participant |
| 0011 | Rescheduled | Event time, location, or duration changed |
| 0100 | Cancelled | Event cancelled; participants notified |
| 0101 | Started | Event in progress |
| 0110 | Completed | Event concluded |
| 0111 | Missed | Event not attended; no-show recorded |
| 1000 | Reminded | Reminder delivered to participant(s) |
| 1001 | Recurring | Instance of a repeating scheduled event |
| 1010 | Delegated | Event responsibility transferred |
| 1011 | Joined | Entity added to an existing event |
| 1100 | Blocked | Time blocked; event conflicts |
| 1101 | State Commit | Calendar period snapshot |
| 1110 | Correction / Void | — |
| 1111 | Compound Continuation | — |

### 7.7 Media Domain Account Pairs

| Code | Name | Meaning |
|------|------|---------|
| 0000 | Referenced | Content item referenced from a record |
| 0001 | Attached | Content item attached to a record |
| 0010 | Linked | External URL or resource linked |
| 0011 | Embedded | Content inline in record (small items only) |
| 0100 | Shared | Content shared with another entity |
| 0101 | Received | Content received from another entity |
| 0110 | Published | Content made available beyond personal Island |
| 0111 | Archived | Content moved to archive status |
| 1000 | Revised | Content updated; version incremented |
| 1001 | Deleted | Content reference voided |
| 1010 | Transcribed | Audio or visual content transcribed to text |
| 1011 | Translated | Content translated to another language |
| 1100 | Captioned | Caption or metadata added |
| 1101 | State Commit | Media library snapshot |
| 1110 | Correction / Void | — |
| 1111 | Compound Continuation | — |

### 7.8 Environment Domain Account Pairs

| Code | Name | Meaning |
|------|------|---------|
| 0000 | Sampled | Sensor reading taken at a point in time |
| 0001 | Averaged | Mean of multiple samples over a period |
| 0010 | Peak | Maximum value recorded in a period |
| 0011 | Minimum | Minimum value recorded in a period |
| 0100 | Threshold | Value exceeded a defined safe limit |
| 0101 | Normal | Value confirmed within expected range |
| 0110 | Calibrated | Sensor calibration reference point recorded |
| 0111 | Forecast | Predicted future value |
| 1000 | Anomaly | Statistically unusual value flagged |
| 1001 | Aggregate | Summary over a period |
| 1010 | Compared | Value compared to reference or prior period |
| 1011 | Shared | Reading shared with another entity or service |
| 1100 | Corrected | Sensor error corrected |
| 1101 | State Commit | Environmental period snapshot |
| 1110 | Correction / Void | — |
| 1111 | Compound Continuation | — |

---

## 8. Extension Mechanism

### 8.1 Distribution-Defined Task Codes

The task_code space is fully allocated in this version of the Codebook Standard. Distributions requiring additional task verbs must apply for ratification in the next minor version of this document, proposing values in the range 0x00–0x3F that are currently reserved (none in v1.0; all 64 values are defined).

Until ratification, distributions may use a custom domain declaration (domain=11, extension byte 0xF0–0xFF) to carry distribution-specific task semantics without conflicting with standard task_codes. This is the correct extension path for domain-specific action verbs that do not generalise across ZAKO.

### 8.2 Distribution-Defined Custom Domains

Distributions may declare their own custom domains using extension byte values 0xF0–0xFF. These values are reserved for distribution use and will not be allocated by the ZAKO Codebook Standard. A distribution using a custom domain must:

1. Document the domain's quantity type codebook and account pair codebook
2. Ensure that records carrying this domain value are ignored (stored but not semantically processed) by implementations that do not recognise the domain
3. Not use reserved codebook entries in ways that conflict with this Standard

### 8.3 Distribution-Defined Quantity Types

Within any standard custom domain, quantity type values 0x30–0x3F are reserved for distribution-defined use. Distributions may assign these values for domain-specific measurements that are not yet ratified in the standard codebook. Distribution-defined values must not conflict with standard values (0x00–0x2F).

A distribution using these values must document them and must ensure that records carrying unrecognised quantity type values are stored and preserved without semantic processing by implementations that do not recognise them.

### 8.4 Proposing Additions to the Standard Codebook

Additions to this Codebook Standard are proposed by submitting a change describing:
- The proposed entry (code, name, definition, unit, encoding)
- The domain and position in the codebook where it belongs
- The rationale for why it cannot be expressed using existing entries
- At least one concrete ZAKO record example using the proposed entry

Minor additions (new quantity types, new custom domain declarations) are ratified as minor version updates. Redefinitions of existing entries are not permitted in minor versions.

---

## 9. Reserved Entries Summary

| Position | Reserved Range | Disposition |
|----------|---------------|-------------|
| Task codes | None — all 64 defined | n/a |
| Custom domains | 0x0B–0xEF | Reserved for future standard use |
| Custom domains | 0xF0–0xFF | Distribution-defined |
| Health quantities | 0x15–0x2F | Reserved |
| Health quantities | 0x30–0x3F | Distribution-defined |
| Academy quantities | 0x0C–0x2F | Reserved |
| Academy quantities | 0x30–0x3F | Distribution-defined |
| Location quantities | 0x0B–0x2F | Reserved |
| Location quantities | 0x30–0x3F | Distribution-defined |
| Social quantities | 0x07–0x2F | Reserved |
| Social quantities | 0x30–0x3F | Distribution-defined |
| Agreement quantities | 0x08–0x2F | Reserved |
| Agreement quantities | 0x30–0x3F | Distribution-defined |
| Schedule quantities | 0x07–0x2F | Reserved |
| Schedule quantities | 0x30–0x3F | Distribution-defined |
| Media quantities | 0x07–0x2F | Reserved |
| Media quantities | 0x30–0x3F | Distribution-defined |
| Environment quantities | 0x0B–0x2F | Reserved |
| Environment quantities | 0x30–0x3F | Distribution-defined |

Implementations must store and preserve records carrying reserved values. They must not assign meaning to reserved values and must not reject records solely because they carry a reserved value. Reserved values may be assigned meaning in future versions of this document; implementations that store them intact will decode them correctly when they upgrade.

---

## Appendix: Codebook Quick Reference

```
Task Codes
  0x00–0x07  System     POWER_CHANGE  MODE_CHANGE  GATE        RESTORE
                        CALIBRATE     ASSIGN       SUSPEND     RESUME
  0x08–0x0F  Identity   GRANT         REVOKE       DELEGATE    EXPIRE
                        CREATE        BIND         ATTEST      VERIFY
  0x10–0x17  Structural OPEN          CLOSE        JOIN        LEAVE
                        REMOVE        COMMIT       AMEND       DISPUTE
  0x18–0x1F  Exchange   SEND          RECEIVE      ACK         INVOICE
                        PAY           QUOTE        AGREE       COMPLETE
  0x20–0x27  Work       ASSIGN        START        PROGRESS    FINISH
                        INSPECT       REPORT       EXPENSE     TRAVEL
  0x28–0x2F  Observe    OBSERVE       NOTE         JOURNAL     CAPTURE
                        MEASURE       ESTIMATE     COMPARE     ALERT
  0x30–0x37  Schedule   SCHEDULE      RESCHEDULE   CANCEL      REMIND
                        TRIGGER       RECUR        ATTEND      DELEGATE
  0x38–0x3F  Learning   QUERY         RESPOND      ACT         DEFER
                        LEARN         TEACH        RECOMMEND   AUTOMATE

Custom Domains (extension byte)
  0x03  Health    0x04  Academy   0x05  Location  0x06  Social
  0x07  Agreement 0x08  Schedule  0x09  Media     0x0A  Environment
  0xF0–0xFF  Distribution-defined
```

---

*ZAKO Codebook Standard v1.0 — June 1, 2026*
*Sub-protocol of ZAKO Standard v1.x*
*Cross-reference: ZAKO Wire Conventions v1.0, ZAKO Standard §3*
