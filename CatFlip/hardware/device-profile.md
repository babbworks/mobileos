# Cat S22 Flip — Device Profile

Hardware reference for the Cat S22 Flip (Bullitt Group) as used in the ZAKO OS project.

---

## SoC: Qualcomm QM215 (MSM8937 Platform)

| Parameter | Value |
|-----------|-------|
| CPU | 4× ARM Cortex-A53 @ 1.4 GHz |
| Architecture | ARMv8-A (running in 32-bit mode) |
| GPU | Adreno 308 (OpenGL ES 3.0) |
| DSP | Hexagon QDSP6 (audio/sensor processing) |
| Modem | Snapdragon X5 LTE (Cat 4 DL / Cat 5 UL) |
| Process node | 28nm LP |
| ISP | Qualcomm Spectra (single 8MP) |

---

## Form Factor

- **Type:** Clamshell flip phone with physical hinge
- **Main display:** 4.0" TN LCD, 480×800, capacitive touch
- **Cover display:** 1.44" TN LCD, 128×128, non-touch (notifications, clock, caller ID)
- **Hinge sensor:** Hall effect magnetic switch → `SW_LID` input event
- **Physical keypad:** T9-style alphanumeric keypad below main display (hardware scan codes via gpio-keys)
- **Dimensions:** ~120 × 60 × 22mm (closed)
- **Weight:** ~164g

---

## Sensors

| Sensor | Type | Driver/Interface |
|--------|------|------------------|
| Hall effect (lid) | Digital switch | `SW_LID` via gpio-keys; 0=open, 1=closed |
| Proximity | IR reflective | STK3x1x or equivalent; `/sys/class/sensors/proximity` |
| Accelerometer | 3-axis MEMS | Bosch BMA2x2 or ST LIS2DH; `/sys/class/sensors/accelerometer` |
| Light sensor | ALS (ambient) | Combined with proximity IC |

No gyroscope, no magnetometer, no barometer. Sensor HAL is `android.hardware.sensors@1.0-impl`.

---

## Ruggedization

| Standard | Rating | Implication |
|----------|--------|-------------|
| IP68 | Dust-tight, submersion to 1.35m/35min | Sealed chassis, no exposed ports during immersion |
| MIL-STD-810H | Drop (1.8m onto steel), vibration, thermal shock | Rubberized corners, reinforced hinge, Gorilla Glass 5 |

Operating temperature range: -20°C to +55°C. Relevant for Zambian deployment (ambient up to 42°C in Luapula/Bangweulu region).

---

## Radio Bands

### LTE (FDD)

| Band | Frequency | Zambian Carrier |
|------|-----------|-----------------|
| B1 | 2100 MHz | MTN Zambia |
| B3 | 1800 MHz | Airtel Zambia, Zamtel |
| B7 | 2600 MHz | MTN Zambia |
| B8 | 900 MHz | Airtel Zambia |
| B28 | 700 MHz | Zamtel (rural) |

### 3G (WCDMA)

| Band | Frequency |
|------|-----------|
| B1 | 2100 MHz |
| B5 | 850 MHz |
| B8 | 900 MHz |

### 2G (GSM)

| Band | Frequency |
|------|-----------|
| GSM 900 | 900 MHz |
| DCS 1800 | 1800 MHz |

All three Zambian carriers (MTN, Airtel, Zamtel) are covered. VoLTE depends on carrier provisioning; CSFB (2G/3G voice fallback) is the guaranteed baseline.

---

## Memory & Storage

| Parameter | Value |
|-----------|-------|
| RAM | 2 GB LPDDR3 |
| Storage | 16 GB eMMC 5.1 |
| External | microSD slot (up to 128 GB, FAT32/exFAT) |

Usable system storage after OS + vendor: ~6–8 GB for userdata partition.

---

## Battery

| Parameter | Value |
|-----------|-------|
| Capacity | 1450 mAh (5.4 Wh) |
| Chemistry | Li-Ion (non-removable) |
| Charging | Micro-USB, 5V/1A (5W max) |
| Voltage | 3.7V nominal, 4.35V max |

Power budget is the primary design constraint. See [[Outstack-Protocol-v1]] for power governance. Target idle power floor: <50mW (deep idle), <100mW (LTE standby with eDRX).

---

## Connectivity

| Interface | Spec |
|-----------|------|
| WiFi | 802.11 b/g/n (2.4 GHz only), WCNSS |
| Bluetooth | 4.2 LE |
| USB | Micro-USB 2.0 (charging + ADB) |
| SIM | Nano-SIM (single slot) |
| NFC | None |

---

## Audio

| Component | Detail |
|-----------|--------|
| Codec | Qualcomm WCD9326 (or equivalent MSM8937 internal) |
| Speaker | Single mono, bottom-firing |
| Earpiece | Standard telephony earpiece |
| Mic | Single DMIC (digital MEMS) |
| FM Radio | WCNSS FM receiver (requires wired headphone as antenna) |
| HAL | `android.hardware.audio@6.0-impl` (Qualcomm LPASS) |

---

## Camera

| Position | Sensor | Resolution |
|----------|--------|------------|
| Rear | Single, 5MP (autofocus) | 2592×1944 |
| Front | None | — |

Camera HAL: QCamera3 (`libmmcamera*` blobs). Not a priority feature for ZAKO v1.

---

## Related Documents

- [[02-architecture]] — Software stack and build approach
- [[partition-map]] — eMMC partition layout
- [[blob-audit]] — Vendor blob inventory
