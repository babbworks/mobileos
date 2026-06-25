# Oware — Technical Architecture (as built)

**Component:** `zako-os-v1/system/components/oware/`
**Status:** Complete & playable (engine + AI + store + UI), merged to `master`.
**Last updated:** 2026-06-25

This is the **as-built** technical summary of the finished game. For the original
design rationale and decision record see [`DESIGN.md`](DESIGN.md); for the
task-by-task build steps see `PLAN-01-engine.md` … `PLAN-04-ui.md`; for what is
left to build see [`ROADMAP.md`](ROADMAP.md).

---

## 1. What it is

A complete native-C implementation of **Oware** (West African mancala, Abapa
ruleset) for ZAKO OS: a terminal game you can play **against the computer**
(Easy / Medium / Hard) or **local two-player**, with configurable rule variants
and persistent win/loss records. It is built host-first on Linux; the rules and
AI are an I/O-free core designed to be re-skinned by `libzako-ui` on the Cat S22
Flip later without changing game logic.

**Verification:** 71,085 test checks pass (64021 engine + 6920 AI + 117 store +
27 UI), clean under AddressSanitizer + UndefinedBehaviorSanitizer, release build
warning-free at `-std=c99 -Wall -Wextra -Wpedantic -Werror`.

---

## 2. Layered architecture

```
            +-------------------+
   main.c   |  stdio I/O glue   |   (the only file that touches stdin/stdout)
            +---------+---------+
                      | oware_io_t (read_line / write_str callbacks)
            +---------v---------+
  oware_ui  |  render · input · |   menus, match loop, board rendering
            |  game driver loop |
            +----+----+----+----+
                 |    |    |
        +--------+    |    +-----------+
        v             v                v
  oware_ai       oware_engine     oware_store
  (search+eval)  (pure rules)     (persistence)
        |             ^                 (independent)
        +------>------+
```

- **`oware_engine`** — pure rules. No I/O, no allocation, ~16-byte state.
- **`oware_ai`** — negamax search built entirely on the engine's public API.
- **`oware_store`** — flat-file persistence; depends only on libc + the enums.
- **`oware_ui`** — presentation + flow, all interaction via an injected
  `oware_io_t` so the whole loop is unit-testable without a terminal.
- **`main.c`** — ~40 lines wiring real stdio into `oware_io_t`, plus store
  load/save. The only throwaway layer for the eventual device port.

Build artifacts: `liboware-engine.a`, `liboware-ai.a`, `liboware-store.a`,
`liboware-ui.a`, and the `oware` binary.

---

## 3. Engine (`oware_engine.[ch]`)

**State** — one 12-cell ring; sowing is `i = (i+1) % 12`:
```c
typedef struct {
    uint8_t  houses[12];        /* 0-5 = player 0 (South), 6-11 = player 1 */
    uint8_t  score[2];
    uint8_t  turn;              /* 0 or 1 */
    uint16_t no_capture_plies;  /* cycle detection */
} oware_state_t;                /* ~16 bytes, stack-only */
```

**Rule configuration** (separate from state) carries the variants:
- `capture_rule`: `OWARE_CAP_STANDARD` {2,3} | `OWARE_CAP_THREE_FOUR` {3,4}
- `grandslam_rule`: `NO_CAPTURE` (default) | `FORBIDDEN` | `OPPONENT_KEEPS` | `LEAVE_LAST`
- `end_mode`: `FIRST_TO_N` (default, `target_score` 25) | `ALL_CAPTURED`
- `cycle_ply_limit` (default 100), `allow_agreed_end`

**Core mechanics** (all in one `oware_simulate` helper, reused by move-gen and
apply):
- Sowing lifts a house, drops counter-clockwise, **skips the origin** on a ≥12
  wrap (seeds conserved).
- Capture: last seed landing in an opponent house at the capturable count
  captures it and **chains backward** over opponent houses until a non-capturable
  or own house. Computed provisionally so a grand slam is detected before mutation.
- Grand slam (would empty the opponent's whole side) dispatched by the four rules.
- Feeding obligation: when the opponent is empty, only moves that leave them ≥1
  seed are legal.
- End: first-to-N / board-empty / no-legal-move (each side keeps its own seeds) /
  cycle split / agreed-end split. Outcome P0 / P1 / DRAW.

**Public API:** `oware_rules_default`, `oware_init`, `oware_legal_moves`,
`oware_is_legal`, `oware_apply_move`, `oware_is_over`, `oware_resolve_agreed`,
plus `oware_house_belongs_to` / `oware_side_seeds` / `oware_board_seeds`.

**Invariant tested across 200 random games:** `sum(houses)+score[0]+score[1] == 48`.

---

## 4. AI (`oware_ai.[ch]`)

Negamax with alpha-beta over the engine. Leaf **evaluation** (mover's view,
named weight constants): material `(score[me]-score[opp])·100`, capture threats
(+8), own vulnerability (−8), hoarding/"kroo" (+3 for the largest own house),
mobility (+2/move). Terminal scores are depth-scaled to prefer faster wins.

| Difficulty | Search | Notes |
|-----------|--------|-------|
| Easy | depth 1 (greedy) + 20% random move | seeded LCG → deterministic/reproducible |
| Medium | alpha-beta depth 5 | |
| Hard | iterative-deepening to depth 10, capture-first move ordering | |

**API:** `oware_ai_config_default(cfg, difficulty)`, `oware_ai_choose_move(...)`.
The AI only ever selects from the engine's legal set, so it honours every rule
variant (proven legal under all four grand-slam rules + the 3/4 capture rule).

---

## 5. Store (`oware_store.[ch]`)

Fixed-size struct, no heap: pinned settings + `cpu[3]` records (per difficulty) +
`pairs[64]` head-to-head records + `pair_count`. Tolerant, human-readable
line-based file at `$ZAKO_OWARE_HOME/oware.dat` (default
`$HOME/.local/share/zako-oware/oware.dat`):

```
# zako-oware store v1
variant=0
capture=0
end=0 25
cpu easy 0 0 1
cpu medium 0 0 0
cpu hard 0 0 0
pair ABENA KOFI 0 1 0
```

Names are sanitized (uppercase alnum, ≤15 chars) and pair keys normalized to a
stable order (`strcmp(a,b) <= 0`) so lookups are order-independent. Missing file →
defaults; malformed lines skipped. **API:** `init`, `default_path`, `load`,
`save`, `record_cpu`, `reset_cpu`, `pair`, `record_pair`.

---

## 6. UI (`oware_ui.[ch]`) + `main.c`

All I/O goes through an injected seam:
```c
typedef struct oware_io {
    bool (*read_line)(struct oware_io *io, char *buf, size_t cap);
    void (*write_str)(struct oware_io *io, const char *s);
    void *ctx;
} oware_io_t;
```
This makes the **entire menu + game loop unit-testable** with scripted input
(`test_oware_ui.c` drives full games/menus and asserts on captured output and
store mutations). `main.c` provides the stdio-backed implementation.

- **Board:** numeric render, opponent row + your row, key legend 1–6, both scores.
- **Input:** keys 1–6 → ring houses (`base + key-1`); `q` concedes (agreed-end split).
- **Menu:** ① vs Computer (pick difficulty) ② Two-Player (names) ③ Records (view + reset) ④ Settings (pin grand-slam variant) ⑤ Quit.
- **Flow:** results recorded to the store (vs-CPU bucket / pair head-to-head from
  the correct side's perspective) and saved after each game / settings change.

---

## 7. Build & conventions

```bash
make            # libs + the `oware` binary (release, -O2 -Werror)
make test       # all four suites under ASan + UBSan
./oware         # play
```

ZAKO house style throughout: C99 / MISRA-leaning — no dynamic allocation in
steady state, bounded loops and buffers, explicit casts across signedness/width,
`default` on every `switch`, `const`-correct read-only pointers, header guards.
Tests use a shared single-header `oware_test.h` (`CHECK` / `TEST_REPORT`).

---

## 8. As-built deviations from `DESIGN.md`

| Topic | Design sketch | As built | Why |
|-------|---------------|----------|-----|
| "3/4" capture rule | unspecified exact set | **{3,4}** | open question; one-line change if `{2,3,4}` intended |
| Pair names in file | quoted (`"ABENA"`) | unquoted uppercase token | simpler/robust `sscanf` parsing |
| String copies | `strncpy` | `snprintf` | avoids `-Wstringop-truncation` under `-O2 -Werror` |
| Match/series | optional best-of-N | single games; records are the running tally | YAGNI for v1 (see ROADMAP) |
| Settings menu | pin all rules | pins grand-slam variant (others applied from store, not yet menu-editable) | headline configurable first |

All deviations are non-behavioral except the explicit 3/4 open question.
