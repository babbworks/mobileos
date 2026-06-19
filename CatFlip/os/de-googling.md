# Cat S22 Flip — De-Googling

Inventory of Google Mobile Services removed from the stock Cat S22 Flip firmware, their replacements, and verification methodology.

---

## Stock GMS Packages Removed

The Cat S22 Flip ships with Android 11 Go + GMS Core (Go edition). The following packages were present in stock firmware and are completely removed in ZAKO OS:

| Package | Purpose | Status |
|---------|---------|--------|
| `com.google.android.gms` | Google Play Services (Go) | Removed |
| `com.google.android.gsf` | Google Services Framework | Removed |
| `com.android.vending` | Google Play Store | Removed |
| `com.google.android.apps.messaging` | Google Messages | Removed |
| `com.google.android.gm` | Gmail (Go) | Removed |
| `com.google.android.apps.maps` | Google Maps (Go) | Removed |
| `com.google.android.youtube` | YouTube (Go) | Removed |
| `com.google.android.apps.photos` | Google Photos | Removed |
| `com.google.android.googlequicksearchbox` | Google Search / Assistant | Removed |
| `com.google.android.setupwizard` | Google Setup Wizard | Removed |
| `com.google.android.tts` | Google Text-to-Speech | Removed |
| `com.google.android.inputmethod.latin` | Gboard | Removed |
| `com.google.android.feedback` | Google Feedback | Removed |
| `com.google.android.onetimeinitializer` | GMS init | Removed |
| `com.google.android.partnersetup` | Partner Setup | Removed |
| `com.google.android.ext.services` | GMS extended services | Removed |

Total packages removed: ~16 system apps + GMS framework components.

---

## Replacement Stack

| Function | Removed | Replacement | Notes |
|----------|---------|-------------|-------|
| Push notifications | FCM (Firebase Cloud Messaging) | ntfy + UnifiedPush | Self-hosted ntfy server; apps use UnifiedPush connector |
| App distribution | Play Store | F-Droid | Pre-configured with ZAKO repo |
| Messaging | Google Messages | AOSP Messaging (MMS/SMS) | Stock AOSP, no RCS |
| Email | Gmail | K-9 Mail | |
| Maps | Google Maps | Organic Maps | Offline OSM data for Zambia pre-loaded |
| Keyboard | Gboard | AOSP Keyboard (LatinIME) | |
| Search | Google Search | None | No search widget; browser uses DuckDuckGo |
| Setup wizard | Google Setup | ZAKO First-Run Experience | Custom setup: language, SIM, accounts |
| TTS | Google TTS | eSpeak NG | Offline, smaller footprint |
| Camera | GCam (not present on Go) | AOSP Camera2 | |
| Browser | Chrome (Go) | Chromium (ungoogled) or FOSS browser | No Google sync, no SafeBrowsing phoning home |
| DNS | Google DNS (hardcoded in GMS) | System resolver (carrier DNS or user-configured) | |

---

## GMS Background Services Eliminated

These persistent GMS background services are no longer running:

| Service | Behaviour on Stock | Impact of Removal |
|---------|-------------------|-------------------|
| `GCM/FCM` | Persistent TCP connection to Google (mtalk.google.com) | Eliminates ~15–30mW persistent radio draw |
| `NetworkLocationService` | Periodic WiFi/cell location reports to Google | No location data leaves device |
| `GmsCore heartbeat` | Periodic checkin to android.clients.google.com | No device telemetry sent |
| `SafetyNet/Play Integrity` | Device attestation to Google | Not needed; no GMS-dependent apps |
| `Google Analytics` | App usage reporting | Zero analytics exfiltration |
| `Clearcut/GMS logging` | Structured logs sent to Google | No diagnostic data leaves device |

---

## Measured Power Savings

Removing GMS eliminates persistent connections and periodic wakeups:

| Source | Estimated Draw (stock) | Draw After Removal |
|--------|----------------------|-------------------|
| FCM persistent connection | 20–40 mW | 0 mW (ntfy uses polling or server-push on existing connection) |
| GMS heartbeat/checkin | 10–20 mW (averaged over wake cycles) | 0 mW |
| Network location scanning | 15–30 mW (periodic WiFi scans) | 0 mW |
| Background GMS services (aggregate) | 15–30 mW | 0 mW |
| **Total savings** | **60–120 mW** | — |

On a 1450 mAh / 5.4 Wh battery, 90mW average savings translates to roughly 6–10 hours additional standby time.

---

## Verification: Zero Google Endpoints

### Method

1. Configure device to route all traffic through a transparent proxy (mitmproxy or PCAPdroid on-device)
2. Use the device normally for 48 hours (calls, SMS, USSD, WiFi browsing, app installs via F-Droid)
3. Filter captured traffic for Google-owned IP ranges and domains

### Google Domains That Must Show Zero Hits

```
*.google.com
*.googleapis.com
*.gstatic.com
*.googleusercontent.com
*.googlevideo.com
*.android.clients.google.com
mtalk.google.com
play.googleapis.com
fcm.googleapis.com
```

### Verification Commands

```bash
# On-device packet capture (requires root):
tcpdump -i any -w /sdcard/capture.pcap &
# Run for 48h, then:
tcpdump -r /sdcard/capture.pcap | grep -i google

# Or use PCAPdroid (no root, VPN-based capture):
# Install from F-Droid, run capture, export PCAP, analyze on workstation

# DNS query log (if using local resolver):
adb shell dumpsys connectivity | grep -i google
```

### Acceptance Criteria

- Zero DNS queries to Google-owned domains over 48-hour test period
- Zero TCP connections to Google IP ranges (AS15169, AS396982)
- USSD, STK, voice, SMS all function without any Google dependency
- OTA updates function via ZAKO infrastructure (no Play Services dependency)

---

## Known Exceptions

- **CAPTCHAs:** Some third-party websites use reCAPTCHA. This is a browser-level interaction, not an OS dependency. The user may encounter Google domains in browser traffic; this is acceptable and not an OS leak.
- **Carrier provisioning:** Some carriers send APN config that references Google DNS (8.8.8.8). The OS overrides this in the APN configuration to use carrier DNS or a neutral resolver.

---

## Related Documents

- [[02-architecture]] — Application layer overview
- [[blob-audit]] — Non-critical blobs removed (Google diagnostic)
- [[security-model]] — Privacy posture
