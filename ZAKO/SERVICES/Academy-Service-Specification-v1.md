# Academy Service Specification
## Version 1.0

*June 1, 2026*

---

> Learning is a record of becoming. A lesson completed, a skill demonstrated, a credential earned, a question asked and answered — each is an event with a timestamp, a verifiable source, and a chain-hash that proves it happened in that order. Academy makes learning legible in the same ledger that makes work and exchange legible. The same sovereignty that protects health data protects learning history. The same offline-first model that enables field workers to invoice enables students without internet to study, assess, and certify.

---

## 1. Purpose and Scope

This document is a service specification within the HOME Standard. It defines the Academy service: the Academy Island model, the Learning Engine relationship, content consumption and assessment records, credential issuance, and integration with the Exchange Engine for paid learning contexts.

In scope:
- Academy Island structure (sub_entity=5, file_sep=5)
- Content consumption, assessment, and mastery lifecycle
- Certificate and credential records
- Learning Engine integration (OPPORTUNISTIC inference, recommendations)
- Academy service as external gateway
- Engagement mechanics: streaks, goals, XP
- Period Tally records for learning
- Conformance requirements

Out of scope:
- Content format specifications (video, audio, document)
- Curriculum design and learning pathway logic
- External LMS integrations
- Assessment item formats

---

## 2. Academy Island

### 2.1 Structure

The Academy Island occupies sub_entity=5, file_sep=5 in the Personal Island. Academy records are written here by the Sovereign (manual study entries), by the Academy service (content delivery and assessment events), and by the Learning Engine (recommendations, inferences, progress summaries).

### 2.2 record_sep Assignments

| record_sep | Category |
|------------|----------|
| 0 | Formal courses and curricula |
| 1 | Informal learning (self-study, reading, exploration) |
| 2 | Assessments and evaluations |
| 3 | Credentials and certificates |
| 4 | Collaborative learning events |
| 5 | Teaching events (Sovereign taught others) |
| 6 | Recommendations received and acted on |
| 7 | Goals and streaks |
| 8 | Period close (Tally records) |
| 9–15 | Reserved |

### 2.3 The Academy Service

The Academy service is the external gateway — the component that connects the Academy Island to content providers, assessment services, and credential authorities. It mediates:

- Content delivery to the device (offline-first; content cached locally before study begins)
- Assessment submission and scoring
- Credential issuance by recognised authorities
- Recommendation feeds from the distribution's content graph

The Academy service operates at BACKGROUND process class for content prefetch and recommendation generation. Assessment scoring runs at INTERACTIVE priority. Learning Engine analysis runs at OPPORTUNISTIC priority.

---

## 3. Content Consumption Records

### 3.1 CONSUME (Consumed)

A content item opened, started, or accessed:

```
task_code     = 0x3C   LEARN
account_pair  = 0000   (Consumed — Academy domain)
domain        = 11; domain_ext = 0x04
file_sep      = 5      Academy sub-entity
record_sep    = 0      Formal course [or 1 for informal]
quantity_type = 0x00   Lesson completion (%)
value         = 0      (0% at open; final value at COMPLETE)
wall_ts       = start_epoch
note          = <content_id; content title>
```

### 3.2 COMPLETE (Completed)

A content item finished:

```
task_code     = 0x3C   LEARN
account_pair  = 0001   (Completed — Academy domain)
quantity_type = 0x00   Lesson completion
value         = 1000   (100.0% — value × 10 encoding)
wall_ts       = completion_epoch
note          = <content_id>
```

COMPLETE records carry the total time engaged as a compound continuation:

```
Record 1 — COMPLETE (Completeness=1):
  [as above]

Record 2 — Time engaged (1111, sub_type=00, Completeness=0):
  quantity_type = 0x02   Time engaged (minutes)
  value         = minutes_spent
```

### 3.3 PROGRESS

For long-form content where the Sovereign checkpoints mid-way:

```
task_code     = 0x39   RESPOND  [repurposed in Academy domain: mid-content checkpoint]
account_pair  = 0110   (Progress — Academy domain)
quantity_type = 0x00   Lesson completion
value         = percent_complete × 10
```

### 3.4 REVISIT

A previously completed item reviewed:

```
task_code     = 0x3C   LEARN
account_pair  = 0100   (Revisited — Academy domain)
quantity_type = 0x08   Revision count
value         = revision_number  (how many times this item has been revisited)
```

---

## 4. Assessment Records

### 4.1 Assessment Taken

```
task_code     = 0x38   QUERY  [repurposed: assessment attempt]
account_pair  = 0010   (Assessed — Academy domain)
domain        = 11; domain_ext = 0x04
record_sep    = 2      Assessments
quantity_type = 0x01   Assessment score (%)
value         = score × 10
wall_ts       = assessment_epoch
note          = <assessment_id; content_area>
```

### 4.2 Mastery Achieved

When a Sovereign demonstrates mastery (configurable threshold per content domain):

```
task_code     = 0x3C   LEARN
account_pair  = 0011   (Mastered — Academy domain)
quantity_type = 0x05   Mastery level (0–50 scale)
value         = mastery_level
note          = <skill or concept identifier>
```

Mastery records are verifiable: they reference the assessment frame_hash that triggered them, allowing any counterparty to verify the mastery claim against the assessment record.

### 4.3 Assessment Compound Record

A complete assessment event — including the score, time spent, and mastery determination — is expressed as a compound group:

```
Record 1 — Score (Completeness=1):
  task_code     = 0x38   QUERY
  account_pair  = 0010   Assessed
  quantity_type = 0x01   Assessment score
  value         = score × 10

Record 2 — Time spent (1111, sub_type=00, Completeness=1):
  quantity_type = 0x02   Time engaged
  value         = minutes_spent

Record 3 — Mastery determination (1111, sub_type=00, Completeness=0):
  task_code     = 0x3C   LEARN
  account_pair  = 0011   Mastered [or 0010 Assessed, if mastery not achieved]
  quantity_type = 0x05   Mastery level
  value         = resulting_mastery_level
```

---

## 5. Credentials and Certificates

### 5.1 Credential Record

A credential is a signed assertion that the Sovereign has demonstrated a defined standard of competence in a domain. It is issued as an ATTEST record (task_code=0x0E) in the Identity sub-entity, referencing the Academy Island record that established the competence:

```
task_code     = 0x0E   ATTEST
account_pair  = 0111   (Certified — Academy domain when in Academy context)
domain        = 11; domain_ext = 0x04
file_sep      = 1      Identity sub-entity (credentials are identity claims)
source_did    = issuing_authority_did  [or sovereign_did for self-certified]
dest_did      = sovereign_did
value         = competence_level
note          = <credential title; issuing authority; expiry if any; mastery record frame_hash>
```

Credentials issued by external authorities (recognised Academy providers) carry the authority's DID as `source_did`. The Sovereign's own ATTEST records (self-certified competencies) carry the Sovereign's DID as `source_did` and are distinguished from authority-issued credentials by the receiver.

### 5.2 Credential Verification

A credential presented to a counterparty is verifiable offline:
1. Counterparty receives the ATTEST record and the referenced mastery record
2. Counterparty verifies the ATTEST signature against `source_did`
3. Counterparty verifies the mastery record signature against the Sovereign's DID
4. Counterparty optionally verifies the assessment record referenced by the mastery record
5. The chain from assessment to mastery to credential is locally verifiable without any network call

---

## 6. Engagement Mechanics

### 6.1 Streaks

A streak is a consecutive-day engagement sequence. HOME tracks streaks by counting consecutive calendar days with at least one COMPLETE or QUERY record in the Academy Island. The streak count is stored in a daily record:

```
task_code     = 0x3C   LEARN
account_pair  = 0110   (Progress — Academy domain)
quantity_type = 0x04   Streak (consecutive days)
value         = current_streak_length
record_sep    = 7      Goals and streaks
```

Streak records are generated by the Academy service at the end of each day in which learning activity occurred. A missed day produces no record — the absence itself marks the streak break. The Academy service computes the current streak at query time by counting backward from the most recent record.

### 6.2 Experience Points (XP)

XP is a cumulative engagement measure:

```
task_code     = 0x3C   LEARN
account_pair  = 0110   (Progress — Academy domain)
quantity_type = 0x03   Points or XP earned
value         = xp_earned_in_event
record_sep    = 7
```

Total XP is computed as the sum of all XP records in the Academy Island — not stored as a single mutable balance. The append-only ledger is the XP ledger.

### 6.3 Goals

A learning goal is an ASSIGN record in the Academy Island:

```
task_code     = 0x3D   TEACH  [repurposed in Academy domain: self-directed goal]
account_pair  = 0101   (Assigned — Academy domain)
domain        = 11; domain_ext = 0x04
record_sep    = 7
value         = target_value (e.g., lessons per week, hours per day)
note          = <goal description; target period>
```

Goal completion is recorded as a COMPLETE record referencing the goal's frame_hash.

---

## 7. Learning Engine Integration

### 7.1 The Learning Engine's Role in Academy

The Learning Engine analyses Academy Island records to:
- Identify content gaps (topics assessed but not mastered)
- Detect study patterns (optimal study times, retention curves)
- Generate recommendations (next content based on mastery history)
- Surface forgotten material (spaced repetition scheduling)

All Learning Engine analysis runs at OPPORTUNISTIC process class in Full Power mode. In Standard mode, only spaced repetition scheduling runs (as BACKGROUND). Analysis stops completely in Conservation mode.

### 7.2 Recommendation Records

Learning Engine recommendations are stored in the Academy Island with source_did of the Learning Engine sub-entity:

```
task_code     = 0x3E   RECOMMEND
account_pair  = 1001   (Recommended — Academy domain)
record_sep    = 6
value         = recommendation_confidence_score × 100
note          = <content_id; rationale summary>
source_did    = learning_engine_did
```

Recommendations are never forced. They appear in the Tasks View as suggestions. The Sovereign acts on them or ignores them; both outcomes are recorded (acting on a recommendation produces a CONSUME record referencing the recommendation's frame_hash; ignoring it produces no record, which the Learning Engine reads as a negative signal after a configurable timeout).

### 7.3 Spaced Repetition Scheduling

When the Learning Engine schedules a review (spaced repetition), it writes a SCHEDULE record (task_code=0x30) in the Schedule domain referencing the Academy content item:

```
task_code     = 0x30   SCHEDULE
domain        = 11; domain_ext = 0x08  Schedule
value         = scheduled_review_epoch
note          = <content_id; review reason: "spaced repetition">
```

This record appears in the Tasks View as a scheduled review task, linking it to the Calendar and Tasks interfaces.

---

## 8. Paid Learning

When the Academy service delivers paid content (courses, assessments, certifications with fees), the payment is handled by the Exchange Engine using the standard payment compound pattern. The learning event and the payment are posted atomically:

```
Record 1 — Content access payment (Completeness=1):
  task_code    = 0x1C   PAY
  account_pair = 1001   (Payable/Asset — Financial domain)
  domain       = 00     Financial
  value        = fee_amount
  dest_did     = content_provider_did

Record 2 — Content access record (1111, sub_type=00, Completeness=0):
  task_code    = 0x08   GRANT
  domain       = 11; domain_ext = 0x04  Academy
  account_pair = 0101   (Assigned — Academy domain)
  note         = <content_id granted; access duration if time-limited>
```

The GRANT record in Record 2 is the access credential. The Sovereign presents it to the Academy service as proof of payment. The Academy service verifies the GRANT record's signature and the referenced PAY record before delivering content.

---

## 9. Academy Period Tallies

A learning period Tally summarises Academy activity over a defined interval:

```
Record 1 — Lessons completed (group_sep=63, Completeness=1):
  task_code     = 0x15   COMMIT
  account_pair  = 1101   State Commit
  domain        = 11; domain_ext = 0x04
  record_sep    = 8      Period close
  group_sep     = 63
  quantity_type = 0x0A   Content items completed
  value         = items_completed_in_period

Record 2 — Time engaged (1111, sub_type=00, Completeness=1):
  quantity_type = 0x02   Time engaged
  value         = total_minutes_in_period

Record 3 — XP earned (1111, sub_type=00, Completeness=1):
  quantity_type = 0x03   XP
  value         = total_xp_in_period

Record 4 — Credentials earned (1111, sub_type=00, Completeness=0):
  quantity_type = 0x07   Certificate or milestone
  value         = credentials_earned_in_period
  note          = <period label>
```

---

## 10. Conformance Requirements

1. **Offline-first content model.** Content required for an enrolled course must be available offline. The Academy service must not require network connectivity to deliver enrolled content, conduct assessments, or record completion events.

2. **Credentials are verifiable without network.** Credential records must be verifiable by a counterparty using the Sovereign's public key alone, with no external resolver required.

3. **Learning Engine analysis is OPPORTUNISTIC.** No Learning Engine analysis task runs in Conservation mode or below. Recommendations and trend analysis are deferred without notification to the Sovereign.

4. **Assessment records are immutable.** Assessment scores, once written to `telux-ledgerd`, are never modified. Retakes produce new assessment records alongside the original. Both remain in the ledger.

5. **XP is computed, not stored as balance.** No implementation stores XP as a mutable balance field. XP is always the sum of XP records in the period, computed at read time.

6. **Paid content grants are atomic.** Payment and content access grant are posted as a compound group. Partial posting (payment without access, or access without payment) is a protocol error.

---

*Academy Service Specification v1.0 — June 1, 2026*  
*Sub-protocol of HOME Standard v1.x*  
*Cross-reference: HOME Codebook Standard v1.0 §6.2 and §7.2, Telux Protocol v1.0 §5, Outstack Protocol v1.0 §4.1*
