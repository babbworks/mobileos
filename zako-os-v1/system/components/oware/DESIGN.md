# ZAKO OS — Oware Game: Design & Decision Record

**Component:** `zako-os-v1/system/components/oware/`
**Status:** Design (pre-implementation) — awaiting final spec approval
**Date:** 2026-06-20
**Phase context:** ZAKO Phase 6 (Applications) — native C, host-first
**Author:** Engineering (drafted with Claude)

---

## 1. Summary

A native-C implementation of **Oware** (the West African mancala game, Abapa
ruleset) for ZAKO OS. It ships as a terminal application on the host (Debian
x86_64) first, with an I/O-free rules + AI core designed to be re-skinned by
`libzako-ui` on the Cat S22 Flip later without changing a line of game logic.

The build embodies ZAKO's discipline: pure C99, MISRA-C subset, **no dynamic
allocation in steady state**, tiny fixed-size state, instant launch, and full
host-side unit testing before any device dependency.

### Goals
- Correct, complete Oware engine with **configurable rule variants**.
- Play **vs computer** (Easy / Medium / Hard) and **local 2-player** pass-and-play.
- All **four grand-slam variants** coded and selectable/pinnable.
- A **configurable capture rule** (standard "2 or 3"; "3/4" variant).
- Selectable **end condition** per match/series (first-to-N, play-till-all-captured, agreed ending).
- Persistent **± win/loss records**: vs-computer (resettable) and per-named-pair head-to-head.

### Non-goals (YAGNI for v1)
- On-device `libzako-ui` rendering (deferred; the render layer is intentionally throwaway).
- Networked/online play.
- Transposition tables / opening books (noted as future AI upgrades).
- A generic multi-mancala framework (over-engineering for "a simple game").
- Raw-terminal keypad capture (v1 uses line-based stdin for portability).

---

## 2. Game background & researched rules

Oware: 2 rows of **6 houses**, **4 seeds per house** at start (**48 seeds** total).
Players sow counter-clockwise and capture by landing in opponent houses.

Researched ruleset (Abapa variant), with sources:

- **Sowing.** Lift all seeds from a chosen own house; drop one per house
  counter-clockwise. Never sow into the score stores. If a house holds **≥12**
  seeds, the loop passes the **origin house, which is skipped** and left empty.
- **Capture.** If the **last** sown seed lands in an **opponent** house bringing
  it to exactly **2 or 3**, those seeds are captured; the capture **chains
  backward** through consecutive opponent houses also at 2 or 3, stopping at the
  first house that is the player's own or not at 2/3.
- **Grand slam.** A move capturing *all* of the opponent's seeds. Handling
  varies by locale — four documented variants (see §5.4).
- **Feeding obligation.** If the opponent has no seeds, the player **must** play
  a move that gives them at least one; if impossible, the game ends.
- **End of game.** First to **25** wins (majority of 48); **24–24 draw**. If a
  player cannot move, remaining seeds go to the side that owns them. An endless
  cycle is resolved by each player taking the seeds on their own side.

**Sources:**
- Oware — Mancala World (Fandom): https://mancala.fandom.com/wiki/Oware
- Grand Slam — Mancala World (Fandom): https://mancala.fandom.com/wiki/Grand_Slam
- Oware rules & strategy — Gambiter: https://gambiter.com/mancala/Oware.html
- Oware info — Awale-Awari: https://www.awale-awari.com/oware-info
- Oware variant — PlayStrategy: https://playstrategy.org/variant/oware
- (User-provided) Blanvillain Oware paper abstract — 48stones.com (not yet ingested; pending if needed for AI tuning)

---

## 3. Confirmed requirements (brainstorming decision log)

| # | Decision | Choice made | Source |
|---|----------|-------------|--------|
| R1 | Front-end for v1 | **Terminal C app** (numeric board, keys 1–6, line stdin) | user |
| R2 | Board rendering style | **Numeric** (one digit per house) — most ZAKO-minimal | user (visual pick "A") |
| R3 | Game modes | **Vs-computer + local 2-player** pass-and-play | user |
| R4 | AI | **Three difficulty levels** on one shared search engine | user |
| R5 | Grand-slam variants | **All four coded**, selectable and **pinned** (persisted) | user |
| R6 | Capture rule | Standard {2,3} **plus a "3/4" variant** — configurable | user (§Open Q1) |
| R7 | End condition | Selectable per match/series: **first-to-N (default 25)**, **play-till-all-captured**, **agreed ending** | user |
| R8 | Records — vs CPU | Running **± win/loss/draw**, with **reset** | user |
| R9 | Records — 2-player | **Named players**, **per-pair head-to-head ±** maintained across games | user |
| R10 | Architecture | **Approach 1 — layered modules** | user |
| R11 | Default grand-slam | **International (legal, no capture)** | user |

---

## 4. Architecture

### 4.1 Chosen approach — layered modules (Approach 1)

Four independently-testable units mirroring the existing ZAKO component pattern
(`<name>_*.c/.h` + `test_*.c` + `Makefile`):

| Unit | Responsibility | Depends on |
|------|----------------|------------|
| `oware_engine.[ch]` | Pure rules: state, move-gen, sowing, chained capture, capture-rule + grand-slam variants, feeding obligation, end/cycle detection. **Zero I/O.** | — |
| `oware_ai.[ch]` | Alpha-beta minimax + positional eval + 3 difficulty presets | engine |
| `oware_store.[ch]` | Persistence: pinned settings, vs-CPU ± record, per-pair head-to-head. Flat text file. | — (libc stdio) |
| `oware_ui.[ch]` + `main.c` | Terminal render, input, menus, match/series flow | all three |

**Why:** the I/O-free engine boundary means the same `oware_engine.c` driving
the terminal today is driven by `libzako-ui` on-device tomorrow, unchanged. Each
file is small enough to reason about in full. The engine is pure integer math on
a 16-byte struct — trivially unit-tested and MISRA-friendly.

### 4.2 Alternatives considered & rejected

- **Approach 2 — fewer translation units (engine+AI+UI merged).** Rejected:
  blurs the rules/render boundary, makes the engine harder to test in isolation
  and harder to reuse for the device port. Saves little.
- **Approach 3 — generic mancala framework, Oware as data.** Rejected as YAGNI:
  the request is one specific game; a configurable rules engine (variants below)
  already covers the realistic variation without a full abstraction layer.

---

## 5. Engine design (`oware_engine`)

### 5.1 State

```c
typedef struct {
    uint8_t  houses[12];        /* 0–5 = South (player 0), 6–11 = North (player 1) */
    uint8_t  score[2];          /* captured seeds per player */
    uint8_t  turn;              /* 0 or 1 — side to move */
    uint16_t no_capture_plies;  /* consecutive plies without a capture (cycle detection) */
} oware_state_t;                /* ~16 bytes, no heap */
```

- **One 12-cell ring**, sowing order `0→1→…→11→0`. Player 0 owns `0–5`,
  player 1 owns `6–11`. Counter-clockwise sowing is `i = (i + 1) % 12`.
- "Opponent side" of player `p` is the other six indices. A house belongs to
  player `p` iff `(index < 6) == (p == 0)`.
- Modular arithmetic collapses all wrap-around and side-crossing into one place,
  minimizing edge cases.

### 5.2 Rule configuration

Variants travel in a small config struct, separate from board state:

```c
typedef enum { OWARE_CAP_STANDARD, OWARE_CAP_THREE_FOUR } oware_capture_rule_t;

typedef enum {
    OWARE_GS_NO_CAPTURE,    /* legal move, captures nothing (default, international) */
    OWARE_GS_FORBIDDEN,     /* move is removed from the legal set */
    OWARE_GS_OPPONENT_KEEPS,/* player takes the slam; remaining board seeds sweep to opponent */
    OWARE_GS_LEAVE_LAST     /* capture chain except its furthest-back house */
} oware_grandslam_rule_t;

typedef enum {
    OWARE_END_FIRST_TO_N,   /* game ends the instant a side reaches target_score */
    OWARE_END_ALL_CAPTURED  /* no early stop; play until all seeds captured/terminal */
} oware_end_mode_t;

typedef struct {
    oware_capture_rule_t   capture_rule;     /* default OWARE_CAP_STANDARD */
    oware_grandslam_rule_t grandslam_rule;   /* default OWARE_GS_NO_CAPTURE */
    oware_end_mode_t       end_mode;         /* default OWARE_END_FIRST_TO_N */
    uint8_t                target_score;     /* default 25; used by FIRST_TO_N */
    uint16_t               cycle_ply_limit;  /* default 100; 0 disables */
    bool                   allow_agreed_end; /* manual "each takes own side"; default true for 2P */
} oware_rules_t;
```

The capturable-count test is a single predicate driven by `capture_rule`:
`STANDARD → count ∈ {2,3}`, `THREE_FOUR → count ∈ {3,4}` (pending confirmation,
§Open Q1). Making it a predicate means any future preset is one line.

### 5.3 Sowing & move generation

- **Move** = an own, non-empty house index (≤6 candidates).
- **Sow:** copy `seeds = houses[src]`, zero `src`, then `seeds` times do
  `i = (i+1) % 12; if (i == src) i = (i+1) % 12;` and `houses[i]++`. Record the
  landing index.
- **Legal moves** respect the **feeding obligation**: if the opponent currently
  has zero seeds, only moves whose sowing deposits ≥1 seed on the opponent side
  are legal. Under `OWARE_GS_FORBIDDEN`, moves that would be a grand slam are
  also excluded. Move generation simulates on a copy to evaluate these.

### 5.4 Capture (chained) & grand-slam handling

Capture is computed **provisionally** (without mutating) so a grand slam can be
detected before commitment:

1. From the landing index, walk backward while the house is on the **opponent**
   side and its post-sow count satisfies the capture predicate; collect those
   indices and their seed totals.
2. Determine `is_grand_slam` = the provisional capture empties the opponent's
   entire side.
3. Apply per `grandslam_rule`:
   - **`GS_NO_CAPTURE`** (default): keep seeds sown, capture nothing.
   - **`GS_FORBIDDEN`**: never reached here (filtered at move-gen).
   - **`GS_OPPONENT_KEEPS`**: credit the captured seeds to the mover, then sweep
     all other board seeds into the opponent's score (board empties → game ends).
   - **`GS_LEAVE_LAST`**: capture all chained houses **except the furthest-back
     one** (the last in the backward walk), leaving its seeds on the board.
4. If not a grand slam, capture normally and credit the mover.

A successful capture resets `no_capture_plies`; otherwise it increments.

### 5.5 End-of-game resolution

Checked after each applied move and at the start of each turn:

1. **First-to-N** (`OWARE_END_FIRST_TO_N`): if any `score ≥ target_score`, game
   ends immediately; that player wins (or draw if both somehow equal at target).
2. **All-captured** (`OWARE_END_ALL_CAPTURED`): no early stop on score; continue
   until the board is empty or a terminal condition below; winner = more seeds.
3. **No legal move** for the side to play → game ends; **every remaining seed is
   credited to the owner of the side it sits on**. (Cleanly covers both the
   "cannot feed" and "empty side" cases.)
4. **Cycle**: if `cycle_ply_limit > 0` and `no_capture_plies ≥ cycle_ply_limit`,
   declare a cycle → each player takes the seeds on their own side.
5. **Agreed ending** (`allow_agreed_end`): a UI-initiated action (both 2-players
   agree, or offered vs-CPU when a loop is suspected) applies the same own-side
   split as the cycle rule.

Final result is `{winner ∈ {0,1,draw}, score[0], score[1]}`.

### 5.6 Public API (sketch)

```c
void  oware_init(oware_state_t *s);
int   oware_legal_moves(const oware_state_t *s, const oware_rules_t *r, uint8_t out[6]);
bool  oware_apply_move(oware_state_t *s, const oware_rules_t *r, uint8_t house, oware_move_result_t *res);
bool  oware_is_over(const oware_state_t *s, const oware_rules_t *r, oware_result_t *res);
void  oware_resolve_agreed(oware_state_t *s, oware_result_t *res); /* own-side split */
```

`oware_move_result_t` reports landing index, captured count, grand-slam flag, and
whether a feeding move was forced — so the UI can narrate the turn.

---

## 6. AI design (`oware_ai`)

Alpha-beta minimax over the engine. Leaf **evaluation** from the mover's view:

| Term | Meaning | Intent |
|------|---------|--------|
| Material | `score[me] − score[opp]` (dominant weight) | win the game |
| Attack | opponent houses one short of a capture count | threats |
| Vulnerability | own houses one short of being captured (penalty) | safety |
| Hoarding ("kroo") | bonus for building one large house | classic Oware tempo weapon |
| Mobility | count of legal moves | avoid being stuck / starved |

- **Difficulty presets:**

  | Level | Search | Character |
  |-------|--------|-----------|
  | Easy | depth-1 (greedy) + random tie-break, occasional random move | beatable beginner |
  | Medium | alpha-beta depth ~5 | solid club player |
  | Hard | iterative-deepening alpha-beta ~depth 10, capture-first move ordering | punishing |

- **Terminal scoring** is depth-scaled so the AI prefers faster wins and slower
  losses.
- **Determinism:** randomness (Easy) uses a seeded PRNG so games are
  reproducible in tests.
- **Eval weights** live in named constants for easy tuning (the Blanvillain
  paper can inform these later).
- **Considered & deferred:** transposition table, endgame database, opening
  book — real strength gains but unnecessary for v1 and costly in code/RAM.

---

## 7. Persistence (`oware_store`)

Single human-readable, line-based file (easy to test and hand-inspect). Path is
env-overridable: `${ZAKO_OWARE_HOME}` else `~/.local/share/zako-oware/oware.dat`
(maps to `/data/zako/oware/` on device later).

```
# zako-oware store v1
variant=0            ; pinned grand-slam rule (0–3)
capture=0            ; pinned capture rule (0=standard, 1=three/four)
end=0 25             ; pinned end mode + target score
cpu easy   <w> <l> <d>
cpu medium <w> <l> <d>
cpu hard   <w> <l> <d>
pair "ABENA" "KOFI" <winsA> <winsB> <draws>
```

- **Pinned settings** (grand-slam, capture rule, end mode/target) survive launches.
- **Vs-CPU ±** kept per difficulty; **Records menu offers reset** (zeroes the cpu lines).
- **Head-to-head**: pair key normalized to a **stable sorted order** so a pair
  maps to one record regardless of who is "player 1". `winsA`/`winsB` track by
  the normalized name slots.
- **Fixed capacity** — e.g. **64 pairs**, **16-char** names — static arrays, no
  heap. Overflow and malformed lines are skipped gracefully; file rewritten
  cleanly on save.
- **Considered & rejected:** SQLite (overkill, adds dep — though `telux-ledgerd`
  uses it, a game's scoreboard doesn't warrant it); binary format (harder to
  test/inspect, no real size win at this scale).

---

## 8. Match / series configuration

A **Match** is one or more **Games** sharing one `oware_rules_t` plus a series
length, chosen at the outset:

- **Settings prompted at match start:** mode (vs-CPU+difficulty / 2-player+names),
  grand-slam variant, capture rule, end mode (first-to-N with N, or all-captured),
  agreed-ending allowed.
- **Series length (optional):** single game, **best-of-N**, or **first-to-K
  game-wins**. Default = single game.
- Per-game results roll into the persisted ± records (R8/R9). A series also shows
  an in-memory running tally for that sitting.
- Pinned settings (from Settings menu) pre-fill the match prompts.

---

## 9. UI / flow (`oware_ui` + `main`)

**Main menu:** ① Play vs Computer ② Two-Player ③ Records (view ± / reset)
④ Settings (pin grand-slam, capture rule, end mode; rules help) ⑤ Quit.

**Match setup:** difficulty/side or names → confirm rule set (pre-filled from
pinned settings) → series length.

**In-game screen:** numeric board (CPU/opponent top, you bottom), both scores,
turn indicator, selected-house highlight, and a message line narrating each turn
(captures, chained captures, illegal move, feeding forced, grand-slam handling,
game over). An "agree to end" action is available when enabled.

**Input:** line-based `stdin` — type a house **1–6** + Enter; letters for menu
navigation/back/quit. (Raw keypad/T9 deferred to the device port.)

**After each game:** update store, show the refreshed ± record / series tally.

---

## 10. File & build layout

```
components/oware/
  oware_engine.h   oware_engine.c
  oware_ai.h       oware_ai.c
  oware_store.h    oware_store.c
  oware_ui.h       oware_ui.c
  main.c
  test_oware_engine.c   test_oware_ai.c   test_oware_store.c
  Makefile
  README.md
  DESIGN.md   (this document)
```

**Makefile** mirrors `libzako-c0`:
- `CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -fstack-protector-strong`
- Debug/test build adds `-g -O0 -fsanitize=address,undefined`
- Targets: `all` (the `oware` binary), `test` (build + run all `test_*`), `clean`.

---

## 11. Test plan

**Engine** (`test_oware_engine.c`):
- Sowing: basic distribution; the **≥12 origin-skip** rule; wrap-around.
- Capture: simple 2/3; **chained** capture; chain stops at own side / non-2-3.
- Capture rule variant: standard {2,3} vs "3/4" preset.
- Grand slam: each of the **four** variants produces the specified board/score.
- Feeding obligation: illegal non-feeding move rejected when opponent empty.
- End: first-to-N stop; all-captured continuation; no-move own-side collection;
  cycle split at the ply limit; agreed-ending split; 24–24 draw.
- Property checks: seeds are conserved (sum of houses + scores == 48) after every
  move under every variant.

**AI** (`test_oware_ai.c`): greedy grabs an obvious capture; deeper search avoids
an obvious one-move blunder; AI never returns an illegal move; deterministic
output for a fixed seed; faster-win preference via depth-scaled terminals.

**Store** (`test_oware_store.c`): write/read round-trip; pinned-settings persist;
CPU record increment + reset; pair head-to-head accumulation with name-order
normalization; capacity overflow handled; malformed file tolerated.

All builds run clean under ASan + UBSan (matching the repo's existing standard).

---

## 12. ZAKO convention adherence

| ZAKO rule | How this design complies |
|-----------|--------------------------|
| C99, MISRA-C subset | Pure C99; predicates over enums; explicit types. |
| No dynamic allocation in steady state | 16-byte state; fixed arrays for moves, AI, store. `malloc` only (if at all) at store load init. |
| No recursion (statically bounded) | Minimax recursion is **depth-bounded** by the difficulty preset — stack depth statically determinable. (Documented exception to "no recursion"; alternative explicit stack noted if required.) |
| `default` on every switch | Variant/enums switched with explicit `default`. |
| Host-first, testable | Builds and fully tested on x86_64 before any device dependency. |
| Native, no JVM | Plain C terminal binary; render layer swappable for `libzako-ui`. |

---

## 13. Decisions weighed (at-a-glance)

| Topic | Options weighed | Chosen | Why |
|-------|-----------------|--------|-----|
| Board model | two 6-rows vs one 12-ring | **12-ring** | modular arithmetic, fewer edge cases |
| Front-end | terminal / SDL sim / WASM / core-only | **terminal** | zero-dep, runs now, device-portable core |
| Seed display | numeric / pips / hybrid | **numeric** | most ZAKO-minimal, fewest pixels |
| AI shape | levels / single knob / fixed / heuristic | **3 levels, shared engine** | best UX, modest code |
| Grand slam | 1 vs all 4 | **all 4, default international** | user requirement |
| Capture rule | fixed vs configurable | **configurable predicate** | supports "3/4" + future |
| End condition | fixed 25 vs selectable | **selectable per match** | user requirement |
| Persistence | text / binary / SQLite | **line-based text** | testable, inspectable, tiny |
| Cycle detection | position-hash vs ply counter | **ply counter** | cheap, fits device; hash deferred |
| Recursion | minimax recursion vs explicit stack | **bounded recursion** | simplest; depth-capped & documented |

---

## 14. Open questions

1. **"3/4 variant" semantics (Q1).** Confirm the capture trigger: is it **{3,4}**
   (capture on reaching 3 or 4) or **{2,3,4}** (extend standard with 4)? Default
   assumption in this doc: **{3,4}**. Trivial one-line change either way.
2. **Series scope.** Is best-of-N / first-to-K wanted in v1, or just single games
   with persisted ± records? Doc currently includes optional series; can be cut.
3. **Agreed-ending vs CPU.** Should the "agree to end" action be offered in
   vs-computer mode, or 2-player only? Doc default: 2-player on, CPU offers it
   only when a loop is detected.

---

## 15. Future work (device port)

- Implement `oware_ui` against `libzako-ui` once the Phase 6 rendering decision
  lands; reuse `oware_engine`/`oware_ai`/`oware_store` unchanged.
- T9/keypad raw input mapping (houses on keys 1–6; soft actions on others).
- Optional AI upgrades: transposition table, endgame database, opening book,
  eval tuning informed by the Blanvillain paper.
- Outstack power-class registration (a game is a foreground/INTERACTIVE app).
```
