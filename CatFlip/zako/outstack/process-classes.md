# Ya — Outstack Process Class Assignments

Complete `outstack-policy.xml` for the Ya distribution on Cat S22 Flip. This is the definitive list of what runs in which power mode.

Reference: `ZAKO/PACKAGES/zako-outstack.md` (process class schema), `CatFlip/zako/outstack/power-modes.md` (mode rationale)

---

## Process Class Quick Reference

| Class | Never gated | Gated in… |
|---|---|---|
| `system-critical` | Always runs | Never |
| `communication` | All modes | Emergency (radio-standby only) |
| `user-active` | Up to Standard | Critical, Emergency |
| `background` | Up to Standard | Conservation, Critical, Emergency |
| `deferred` | Full Power only | Standard, Conservation, Critical, Emergency |

---

## outstack-policy.xml — Ya

```xml
<outstack-policy version="1">

  <!-- ─────────────────────────────────────────────────────────
       SYSTEM CRITICAL — never gated
       These processes must survive to the last milliwatt.
       ───────────────────────────────────────────────────────── -->
  <process class="system-critical" kill_on_gate="false">

    <!-- Core Android services -->
    <entry type="service" name="servicemanager" />
    <entry type="service" name="hwservicemanager" />
    <entry type="service" name="vold" />
    <entry type="service" name="keystore" />
    <entry type="service" name="gatekeeperd" />

    <!-- Outstack itself -->
    <entry type="service" name="outstack-governed" />

    <!-- Telux daemon triad — exchange infrastructure; never gated -->
    <entry type="service" name="telux-ledgerd" />
    <entry type="service" name="telux-identd" />
    <entry type="service" name="telux-sharedb" />

    <!-- SIMBA governance daemon -->
    <entry type="service" name="simba-governed" />

    <!-- Radio interface layer — required for emergency calls -->
    <entry type="service" name="rild" />

    <!-- Network daemon — required for IP carrier layer -->
    <entry type="service" name="netd" />

    <!-- Qualcomm TrustZone / Keymaster -->
    <entry type="service" name="android.hardware.keymaster@4.0-service-qti" />
    <entry type="service" name="android.hardware.gatekeeper@1.0-service-qti" />

  </process>


  <!-- ─────────────────────────────────────────────────────────
       COMMUNICATION — gated in Emergency (radio-standby only)
       Telephony and Telux exchange must survive Critical Reserve.
       In Emergency: modem hardware continues paging (rild is system-critical);
       these higher-level services are suspended.
       ───────────────────────────────────────────────────────── -->
  <process class="communication" kill_on_gate="false">

    <!-- Android telephony stack -->
    <entry type="service" name="telecom" />
    <entry type="service" name="phone" />
    <entry type="app"     package="com.android.phone" />

    <!-- Telux ledger exchange services (informational; actual ledgerd is system-critical) -->
    <entry type="service" name="telux-ledger-exchange" />

    <!-- SMS and messaging -->
    <entry type="app"     package="com.android.messaging" />
    <!-- QKSMS (if installed as system app) -->
    <entry type="app"     package="org.qksms" />

    <!-- PADS work daemon — exchange records must complete in Critical -->
    <entry type="service" name="pads-workd" />

  </process>


  <!-- ─────────────────────────────────────────────────────────
       USER ACTIVE — gated in Critical and Emergency
       SurfaceFlinger and input must run when the user is present.
       In Critical Reserve: screen is dark most of the time;
       these are suspended until the user wakes the device.
       ───────────────────────────────────────────────────────── -->
  <process class="user-active" kill_on_gate="false">

    <!-- Core rendering and input -->
    <entry type="service" name="surfaceflinger" />
    <entry type="service" name="inputflinger" />
    <entry type="service" name="audioserver" />

    <!-- Launcher -->
    <entry type="app"     package="com.android.launcher3" />

    <!-- F-Droid foreground — if user has it open -->
    <entry type="app"     package="org.fdroid.fdroid" />

    <!-- Browser — if user has it open -->
    <entry type="app"     package="us.spotco.mulch" />    <!-- Mull -->

    <!-- OsmAnd — maps; user-active when opened -->
    <entry type="app"     package="net.osmand.plus" />

  </process>


  <!-- ─────────────────────────────────────────────────────────
       BACKGROUND — gated in Conservation, Critical, Emergency
       Non-critical services that can safely pause.
       kill_on_gate=false: suspend (SIGSTOP) so state is preserved.
       ───────────────────────────────────────────────────────── -->
  <process class="background" kill_on_gate="false">

    <!-- Package management (app install operations can pause) -->
    <entry type="service" name="installd" />

    <!-- Photo sync (ente.photos) -->
    <entry type="app"     package="io.ente.photos" />

    <!-- ntfy push relay client -->
    <entry type="app"     package="io.ente.auth" />

    <!-- DAVx5 calendar/contact sync -->
    <entry type="app"     package="at.bitfire.davdroid" />

    <!-- Nextcloud sync client (if installed) -->
    <entry type="app"     package="com.nextcloud.client" />

  </process>


  <!-- ─────────────────────────────────────────────────────────
       DEFERRED — gated in Standard and below
       Low-priority workers that should only run on a full battery.
       kill_on_gate=true: killed and restarted by init on ungate.
       These are idempotent background workers; killing is safe.
       ───────────────────────────────────────────────────────── -->
  <process class="deferred" kill_on_gate="true">

    <!-- Dex optimization — runs only on full power -->
    <entry type="service" name="BackgroundDexOptService" />

    <!-- Storage maintenance -->
    <entry type="service" name="StorageManagerService" />

    <!-- F-Droid background repository updates -->
    <entry type="service" name="org.fdroid.fdroid.updater" />

    <!-- OsmAnd map update checks -->
    <entry type="service" name="net.osmand.plus.download" />

  </process>

</outstack-policy>
```

---

## Notes on Ya-Specific Assignments

### PADS in `communication` class

`pads-workd` is assigned to `communication` class — it is gated only in Emergency (not in Critical). This is intentional: a field worker at 8% battery completing a payment or submitting a work record should be able to finish. PADS exchange records must settle even in constrained conditions. This mirrors the Exchange Engine's CRITICAL-priority invariant at the process level.

### `kill_on_gate` policy

All process classes use `kill_on_gate="false"` (SIGSTOP/SIGCONT) except `deferred`. Deferred workers are idempotent: BackgroundDexOptService can be killed mid-run and restarted cleanly; so can F-Droid repository updates. All other processes preserve state across suspension because the user's in-flight work or application state must not be lost.

### Future app additions

When new apps are added to the Ya preload set or installed via F-Droid, they should be assigned to `background` or `deferred` by adding entries here. Apps not listed in any class default to `user-active` behavior (they are not gated by Outstack) — which is wrong for background-only services. Keep this file current with the installed app set.
