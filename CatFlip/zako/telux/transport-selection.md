# Ya — Telux Transport Selection

How `telux-sharedb` selects a carrier for each outbound record on Ya, and how transport selection interacts with Outstack power modes.

Reference: `ZAKO/PROTOCOLS/Telux-Protocol-v1.md` §8.3, `ZAKO/PACKAGES/zako-telux.md`

---

## The Rule

Carriers are roads. The Telux protocol does not change per carrier. `telux-sharedb` selects the road based on:

1. What the receiving party's device can accept (their DID document service endpoints)
2. What carriers are currently available on the sending device
3. What the current Outstack mode permits

These three factors are evaluated in order. The result is the carrier used. The Sovereign can override.

---

## Decision Matrix

| Condition | IP (LTE/WiFi) | SMS | BLE | QR |
|---|---|---|---|---|
| Full Power, Standard | ✓ Preferred | ✓ Fallback | ✓ Short-range | ✓ Always |
| Conservation | ✓ If available | ✓ Preferred | ✓ Permitted | ✓ Always |
| Critical Reserve | ✓ If available | ✓ Preferred | ✓ Permitted | ✓ Always |
| Emergency | ✗ Suspended | ✓ Last resort | ✓ Permitted | ✓ Always |
| No carrier available | ✗ | ✗ | ✗ | ✓ Queue + QR |

In Emergency mode, active IP data connections are suspended (the `communication` class is partially gated). `telux-sharedb` routes all outbound records to SMS or BLE. QR is always available at zero radio cost.

---

## Outbound Queue Behavior

When no carrier is available (no SMS, no IP, no BLE counterparty nearby), `telux-sharedb` queues the outbound record as a SEND record with `status=Pending` in `telux-ledgerd`. The queue is:

- **Durable** — survives device restart (it's in the ledger)
- **Ordered** — processed FIFO when connectivity resumes
- **Bounded** — maximum 1024 pending records (configurable in `telux.conf`)

When connectivity resumes (Outstack mode rises, SIM registers, WiFi connects), `telux-sharedb` processes the queue in order. The counterparty receives the records with their original wall_ts timestamps — the delay is visible in the exchange history but the records themselves are unmodified.

---

## Carrier Availability Detection

`telux-sharedb` monitors carrier availability via:

| Carrier | Availability signal |
|---|---|
| IP | `ConnectivityManager.NetworkCallback` — fires on network connect/disconnect |
| SMS | RIL state — `SERVICE_STATE_IN_SERVICE` on the modem |
| BLE | BluetoothManager state — `STATE_ON` |
| QR | Always available (display-only; no radio) |

On Outstack mode change, Outstack signals `telux-sharedb` before any process class gating takes effect. `telux-sharedb` completes any in-flight frame transmission before acknowledging the mode change.

---

## Counterparty Capability Negotiation

Before transmitting, `telux-sharedb` reads the counterparty's DID document service endpoints to determine what they can receive. The DID document declares:

```json
{
  "service": [
    {
      "id": "#telux-sms",
      "type": "TeluxCarrierSMS",
      "serviceEndpoint": "+260971234567"
    },
    {
      "id": "#telux-ip",
      "type": "TeluxCarrierIP",
      "serviceEndpoint": "https://relay.babb.tel/did:key:z6Mk..."
    }
  ]
}
```

If the counterparty's DID document has no declared service endpoints, `telux-sharedb` defaults to SMS (using the MSISDN from the People Island contact record) and falls back to QR for out-of-band delivery.

---

## pads-v1 Compatibility and Transport

When sending a PADS record to a counterparty running a legacy Workpads client (not a full ZAKO device):

1. `telux-sharedb` encodes the BitPads frame as a pads-v1 URL (`#1pa/...`)
2. The URL is transmitted via SMS (primary for Zambia Workpads users) or IP
3. The counterparty's Workpads client decodes the URL and displays the work record

The transport is still SMS or IP — pads-v1 is an encoding format, not a transport.

```
ZAKO device                    Workpads client (non-ZAKO)
    │                                    │
    │  FINISH record → pads-v1 URL       │
    │  via SMS: "Send 15 kwacha           │
    │  for work done on 2026-06-02        │
    │  #1pa/[encoded frame]"             │
    │ ─────────────────────────────────► │
    │                                    │  Workpads decodes URL
    │  ◄──────────────────────────────── │  
    │  ACK record (BitPads frame)         │  sends ACK back
```

---

## Transport Cost Awareness on Ya

On a 1450mAh battery in Zambia, transport cost matters:

| Transport | Power cost | Data cost |
|---|---|---|
| QR (display only) | Negligible | None |
| BLE | Very low (~1mA during transfer) | None |
| SMS | Moderate (radio Tx burst) | ~1 Kwacha per SMS (carrier-dependent) |
| IP | Highest (sustained LTE radio) | Metered data |

For most Telux exchange on Ya — a PADS invoice, a SEND record, an ACK — a single SMS carries the entire frame. SMS is the appropriate default for this market and this battery.

The `ip_preferred_above = standard` setting in `telux.conf` means Ya uses IP when on Full Power or Standard mode and IP is available, falling to SMS otherwise. At Conservation and below, SMS becomes the de facto primary transport even when IP is technically available, because the power mode signals conserve radio usage.
