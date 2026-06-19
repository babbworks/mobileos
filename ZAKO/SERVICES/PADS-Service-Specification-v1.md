# PADS Service Specification
## Work Record Service — Version 1.0

*June 1, 2026*

---

> Work is the primary relationship between most HOME users and the economic world. A job assigned, a site visited, a report filed, a payment requested, an expense incurred — these are the records that make a livelihood legible. PADS is the HOME service that makes them sovereign: every work event recorded on-device, signed by the Sovereign, legible to any counterparty with access, and expressible over any channel that can carry a BitPads frame.

---

## 1. Purpose and Scope

This document is a service specification within the HOME Standard. It defines the PADS Work Record Service: its Island model, record lifecycle, counterparty access model, integration with the Exchange Engine, pads-v1 compatibility path, and conformance requirements.

PADS is the internal HOME name for the Work Record Service. The external application built on PADS is called Workpads. HOME distributions expose the PADS service to Workpads and to any other application with appropriate capability grants. The service layer does not change based on the external application name.

In scope:
- The Work Island model and sub-entity structure
- The work record lifecycle (all eight work verbs)
- Expense and travel records
- Work period management and Tally records
- Counterparty access model (reading and responding to work records)
- Integration with the Exchange Engine for work+payment compound records
- pads-v1 URL encoding compatibility path
- Service Views that PADS contributes to
- Process class assignments for PADS daemons

Out of scope:
- Workpads application UI
- Invoice templates and formatting
- External payroll or accounting system integrations

---

## 2. The Work Island Model

### 2.1 Work Islands

A Work Island is a named working context that organises a set of related work records. It lives in the PADS sub-entity namespace (sub_entity 16–31 of the Personal Island container). Each Work Island represents one working context: a client, a project, a contract, an employer, or a trade.

A Work Island has:
- A unique Island ID (derived from its CREATE record's frame_hash)
- A human-readable name (carried in the note component of the CREATE record)
- An optional counterparty DID (the client or employer entity's sovereign DID)
- A record of all work events occurring within that context
- Its own Tally records closing each work period

A HOME device supports up to 16 active Work Islands simultaneously (sub_entity 16–31). Work Islands beyond this limit require archiving an existing island to free a sub-entity slot. Archived islands remain in `telux-ledgerd`; they are removed from the active working set.

### 2.2 Work Island Creation

Creating a Work Island writes a CREATE record to the Identity sub-entity (file_sep=1) of the Personal Island, plus an OPEN record to the new island's own ledger context:

**CREATE record (in Identity sub-entity):**
```
task_code    = 0x0C   CREATE
account_pair = 0000   (Source→Sink; Island instantiated)
domain       = 10     Hybrid
file_sep     = 1      Identity sub-entity
sub_entity   = <assigned slot: 16–31>
value        = island_creation_epoch
source_did   = sovereign_did
note         = <island name; UTF-8; max 64 bytes>
```

**OPEN record (in new Island's ledger context):**
```
task_code    = 0x10   OPEN
account_pair = 0000
domain       = 10     Hybrid
file_sep     = <island slot: 0–7 within PADS container>
record_sep   = 0      (Self-record; no counterparty at open)
group_sep    = 0      (First position in island)
sub_entity   = <island slot>
source_did   = sovereign_did
```

The two records are written as a compound group to ensure atomic creation: if either fails, neither is stored.

### 2.3 Work Island Naming Convention

Work Island names are free-form UTF-8 strings stored in the note component of the CREATE record. They are not parsed by PADS; they are display names for the Sovereign's reference. Maximum 64 bytes. Emoji permitted.

A Work Island may be renamed by writing an AMEND record (task_code=0x16) referencing the original CREATE record's frame_hash, with the new name in the note component. The original name remains in the ledger via the original CREATE record.

### 2.4 Work Island Sub-Entity Structure

Within a Work Island, work records are organised by `record_sep` to reflect the exchange relationship type. The PADS-defined record_sep values within a Work Island:

| record_sep | Relationship | Records |
|------------|-------------|---------|
| 0 | Self-assigned | Work assigned to self; solo work context |
| 1 | Outbound exchange | Work sent to counterparty; Sovereign is provider |
| 2 | Inbound exchange | Work received from counterparty; Sovereign is recipient |
| 3 | Bidirectional | Work where both parties contribute |
| 4 | Expense | Financial outflows associated with this island's work |
| 5 | Travel | Travel events associated with this island's work |
| 6 | Inspection | Inspection and report events |
| 7 | Period close | Tally records for this island's work periods |
| 8–15 | Reserved for Work Island extension | — |

---

## 3. The Work Record Lifecycle

### 3.1 Overview

A complete work lifecycle moves through some or all of the following stages. Not all stages are required for all work types. The lifecycle is not a strict state machine — records may be written in any order the Sovereign chooses — but the standard sequence is:

```
ASSIGN → START → [PROGRESS]* → FINISH → [INSPECT] → [REPORT]
                                                 ↘ [EXPENSE / TRAVEL]
```

Each stage is a BitPads record. Each record is signed by the authoring Sovereign. Counterparty records (e.g., a client's ACK or a receipt from a supplier) are signed by the counterparty's Sovereign.

### 3.2 ASSIGN (0x20)

The work assignment record. Marks the beginning of a trackable work item.

**Self-assignment (Sovereign assigns work to themselves):**
```
task_code    = 0x20   ASSIGN
account_pair = 0000   (Source→Sink; work flows from assignment to execution)
domain       = 10     Hybrid
record_sep   = 0      Self-assigned
group_sep    = 0      First record in this work item
value        = estimated_work_units (encoding defined per Work Island's unit convention)
source_did   = sovereign_did
note         = <work description>
```

**Outbound assignment (Sovereign assigns work to counterparty):**
```
task_code    = 0x20   ASSIGN
account_pair = 0001   (Parent→Child; Sovereign delegates work)
record_sep   = 1      Outbound exchange
value        = estimated_work_units
source_did   = sovereign_did
dest_did     = counterparty_did
note         = <assignment description>
```

**Inbound assignment (Sovereign receives work from counterparty):**
```
task_code    = 0x20   ASSIGN
account_pair = 0001   (Parent→Child; counterparty delegates)
record_sep   = 2      Inbound exchange
value        = estimated_work_units
source_did   = counterparty_did
dest_did     = sovereign_did
```

### 3.3 START (0x21)

Written when work begins. Carries the actual start timestamp in the time component. For time-tracked work, the START record is the anchor for duration calculation.

```
task_code    = 0x21   START
account_pair = 0101   (Generation/Input — work enters active state)
domain       = 10
value        = 0      (Work units completed so far; zero at start)
wall_ts      = start_epoch
```

### 3.4 PROGRESS (0x22)

Optional intermediate update. Written when the Sovereign wants to record partial completion or share status with a counterparty.

```
task_code    = 0x22   PROGRESS
account_pair = 0000   (Source→Sink; progress flows)
value        = units_completed_to_date
wall_ts      = progress_epoch
note         = <optional status note>
```

Multiple PROGRESS records are permitted. Each is independent — it records cumulative work units completed, not the delta since the last PROGRESS record.

### 3.5 FINISH (0x23)

Written when the Sovereign considers their work complete. Carries the final work unit count and the actual finish timestamp.

```
task_code    = 0x23   FINISH
account_pair = 0100   (Loss/Dissipation — work consumed to produce output)
value        = total_work_units_completed
wall_ts      = finish_epoch
note         = <optional completion note>
```

### 3.6 INSPECT (0x24)

Written when an inspection event occurs at a work site or on a delivered output. May be written by the Sovereign (self-inspection) or by a counterparty with write capability on the Work Island.

```
task_code    = 0x24   INSPECT
account_pair = 0111   (Repayment — inspection verifies delivery obligation met)
value        = inspection_score_or_code  (interpretation per Work Island convention)
source_did   = inspector_did
note         = <inspection findings>
```

### 3.7 REPORT (0x25)

A formal report submission. Used when the work product requires a documented output — a site inspection report, a service completion report, a progress report for a client.

```
task_code    = 0x25   REPORT
account_pair = 0000   (Source→Sink)
domain       = 10
value        = report_sequence_number    (increments per island per period)
source_did   = reporting_sovereign_did
dest_did     = recipient_did             (if submitted to specific party; NULL if retained)
note         = <report body or hash of external document>
```

A REPORT with `dest_did` set creates a copy obligation in `telux-sharedb`: when the Sovereign approves the share, `telux-sharedb` encodes the record and transmits it to the named counterparty via the appropriate channel.

### 3.8 EXPENSE (0x26)

Records a financial outflow associated with the work context. Expense records exist in two forms:

**Simple expense (no counterparty; Sovereign records their own expenditure):**
```
task_code    = 0x26   EXPENSE
account_pair = 1000   (Expense/Payable in Financial domain)
domain       = 00     Financial
record_sep   = 4      Expense relationship
value        = amount (in base currency units per Layer 2 batch context)
note         = <expense description: fuel, materials, tools, etc.>
```

**Reimbursable expense (Sovereign claims reimbursement from counterparty):**
An EXPENSE record followed by an INVOICE record as a compound group:
```
Record 1 — EXPENSE (Completeness=1, Partial):
  task_code    = 0x26   EXPENSE
  account_pair = 0110   (Receivable/Income — reimbursement owed)
  domain       = 00     Financial
  value        = amount

Record 2 — INVOICE reference (1111 continuation, sub_type=00, Completeness=0):
  account_pair = 1111
  sub_type     = 00
  task_code    = 0x1B   INVOICE
  dest_did     = counterparty_did
  value        = amount  (same amount; conservation holds within compound group)
```

### 3.9 TRAVEL (0x27)

Records a travel event associated with the work context. Travel records carry location context in compound form:

```
Record 1 — Travel event (Completeness=1, Partial):
  task_code    = 0x27   TRAVEL
  account_pair = 0000   (Source→Sink; Sovereign moves from A to B)
  domain       = 10     Hybrid
  record_sep   = 5      Travel relationship
  value        = distance_metres
  wall_ts      = departure_epoch

Record 2 — Origin location (1111 continuation, sub_type=00, Completeness=1):
  domain       = 11     Custom
  domain_ext   = 0x05   Location
  task_code    = 0x0F   VERIFY  (location confirmed)
  value        = origin_place_id_hash   (reference to a Places Island record)

Record 3 — Destination location (1111 continuation, sub_type=00, Completeness=0):
  domain       = 11     Custom
  domain_ext   = 0x05   Location
  task_code    = 0x0F   VERIFY
  value        = destination_place_id_hash
```

If the origin or destination is not a named place in the Places Island, the value field carries a raw coordinate hash instead of a place_id_hash. The note component of Record 1 may carry a free-text location description.

---

## 4. Work Period Management

### 4.1 Work Periods

A work period is a defined span of time within a Work Island's history. Periods are closed by Tally records. A Tally in a Work Island records the aggregate work units completed, total expenses, and total distance travelled in the period.

Work period Tallies are always the last record in a batch (group_sep=63) as required by HOME Wire Conventions §5.1.

### 4.2 Work Tally Record

```
task_code    = 0x15   COMMIT
account_pair = 1101   State Commit
domain       = 10     Hybrid
record_sep   = 7      Period close relationship
group_sep    = 63     Period Close (mandatory for Tally)
value        = total_work_units_in_period
source_did   = sovereign_did
note         = <period label: e.g., "Week ending 2026-06-07">
```

The Tally value carries total work units. Expense totals for the period are carried in a compound continuation:

```
Record 1 — Work Tally (Completeness=1, Partial):
  [as above]

Record 2 — Expense Tally (1111 continuation, sub_type=00, Completeness=1):
  domain       = 00    Financial
  task_code    = 0x15  COMMIT
  value        = total_expenses_in_period

Record 3 — Distance Tally (1111 continuation, sub_type=00, Completeness=0):
  domain       = 11 / 0x05  Location
  task_code    = 0x15  COMMIT
  value        = total_distance_metres_in_period
```

`telux-ledgerd` verifies the Tally value against the sum of all work unit records in the period before storing the Tally, consistent with the conservation invariant.

### 4.3 Period Boundaries

A work period begins immediately after the previous Tally record (or at the Work Island's genesis if no prior Tally exists) and ends at the current Tally. The period is identified by the frame_hash of its closing Tally record.

Periods may span any duration the Sovereign chooses: daily, weekly, fortnightly, monthly, or project-defined. PADS does not enforce a period duration.

---

## 5. Counterparty Access

### 5.1 Granting Work Island Access

When a Sovereign shares a Work Island with a counterparty (client, employer, partner), they issue a capability grant via `telux-identd`:

```
task_code    = 0x08   GRANT
capability_scope_bitmask:
  Bit 10: Read Work Island (counterparty can see completed work records)
  Bit  8: Write Exchange sub-entity (counterparty can write ACK/RECEIVE responses)
  [Bit 9: Write Work Island — only if counterparty should add records to this island]
dest_did     = counterparty_did
```

The read capability (Bit 10) allows the counterparty to read all records in the Work Island except those with `restrict_fwd=1`. The write capability (Bit 8) allows the counterparty to submit ACK, RECEIVE, INSPECT, and AGREE records into the Sovereign's Exchange sub-entity in response to the Sovereign's SEND, INVOICE, and REPORT records.

### 5.2 Counterparty Record Reception

When a counterparty submits a response record (ACK, RECEIVE, INSPECT, AGREE), the Sovereign's `telux-sharedb` receives it, validates the counterparty's signature against their DID, and forwards it to `telux-ledgerd` for storage. The counterparty's records are stored in the Exchange sub-entity (file_sep=2), not in the Work Island itself — they are exchange events, not work events.

```
Counterparty ACK:
  task_code    = 0x1A   ACK
  account_pair = 0111   (Repayment — obligation acknowledged)
  file_sep     = 2      Exchange sub-entity (stored here, not in Work Island)
  source_did   = counterparty_did
  dest_did     = sovereign_did
  value        = acknowledged_record_frame_hash (truncated)
```

### 5.3 Revoking Counterparty Access

When a work context ends, the Sovereign revokes the counterparty's capability grant. The REVOKE record follows the standard Telux protocol (§7.2). On revocation, `telux-ledgerd` begins refusing read and write operations from the counterparty's DID for that Work Island.

---

## 6. Exchange Engine Integration

### 6.1 When Exchange Engine Is Involved

The Exchange Engine mediates PADS work transactions whenever:
- The work carries a financial obligation (invoice, payment, reimbursement)
- Both the Sovereign and a counterparty need to post matching records
- Conservation must be enforced across the transaction

Exchange Engine integration uses the Work+Payment compound pattern defined in HOME Wire Conventions §7.2 (Pattern B).

### 6.2 Invoice Flow

The standard invoice flow for a completed work item:

```
Step 1 — Sovereign completes work:
  FINISH record in Work Island (as §3.5)

Step 2 — Sovereign generates invoice as compound group:
  Record A: INVOICE (task_code=0x1B, account_pair=0110 Receivable/Income)
            domain=00 Financial; value=invoice_amount; dest_did=client_did
  Record B: Work reference (1111, sub_type=00, Completeness=0)
            task_code=0x18 SEND; value=FINISH_record_frame_hash

Step 3 — telux-sharedb transmits invoice record to counterparty

Step 4 — Counterparty receives, ACKs:
  ACK record arrives in Sovereign's Exchange sub-entity

Step 5 — Counterparty pays:
  PAY record arrives (task_code=0x1C, account_pair=0111, value=payment_amount)
  Conservation check: invoice_amount must equal payment_amount within the batch

Step 6 — Sovereign's Exchange Engine posts RECEIVE:
  task_code=0x19 RECEIVE; value=payment_amount; source_did=client_did
```

The conservation invariant is enforced at Step 5: the counterparty's PAY record and the Sovereign's RECEIVE record must sum to zero within the Exchange Engine batch. The Exchange Engine holds both legs pending until both arrive, then posts atomically.

### 6.3 Partial Payments

When a counterparty pays less than the invoiced amount, the Exchange Engine posts the partial payment with `account_pair=0110` (Receivable — partial; remainder outstanding) and creates a pending balance record. The outstanding amount is tracked as an open receivable until settled or written off.

---

## 7. pads-v1 Compatibility

### 7.1 The Compatibility Path

pads-v1 is the URL encoding format used by Workpads (the external application that runs on PADS). When a PADS record must be transmitted over a channel that expects a Workpads-compatible URL — such as an SMS to a party running a legacy Workpads client — `telux-sharedb` encodes the BitPads frame as a pads-v1 URL via the pads-v1 codec.

The pads-v1 URL format uses the `#1pa/` scheme tag. The codec maps HOME BitPads fields to pads-v1 field positions. This mapping is defined in the HOME Transmission Adapters document (forthcoming). The key mapping points:

| HOME BitPads field | pads-v1 field |
|--------------------|---------------|
| `task_code` (Work verbs 0x20–0x27) | Job type code |
| `source_did` | Sender identifier |
| `wall_ts` | Date/time field |
| `value_raw` | Quantity or amount |
| `note` component | Description / story field |
| `dest_did` | Recipient identifier |
| Work Island name | Client/project name |

### 7.2 Inbound pads-v1 Decoding

When a HOME device receives an inbound pads-v1 URL (from a Workpads application or legacy client), the pads-v1 codec decodes it into a BitPads frame which is then submitted to `telux-ledgerd` via the standard record intake path. The decoded record's `source_did` is populated from the sender information in the pads-v1 URL if available; otherwise it is set to a synthetic DID derived from the URL's sender identifier.

Inbound pads-v1 records that decode successfully are indistinguishable in the ledger from natively-created BitPads records. They are stored, chain-hashed, and served to Service Views in the same way.

### 7.3 pads-v1 Limitations

Not all HOME record types have pads-v1 equivalents. Records using HOME-specific compound patterns (blood pressure readings, power events, capability grants) cannot be encoded as pads-v1 URLs. The pads-v1 compatibility path applies only to Work verbs (task_code 0x20–0x27) and Exchange verbs (0x18–0x1F) used within a PADS work context. All other HOME records transmit as raw BitPads frames over appropriate channels.

---

## 8. Service Views

### 8.1 PADS in the Service View Architecture

PADS records contribute to multiple Service Views. A single BitPads record may appear in more than one Service View depending on which dimensions it carries:

| Service View | Condition for PADS record to appear |
|-------------|--------------------------------------|
| Tasks View | Any record with task_code in Work verbs (0x20–0x27) |
| Money View | Any record with domain=00 (Financial) or an INVOICE/PAY/EXPENSE record |
| Notes View | Any record with a note component present |
| Exchanges View | Any record with dest_did set (bilateral; counterparty-present) |
| Tallies View | Any record with task_code=0x15 COMMIT and account_pair=1101 |

The record is stored once in `telux-ledgerd`. Each Service View queries it through an indexed read filtered by the relevant dimensions. PADS does not maintain separate storage for each view.

### 8.2 Tasks View

The Tasks View presents all PADS records as a list of work items ordered by wall_ts, grouped by Work Island. It derives task status from the lifecycle record sequence:

| Most recent task record | Displayed status |
|------------------------|-----------------|
| ASSIGN only | Assigned / Not started |
| START present | In progress |
| PROGRESS present (after START) | In progress (with % complete if estimable) |
| FINISH present | Completed |
| INSPECT present (after FINISH) | Inspected |
| REPORT present | Reported |
| PAY/RECEIVE present | Settled |

Status is derived at read time from the ledger — not stored as a mutable status field. The append-only ledger is the source of truth; the status is a computed property.

### 8.3 Tallies View

The Tallies View shows all period-close Tally records across all Work Islands and other domains. In PADS context, each Tally represents a closed work period with its aggregate work units, expenses, and distance. The Tallies View allows the Sovereign to review historical period summaries and export them for invoicing or accounting purposes.

---

## 9. Process Class Assignments

| Daemon / Component | Process Class | Rationale |
|-------------------|--------------|-----------|
| PADS record creation (user-initiated) | INTERACTIVE | Sovereign is present and waiting |
| Work Island creation | INTERACTIVE | User-initiated setup |
| Counterparty record reception | INTERACTIVE | Incoming exchange; timely response expected |
| Period Tally generation | INTERACTIVE | User-initiated close |
| Background sync to counterparty | BACKGROUND | Periodic; not time-critical |
| pads-v1 inbound decode | BACKGROUND | Processing received URLs |
| Work period analysis and aggregation | BACKGROUND | Summary computation |
| Learning Engine work pattern analysis | OPPORTUNISTIC | Non-essential intelligence |

PADS does not operate any CRITICAL processes of its own. It relies on `telux-ledgerd` (CRITICAL) for all durable storage and `telux-identd` (CRITICAL) for all signing. PADS itself runs at INTERACTIVE for user-initiated actions and BACKGROUND for periodic work.

---

## 10. Offline Operation

### 10.1 PADS Is Fully Offline

PADS creates and stores work records entirely on-device. No network connection is required to:
- Create, read, or update any work record
- Generate an invoice
- Record expenses and travel
- Close a work period with a Tally

Network connectivity is required only to transmit records to counterparties. When connectivity is unavailable, `telux-sharedb` queues outbound records for transmission when connectivity resumes. The queue is stored in `telux-ledgerd` as a set of SEND records with `status=0` (Pending). On connectivity restoration, the queue is processed in order.

### 10.2 Conflict Resolution

Because PADS operates offline-first and counterparties may also be offline-first, two parties may independently write records that appear to conflict (e.g., both claim to have assigned the same work item). HOME does not attempt automatic conflict resolution.

Conflicts surface when records from both parties are present in `telux-ledgerd` for the same work item. The Service View presents both records with their source_did and wall_ts. The Sovereign resolves the conflict by writing a DISPUTE record (task_code=0x17) referencing both conflicting frame_hashes, followed by an AMEND record establishing the authoritative version.

---

## 11. Conformance Requirements

A HOME distribution conforms to this specification when:

1. **Work Island sub-entity numbering is respected.** Work Islands occupy sub_entity values 16–31 only. No other service uses these slots.

2. **All eight work verbs are handled.** The implementation produces and consumes records with task_code 0x20–0x27 correctly, storing them in `telux-ledgerd` with the correct domain, account_pair, and separator values.

3. **Period Tallies enforce conservation.** Work period Tally records are validated against the sum of work unit records in the period. A Tally whose value does not match the computed sum is rejected.

4. **Counterparty access is capability-gated.** No counterparty reads or writes Work Island records without a valid capability grant from the Work Island Sovereign.

5. **Exchange Engine integration is atomic.** Invoice and payment record pairs are handled as compound groups. Partial posting (one leg without the other) is not permitted.

6. **pads-v1 inbound records are stored without distinction.** Successfully decoded pads-v1 inbound records are stored in `telux-ledgerd` with the same validation and chain-hashing as natively-created records.

7. **Service Views are derived, not stored.** PADS does not maintain separate mutable status fields. Task status, period summaries, and all derived properties are computed from the append-only ledger at read time.

8. **Outbound queue is durable.** Queued-for-transmission records (SEND records with status=Pending) survive device restart. They are stored in `telux-ledgerd` and processed on next connectivity.

9. **PADS runs at INTERACTIVE and BACKGROUND.** No PADS-specific process claims CRITICAL class. CRITICAL operations are delegated to `telux-ledgerd` and `telux-identd`.

---

## Appendix A: Work Record Quick Reference

```
ASSIGN   (0x20) — Work assigned; self or counterparty; opens work item
START    (0x21) — Work begun; anchor timestamp for duration calculation
PROGRESS (0x22) — Partial update; cumulative units to date
FINISH   (0x23) — Work complete; final unit count and timestamp
INSPECT  (0x24) — Inspection event; may be written by counterparty
REPORT   (0x25) — Formal report submitted; dest_did if to specific party
EXPENSE  (0x26) — Financial outflow; compound with INVOICE for reimbursement
TRAVEL   (0x27) — Travel event; compound with location references
```

## Appendix B: Work Island record_sep Assignments

```
0  Self-assigned      Work assigned to Sovereign by Sovereign
1  Outbound exchange  Work sent to counterparty; Sovereign is provider
2  Inbound exchange   Work received from counterparty; Sovereign is recipient
3  Bidirectional      Both parties contribute
4  Expense            Financial outflows for this island
5  Travel             Travel events for this island
6  Inspection         Inspection and report events
7  Period close       Tally records
```

## Appendix C: Standard Work Account Pairs by Context

| account_pair | Code | Work Context |
|-------------|------|-------------|
| Source→Sink | 0000 | Self-assignment; progress; report |
| Parent→Child | 0001 | Outbound assignment to counterparty |
| Reservation/Escrow | 0110 | Invoice issued; payment pending |
| Repayment/Return | 0111 | Inspection confirms delivery; payment received |
| Transformation | 1000 | Work consumed to produce output (FINISH) |
| Generation/Input | 0101 | Work enters active state (START) |
| Loss/Dissipation | 0100 | Effort expended; no further recovery |
| State Commit | 1101 | Period Tally |

---

*PADS Service Specification v1.0 — June 1, 2026*  
*Sub-protocol of HOME Standard v1.x*  
*Cross-reference: HOME Wire Conventions v1.0 §3.2, HOME Codebook Standard v1.0 §3.5, Telux Protocol v1.0 §3.3, Outstack Protocol v1.0 §4*
