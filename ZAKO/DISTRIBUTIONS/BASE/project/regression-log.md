# Regression Log — [DISTRO_NAME] on [DEVICE_NAME]

Records test results per build candidate. Each entry covers the full test-plan run for that candidate. Reference test-plan-template.md for test IDs.

---

## Log Entry Format

```
### Build Candidate — [DISTRO_NAME]-[VERSION]-[YYYYMMDD]

**Date tested:** YYYY-MM-DD
**Tester:** [name]
**Build type:** dev / beta / release-candidate
**Kernel:** [kernel version + commit hash]

P0 results:
- Boot: [Pass / Fail]
- Voice calls ([CARRIER]): [Pass / Fail]
- SMS: [Pass / Fail]
- USSD/STK: [Pass / Fail]
- ZAKO core: [Pass / Fail]

P1 results:
- WiFi: [Pass / Fail]
- Bluetooth: [Pass / Fail]
- GPS: [Pass / Fail / Not applicable]
- Audio: [Pass / Fail]
- Camera: [Pass / Fail / Not applicable]
- Power/battery: [Pass / Fail]
- De-Googling: [Pass / Fail]
- Security: [Pass / Fail]

New failures vs previous build:
- [list regressions, or "None"]

Issues opened this build:
- [ISSUE-NNN — brief description, or "None"]

Issues closed this build:
- [ISSUE-NNN — brief description, or "None"]

Notes:
- [any observations not captured above]
```

---

## Log

*No entries yet. First entry goes here.*
