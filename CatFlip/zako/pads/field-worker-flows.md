# Ya — PADS Field Worker Flows

Concrete usage flows for the primary Ya user: an informal sector field worker in Zambia tracking daily work assignments, expenses, and payment.

These flows represent the design intent for the Workpads application built on PADS. They are specified here at the record level so that anyone building the app or testing the integration can see exactly what records are produced at each step.

Reference: `ZAKO/SERVICES/PADS-Service-Specification-v1.md`, `CatFlip/zako/pads/active-record-types.md`

---

## The Ya Field Worker Context

**Who:** A Zambian field worker — water inspector, agricultural extension officer, road surveyor, community health worker, construction site supervisor. Works for a contractor, NGO, or government agency.

**Connectivity:** Often in rural areas with intermittent LTE or 2G only. Device may spend full days offline. All work records created offline; exchange happens when connectivity arrives.

**Literacy:** Variable. The app must be usable by someone who is not highly literate in English. Record creation should require minimal typing — tap-based flows wherever possible.

**Phone number as identity:** In the Zambia context, the phone number (MSISDN) is the primary contact identifier. Telux maps MSISDN to DID automatically when a SIM is present.

---

## Flow 1: Self-Assigned Daily Work

The simplest case: a field worker records their own work without a counterparty.

**Scenario:** Water inspector records a day of borehole inspections.

**Step 1 — Create Work Island (first time only):**
```
User action: Opens PADS app → "New Work Context" → names it "Water Works Ltd"

Records written:
  CREATE (0x0C) in Identity sub-entity — creates Work Island, sub_entity=16
  OPEN (0x10) in new island's context
```

**Step 2 — Assign work to self:**
```
User action: Taps "New Task" → enters "Inspect BH-041 through BH-045"

Record written:
  ASSIGN (0x20)
  account_pair = 0000   (self-assigned)
  record_sep   = 0
  value        = 5      (5 inspection units)
  note         = "BH-041 through BH-045, Choma"
```

**Step 3 — Start work:**
```
User action: Taps "Start" when arriving at first borehole

Record written:
  START (0x21)
  wall_ts = [actual arrival time]
```

**Step 4 — Finish work:**
```
User action: Taps "Finish" at end of day

Record written:
  FINISH (0x23)
  value   = 5      (all 5 units completed)
  wall_ts = [completion time]
```

**Step 5 — Close the period (weekly):**
```
User action: Taps "Close Week" → confirms period summary

Records written (compound group):
  COMMIT (0x15) — Work Tally:
    record_sep = 7     (Period close)
    group_sep  = 63    (Period Close — mandatory)
    value      = 35    (total units this week across all tasks)
  COMMIT continuation — Expense Tally:
    domain = 00 (Financial)
    value  = 18500    (185.00 ZMW total expenses this week)
  COMMIT continuation — Distance Tally:
    domain = 11 / 0x05 (Location)
    value  = 142000   (142 km total travel this week)
```

---

## Flow 2: Outbound Assignment (Supervisor to Worker)

**Scenario:** NGO program manager assigns borehole inspections to a field worker.

**Step 1 — Manager creates outbound ASSIGN (on manager's ZAKO device):**
```
Records written on manager's device:
  ASSIGN (0x20)
  account_pair = 0001   (Parent→Child; outbound assignment)
  record_sep   = 1      (Outbound exchange)
  value        = 5
  dest_did     = [field worker's DID]
  note         = "Inspect boreholes BH-041–045, Choma. Due 2026-06-05."
```

**Step 2 — Assignment transmitted:**
```
telux-sharedb encodes the ASSIGN frame
Transmits via SMS (or IP if available)
```

**Step 3 — Worker's device receives assignment:**
```
telux-sharedb on worker's device receives frame
Validates manager's signature against manager's DID
Forwards to telux-ledgerd
Stored in worker's Exchange sub-entity (inbound from manager)
PADS app shows notification: "New assignment from [manager name]"
```

**Step 4 — Worker records completion (same as Flow 1, Steps 3–4)**

**Step 5 — Worker submits REPORT to manager:**
```
User action: Taps "Submit Report" → writes brief summary

Records written (compound group):
  REPORT (0x25)
  account_pair = 0000
  dest_did     = [manager's DID]
  note         = "BH-041: functional. BH-042: pump seal worn. BH-043: functional. BH-044: casing crack. BH-045: functional."
  
telux-sharedb transmits report to manager's device
```

---

## Flow 3: Invoice and Payment

**Scenario:** Field worker invoices the NGO for completed work.

**Step 1 — Worker generates invoice (after FINISH):**
```
User action: Taps "Invoice" on completed task → enters amount

Records written (compound group):
  INVOICE (0x1B)
  account_pair = 0110   (Receivable/Income)
  domain       = 00     (Financial)
  value        = 37500  (375.00 ZMW)
  dest_did     = [NGO DID]
  
  SEND (0x18) reference — links invoice to completed work:
  value = [FINISH record's frame_hash truncated to fit value field]

telux-sharedb transmits invoice via SMS to NGO
```

**Step 2 — NGO reviews and approves:**
```
NGO's device (ZAKO or Workpads) receives invoice
Reviews work reference
Approves → generates PAY + ACK records
Transmits back to worker's device via SMS
```

**Step 3 — Exchange Engine settles:**
```
Worker's Exchange Engine receives:
  ACK (0x1A) — invoice acknowledged
  PAY (0x1C) — value = 37500

Conservation check:
  INVOICE outflow (37500) + RECEIVE inflow (37500) = 0 ✓

Exchange Engine posts atomically:
  PAY record stored (inbound from NGO)
  RECEIVE (0x19) record stored (worker's inflow confirmation)
  
PADS Tasks View updates: "Settled — 375.00 ZMW received"
```

---

## Flow 4: Expense and Reimbursement

**Scenario:** Field worker buys fuel for site travel and claims reimbursement from NGO.

**Step 1 — Record expense:**
```
User action: Taps "Add Expense" → "Fuel" → enters amount

Records written (compound group):
  EXPENSE (0x26)
  account_pair = 0110   (Receivable/Income — reimbursable)
  domain       = 00     (Financial)
  value        = 4500   (45.00 ZMW)
  
  INVOICE reference (continuation):
  dest_did     = [NGO DID]
  value        = 4500   (same amount; conservation holds within compound group)
  note         = "Fuel for Choma site travel 2026-06-03"
```

**Step 2 — Transmit to NGO and receive reimbursement (same as Flow 3, Steps 2–3)**

---

## Flow 5: Offline Operation

**Scenario:** Field worker spends 3 days in a remote rural area with no connectivity.

**While offline:**
- All ASSIGN, START, FINISH, REPORT, EXPENSE records are written normally
- telux-sharedb queues outbound SEND records as Pending in telux-ledgerd
- telux-identd signs all records normally (identity lock only prevents signing while screen locked)
- Work Island accumulates records; Tasks View shows full work history
- Nothing is lost; nothing is delayed in terms of record creation

**When connectivity returns (arrives in town, registers on Airtel LTE):**
- telux-sharedb detects connectivity restoration
- Processes Pending SEND queue in order
- Manager receives reports for 3 days of work simultaneously
- Invoices transmit and await ACK + PAY
- Counterparties see the original wall_ts timestamps — they know when the work was done, not just when the device reconnected

**User sees:** "3 records transmitted" notification. Nothing to do — it all happened automatically.

---

## Work Island Lifecycle on Ya

A typical field worker on Ya will have:
- 1–3 active Work Islands (one per client or employer context)
- Each island accumulates records over the working relationship
- At end of contract: period Tally closes the island's final period; island archived

The 16-island limit is generous for the Ya use case. A field worker with 5 concurrent clients is unusual. Even at maximum, islands are archived (not deleted) — the historical record is always accessible in `telux-ledgerd`.
