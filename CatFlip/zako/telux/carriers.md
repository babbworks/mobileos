# Ya — Telux Carrier Configuration

Carrier configs for the three Zambian carriers supported as Telux transport layers on Ya. Each carrier is a road — a surface over which BitPads frames travel. The Telux protocol does not change per carrier; only the transport characteristics differ.

Reference: `ZAKO/PROTOCOLS/Telux-Protocol-v1.md` §8, `ZAKO/PACKAGES/zako-telux.md`

---

## Carrier Landscape — Zambia

| Carrier | MCC/MNC | Market share | LTE bands | Mobile money | Notes |
|---|---|---|---|---|---|
| Airtel Zambia | 64504 | ~40% | 8, 3, 7 | Airtel Money (`*778#`) | Full band compatibility with Cat S22 |
| MTN Zambia | 64501 | ~42% | 3, 7 | MTN MoMo (`*303#`) | Largest network; no Band 8 (Cat S22 works on 3+7) |
| Zamtel | 64503 | ~18% | 40, 7 | Zamtel Kwacha (`*338#`) | State carrier; rural coverage |

All three carriers are SMS-capable, which is the primary Telux transport for Ya. Data connectivity is secondary and may not be available in rural Zambia.

---

## Telux Transport Priority on Ya

In order of preference, per Outstack mode:

| Mode | Primary | Secondary | Fallback |
|---|---|---|---|
| Full Power / Standard | IP (LTE/WiFi) | SMS | QR (out-of-band) |
| Conservation | IP (if available) | SMS | QR |
| Critical Reserve | SMS | IP | QR |
| Emergency | SMS | — | QR (no IP in Emergency) |

QR code transport is always available (no radio transmit cost, no carrier dependency) and is used for initial identity exchange and one-time record transfer when no carrier link is available.

---

## Carrier Config Files

### airtel-zm.conf

```ini
[carrier]
name              = Airtel Zambia
mcc_mnc           = 64504
slug              = airtel-zm

apn_name          = airtelweb
apn_type          = default,mms,supl
apn_protocol      = IPV4V6
apn_proxy         =
apn_mmsc          = http://mms.airtel.in/

sms_gateway       = direct
mobile_money_ussd = *778#
mobile_money_name = Airtel Money

volte_enabled     = false
rcs_enabled       = false
edrx_supported    = true
psm_supported     = false
```

### mtn-zm.conf

```ini
[carrier]
name              = MTN Zambia
mcc_mnc           = 64501
slug              = mtn-zm

apn_name          = internet
apn_type          = default,mms,supl
apn_protocol      = IPV4V6
apn_proxy         =
apn_mmsc          = http://10.10.10.9/

sms_gateway       = direct
mobile_money_ussd = *303#
mobile_money_name = MTN MoMo

volte_enabled     = false
rcs_enabled       = false
edrx_supported    = true
psm_supported     = false
```

### zamtel.conf

```ini
[carrier]
name              = Zamtel
mcc_mnc           = 64503
slug              = zamtel

apn_name          = zamtel
apn_type          = default
apn_protocol      = IPV4
apn_proxy         =
apn_mmsc          =

sms_gateway       = direct
mobile_money_ussd = *338#
mobile_money_name = Zamtel Kwacha

volte_enabled     = false
rcs_enabled       = false
edrx_supported    = false   # unconfirmed; conservative default
psm_supported     = false
```

---

## APN Configuration in AOSP

The carrier configs above are Telux-layer configs. Android's APN configuration is separate — it lives in `telephony/apns-conf.xml` in the device tree or in `PRODUCT_COPY_FILES`:

```xml
<!-- device/cat/S22FLIP/telephony/apns-conf.xml -->
<apns version="8">

  <!-- Airtel Zambia -->
  <apn carrier="Airtel Zambia"
       mcc="645"
       mnc="04"
       apn="airtelweb"
       type="default,mms,supl"
       protocol="IPV4V6" />

  <!-- MTN Zambia -->
  <apn carrier="MTN Zambia"
       mcc="645"
       mnc="01"
       apn="internet"
       type="default,mms,supl"
       protocol="IPV4V6"
       mmsc="http://10.10.10.9/"
       mmsproxy="10.10.10.9"
       mmsport="8080" />

  <!-- Zamtel -->
  <apn carrier="Zamtel"
       mcc="645"
       mnc="03"
       apn="zamtel"
       type="default"
       protocol="IPV4" />

</apns>
```

```makefile
# In device.mk:
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/telephony/apns-conf.xml:system/etc/apns-conf.xml
```

---

## USSD / STK — Mobile Money Integration

Mobile money in Zambia is conducted via USSD codes (`*778#`, `*303#`, `*338#`). These require STK (SIM Toolkit) and USSD session support in the telephony stack.

**GAP-001:** STK/USSD functionality has not been verified on Ya. This is a critical gap because mobile money is the primary financial mechanism for the Ya target users. If USSD sessions fail, the mobile money flows that PADS work records are designed to accompany will not complete on-device.

Testing checklist:
- [ ] Dial `*778#` on Airtel SIM — does Airtel Money menu appear?
- [ ] Dial `*303#` on MTN SIM — does MTN MoMo menu appear?
- [ ] Dial `*338#` on Zamtel SIM — does Zamtel Kwacha menu appear?
- [ ] Confirm USSD session stays open and navigable without RIL timeout

---

## VoLTE / IMS

VoLTE is listed as `volte_enabled = false` for all three Zambian carriers. This reflects the investigation status — it is not confirmed whether the Cat S22 Flip's CAF IMS stack will work on the Zambian network implementations.

For the Zambia v1 deployment, voice over GSM/WCDMA is the fallback. This is acceptable — all three carriers have 2G/3G GSM voice coverage even where LTE data is patchy.

See `CatFlip/project/gaps-and-blind-spots.md` GAP-003 for VoLTE investigation status.

---

## Carrier Testing on Canada Bell (Development)

Development of Ya is done in Canada on Bell network. Bell has different MCC/MNC (302 220 or 302 780) and different APNs. Bell is not in the carrier config set — it does not need to be. Ya is a Zambia distribution.

For testing Telux transport in Canada:
- SMS: works on Bell SIM with standard SMS SMSC routing
- IP: works on Bell LTE (requires appropriate APN — `pda.bell.ca` or `ltemobile.apn`)
- QR: works without any carrier

Telux protocol behavior is identical on Bell as on Zambian carriers. The carrier is transport. Test the protocol on Bell; test the carrier configs on Zambian SIMs.
