# Oware — Roadmap / Remaining Work

What is **done**: a complete, tested, playable terminal game — engine, AI
(Easy/Medium/Hard), persistence, and UI. See [`ARCHITECTURE.md`](ARCHITECTURE.md).

This document lists what could **still** be built. Nothing here is required for
the game to be played today; it is a menu of optional next steps, roughly ordered
by value.

---

## 1. Open question to confirm (cheap)

- **"3/4" capture rule semantics.** Currently `OWARE_CAP_THREE_FOUR` captures on a
  house reaching **{3, 4}**. If the intent was **{2, 3, 4}** (standard plus 4),
  it is a one-line change in `oware_capturable` (`oware_engine.c`) plus the
  matching test expectation. Confirm and adjust.

## 2. Gameplay features deferred from v1

- **Match / series (best-of-N or first-to-K).** The engine/store already support
  everything needed; this is a thin wrapper in `oware_ui` that loops games and
  tracks a per-sitting tally. (DESIGN §8/§10 sketched it; deferred as YAGNI.)
- **Expose more settings in the menu.** The store and engine already carry
  `capture_rule`, `end_mode`, and `target_score`; only the grand-slam variant is
  editable from the Settings menu today. Adding the others is the same pattern as
  the existing `ui_settings` handler.
- **Explicit "agree to end" action in-game.** Currently a game ends early only via
  `q` (concede / own-side split), EOF, or the cycle limit. A mutual "agree to
  end" menu action during 2-player play would match DESIGN §5.5.
- **Reset head-to-head records.** Records view resets vs-CPU stats; add an option
  to clear (or remove a single) pair record too.
- **Per-side difficulty / AI-vs-AI / choose your side vs CPU.** The driver already
  supports `side_is_ai[2]`; only the menu wiring is missing.

## 3. Player-experience polish

- **Richer board rendering.** Pips/hybrid styles were mocked during design; the
  numeric view shipped. A toggle could offer the dot view.
- **Turn narration.** Surface capture counts / chained captures / "fed opponent"
  messages (the engine already returns these in `oware_move_result_t`).
- **Undo last move** (within a game) and **save/resume** a game in progress.
- **Help / rules screen** in the menu.

## 4. On-device port (ZAKO Phase 6)

This is the main reason the engine/AI/store are I/O-free.

- **`libzako-ui` front-end.** Reimplement `oware_ui`'s render/input against the
  Phase-6 rendering toolkit once that decision lands (direct framebuffer /
  `ANativeWindow` / `NativeActivity`+EGL / lvgl). Engine/AI/store reused unchanged.
- **T9 / physical keypad input** mapping (houses on keys 1–6; soft actions on
  others) replacing line-based stdin.
- **Data directory** `/data/zako/oware/` (the store path is already env-overridable).
- **Outstack power-class registration** — register the game as a foreground /
  INTERACTIVE app so it is governed correctly.
- **Cross-compile for ARM32** and verify in the device image.

## 5. AI strength (optional)

- **Transposition table** (Zobrist hashing) — the biggest single strength/speed
  win; explicitly deferred from v1.
- **Opening book** and/or **endgame database** for near-perfect late play.
- **Evaluation tuning** — weights are named constants ready to tune; the
  Blanvillain Oware paper (user-provided) can inform this.
- **Position-hash repetition detection** to replace the pragmatic
  `no_capture_plies` cycle counter with true threefold-style detection.

## 6. Hardening (matches ZAKO quality gates)

- **Fuzz the store parser** (AFL++) — it reads an on-disk file and is the main
  untrusted-input surface.
- **Full `cppcheck --enable=all` / MISRA scan** across the component.
- **Cross-compile + run the suites on ARM32**, not just host x86_64.

---

*Branches `oware-engine`, `oware-store`, `oware-ui` remain at their merge points
for reference; all work is on `master` (pushed to `origin`).*
