# Ya — Telux Exchange Flow

End-to-end exchange flows on Ya: what happens when two Sovereigns exchange records, how conservation is enforced, and how exchange interacts with Outstack power modes.

Reference: `ZAKO/PROTOCOLS/Telux-Protocol-v1.md` §5–§6, `CatFlip/zako/pads/field-worker-flows.md`

---

## The Fundamental Flow

Every Telux exchange between two parties follows the same structure regardless of what is being exchanged:

```
Sovereign A                           Sovereign B
     │                                     │
     │  1. A creates SEND record           │
     │     signed by A's key               │
     │     → telux-ledgerd stores it       │
     │     → telux-sharedb queues it       │
     │                                     │
     │ ────────────────────────────────►   │
     │  2. Frame transmitted (SMS/IP/QR)   │
     │                                     │
     │                                     │  3. B's telux-sharedb receives frame
     │                                     │     validates A's signature
     │                                     │     forwards to telux-ledgerd
     │                                     │     ledger stores + chain-hashes
     │                                     │
     │   ◄────────────────────────────────  │
     │  4. B sends ACK record              │
     │                                     │
     │  5. A's Exchange Engine receives    │
     │     both legs (SEND + ACK)          │
     │     verifies conservation           │
     │     posts atomically                │
```

The Exchange Engine never posts a single leg. Both records are held pending until both arrive, then posted as an atomic unit. Conservation is enforced at posting time.

---

## Payment Exchange Flow

The standard PADS work payment scenario: field worker A (Sovereign A) submits a work record to client B (Sovereign B), invoices for payment, and B pays.

```
A (Field Worker)                       B (Client / Employer)
     │                                     │
     │  FINISH record (work complete)      │
     │  → stored in Work Island            │
     │                                     │
     │  INVOICE record (compound group):   │
     │    Record 1: INVOICE (0x1B)         │
     │    Record 2: SEND (0x18) [FINISH    │
     │               frame_hash reference] │
     │  → telux-ledgerd stores             │
     │  → telux-sharedb encodes as         │
     │    pads-v1 URL (if B is Workpads)   │
     │    or raw BitPads frame (if B ZAKO) │
     │                                     │
     │ ────────────────────────────────►   │
     │  Invoice transmitted via SMS        │
     │                                     │
     │                                     │  B receives invoice
     │                                     │  Reviews work record reference
     │                                     │  Approves payment
     │                                     │
     │   ◄────────────────────────────────  │
     │  ACK + PAY records arrive:          │
     │    ACK (0x1A): receipt confirmed    │
     │    PAY (0x1C): payment_amount       │
     │                                     │
     │  A's Exchange Engine:               │
     │    INVOICE amount == PAY amount?    │
     │    → Yes: conservation holds        │
     │    → Post INVOICE + RECEIVE atomic  │
     │                                     │
     │  RECEIVE record stored:             │
     │    (0x19) value=payment_amount      │
     │    source_did=B's DID               │
```

The conservation invariant: A's INVOICE (outflow) and A's RECEIVE (inflow) sum to zero within the Exchange Engine batch. A partial PAY creates a pending receivable balance.

---

## Cross-Device Exchange: ZAKO to ZAKO

When both parties run Ya (or any ZAKO distribution):

- Frames are raw BitPads format
- Transport: IP preferred (full frame, fast), SMS fallback (segmented if needed), QR for out-of-band
- DIDs are exchanged at first connection via QR code scan (identity bootstrap)
- After first exchange, DID is cached in B's People Island

```bash
# Identity bootstrap — QR scan:
# A displays their DID document as a QR code:
telux-ctl identity qr

# B scans it with the camera:
# telux-identd on B receives A's DID, validates, stores in People Island
# Exchange is now ready: A and B have each other's DID and service endpoints
```

---

## Cross-Device Exchange: ZAKO to Workpads

When A runs Ya and B runs a legacy Workpads client (iOS/Android with pads-v1 support):

- A's `telux-sharedb` detects B's DID document lacks a ZAKO BitPads endpoint
- Falls back to pads-v1 URL encoding
- Transmits via SMS (most reliable for Zambia Workpads users)
- B's Workpads client decodes the pads-v1 URL and displays the record
- B's ACK travels back as a pads-v1 URL via SMS
- A's `telux-sharedb` decodes the inbound pads-v1 URL into a BitPads ACK frame
- Exchange Engine receives both legs; conservation check; atomic post

The protocol is identical. Only the wire encoding differs.

---

## Outstack Interaction

Exchange records are CRITICAL priority and run in all Outstack modes. What changes per mode is transport availability:

**Full Power / Standard:** IP + SMS available. Exchanges settle quickly.

**Conservation:** IP may be available (if LTE is active). SMS always available. `pads-workd` is in the `communication` class and runs through Conservation — field workers can complete invoices at 25% battery.

**Critical Reserve:** SMS primary. IP suspended for data. Exchange Engine continues to run. An in-flight invoice that arrived at 8% battery completes.

**Emergency:** `communication` class partially gated — active data connections suspended. But a SEND record that was already transmitted and is awaiting ACK will post when the ACK arrives, even in Emergency. The Exchange Engine is CRITICAL; it does not stop.

**No connectivity:** Records queue in `telux-ledgerd` as Pending SEND records. They transmit when connectivity resumes. This is offline-first behavior — the user can create and store exchange records without any carrier present.

---

## Newgroup Exchange (Future / v2)

Ya v1 supports bilateral exchange only (two Sovereigns). Newgroup exchange (multiple Sovereigns sharing a common ledger context — a community work group, a shared payment pool) is designed in the Telux Protocol but not activated in Ya v1.

When newgroup support is added in a future release:
- The founding Sovereign creates a CREATE record (0x0C) establishing the newgroup
- Participants join via AGREE records (0x1E)
- The hosted Ledger Node at `babb.tel` provides total ordering for the newgroup

This is a v2 feature. Track it in `CatFlip/project/known-issues.md` when development begins.
