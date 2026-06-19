# Ya — PADS Active Record Types

The PADS work record types active on Ya v1. This document maps the normative PADS record vocabulary to the specific Ya deployment context — what each record type means in practice for Zambian informal sector field workers.

Reference: `ZAKO/SERVICES/PADS-Service-Specification-v1.md`, `ZAKO/PACKAGES/zako-pads.md`

---

## Active Work Record Types

All eight PADS work verbs are active on Ya. Their use in the Ya context:

### ASSIGN (0x20) — Work assignment

**In Ya:** A supervisor creates a work item and assigns it to a field worker (outbound ASSIGN), or a field worker assigns work to themselves (self-ASSIGN) for solo work tracking.

**Example:** A water infrastructure inspector is assigned to inspect 5 boreholes in a rural area. The supervisor's ASSIGN record arrives on the field worker's device via SMS.

```
task_code    = 0x20   ASSIGN
account_pair = 0001   (Parent→Child; supervisor assigns to worker)
value        = 5      (5 inspection units)
source_did   = supervisor's DID
dest_did     = field worker's DID
note         = "Inspect boreholes: BH-041 through BH-045, Choma district"
```

### START (0x21) — Work begun

**In Ya:** Field worker taps "Start work" when they arrive at the work site. The wall_ts of the START record is the anchor for duration tracking.

### PROGRESS (0x22) — Partial update

**In Ya:** Field worker records partial completion during long multi-day work. Optional — not required for short single-day work items.

### FINISH (0x23) — Work complete

**In Ya:** Field worker taps "Finish work" on completion. This record is the trigger for invoice generation. Its frame_hash is referenced in the INVOICE compound group.

### INSPECT (0x24) — Inspection event

**In Ya:** Either the field worker self-inspects their deliverable, or the supervisor writes an INSPECT record after reviewing the worker's REPORT. Carries an inspection score or verification code.

**Example:** A supervisor verifies borehole inspection photos submitted by the field worker. The supervisor writes an INSPECT record referencing the field worker's REPORT, with a score value.

### REPORT (0x25) — Formal report submission

**In Ya:** Field worker submits a site inspection report. If `dest_did` is set, `telux-sharedb` transmits the record to the supervisor. The note field carries report text or a hash of an external document (photo, PDF).

**Important for Zambia:** reports may reference photos taken at the field site. Photos themselves are not stored in the ledger — the note carries a hash of the photo for integrity verification.

### EXPENSE (0x26) — Financial outflow

**In Ya:** Field worker records a work-related expense: transport costs, materials purchased, tools rented. Reimbursable expenses generate a compound EXPENSE + INVOICE record for submission to the client.

**Example:** Field worker records fuel cost for motorbike travel to a remote borehole site.
```
task_code    = 0x26   EXPENSE
account_pair = 1000   (Expense/Payable — simple expense; no reimbursement)
domain       = 00     Financial
record_sep   = 4      Expense relationship
value        = 4500   (45.00 ZMW in centikwacha units)
note         = "Fuel: 3L petrol for Choma site travel, 2026-06-03"
```

### TRAVEL (0x27) — Travel event

**In Ya:** Records a travel leg associated with the work. Compound record carrying departure point, destination, and distance. Useful for distance-based reimbursement claims.

**Places Island integration:** Origin and destination are references to Places Island entries (named locations the Sovereign has recorded). Field workers regularly travelling to the same sites will have those sites as Places Island entries.

---

## Exchange Verbs Active on Ya

In addition to work verbs, these Telux Exchange verbs are used in PADS work flows:

| Verb | Code | Ya use |
|---|---|---|
| INVOICE | 0x1B | Worker invoices client for completed work |
| PAY | 0x1C | Client confirms payment (arrives as inbound record) |
| RECEIVE | 0x19 | Worker records payment received (Exchange Engine posts) |
| ACK | 0x1A | Client acknowledges work record receipt |
| AGREE | 0x1E | Mutual confirmation of a work agreement |
| COMPLETE | 0x1F | Exchange fully resolved; work item closed |

---

## Financial Domain on Ya

All financial records on Ya use `base_currency = ZMW` (Zambian Kwacha). Value fields in Financial domain records carry amounts in **centikwacha** (100 centikwacha = 1.00 ZMW). This gives sufficient precision for typical informal sector amounts (1 ZMW to 10,000 ZMW range) without floating point.

```
value = 150000  →  1,500.00 ZMW
value = 25000   →  250.00 ZMW
value = 500     →  5.00 ZMW
```

The PADS UI displays value fields formatted as ZMW amounts. The ledger stores raw centikwacha.

---

## Deferred Record Types (Not Active in Ya v1)

| Service | Record types | Status |
|---|---|---|
| Health service | Health measurements (blood pressure, weight, glucose) | Deferred — post-v1 |
| Academy service | Learning records (LEARN, COMPLETE, ASSESS) | Deferred — post-v1 |
| Agreements service | AGREE, TERMS records | Active via Telux Exchange Engine |

Health and Academy record types exist in the ZAKO Codebook Standard but are not surfaced in the Ya v1 PADS UI. They can be produced by future SIMBA-admitted services and will be stored correctly by `telux-ledgerd`.
