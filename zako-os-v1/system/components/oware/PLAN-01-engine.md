# Oware Engine — Implementation Plan (Plan 1 of 4)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `oware_engine` — a pure, I/O-free C99 library implementing complete Oware (Abapa) rules with configurable capture/grand-slam/end variants, fully unit-tested on host.

**Architecture:** One 12-cell ring board in a ~16-byte struct; a single `oware_simulate()` core computes sowing + provisional capture, reused by move generation and move application. Rule variants travel in a separate config struct. No dynamic allocation; bounded loops only.

**Tech Stack:** C99, GNU Make, gcc/clang with `-Wall -Wextra -Wpedantic -Werror`, ASan+UBSan for the test build. Custom single-header assert macros (matches repo `test_*.c` convention).

This plan covers **only the engine** (`oware_engine.[ch]` + `test_oware_engine.c`). The AI, store, and UI are separate plans. See `DESIGN.md` in this directory for the full spec; section references below (§) point into it.

---

## File Structure

| File | Responsibility |
|------|----------------|
| `oware_engine.h` | Public types (`oware_state_t`, `oware_rules_t`, result structs, enums) + function declarations |
| `oware_engine.c` | All rules logic: init, simulate (sow+capture), legal moves, apply, end detection |
| `oware_test.h` | Tiny `CHECK`/`TEST_REPORT` assert macros shared by all test files |
| `test_oware_engine.c` | Engine unit tests; its own `main()` |
| `Makefile` | `all` (lib), `test` (build+run, ASan/UBSan), `clean` |

All paths below are relative to `zako-os-v1/system/components/oware/`.

---

### Task 1: Scaffold — header, test harness, Makefile, init + helpers

**Files:**
- Create: `oware_engine.h`
- Create: `oware_test.h`
- Create: `oware_engine.c`
- Create: `test_oware_engine.c`
- Create: `Makefile`

- [ ] **Step 1: Write the public header**

Create `oware_engine.h`:

```c
#ifndef OWARE_ENGINE_H
#define OWARE_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#define OWARE_HOUSES 12u
#define OWARE_SIDE    6u
#define OWARE_SEEDS  48u

typedef enum {
    OWARE_CAP_STANDARD   = 0, /* capture on house reaching 2 or 3 */
    OWARE_CAP_THREE_FOUR = 1  /* capture on house reaching 3 or 4 */
} oware_capture_rule_t;

typedef enum {
    OWARE_GS_NO_CAPTURE     = 0, /* legal move, captures nothing (default) */
    OWARE_GS_FORBIDDEN      = 1, /* move removed from legal set */
    OWARE_GS_OPPONENT_KEEPS = 2, /* slam captured; remaining board seeds to opponent */
    OWARE_GS_LEAVE_LAST     = 3  /* capture chain except its furthest-back house */
} oware_grandslam_rule_t;

typedef enum {
    OWARE_END_FIRST_TO_N  = 0, /* ends instant a side reaches target_score */
    OWARE_END_ALL_CAPTURED = 1 /* no early stop; play until board empty/terminal */
} oware_end_mode_t;

typedef struct {
    oware_capture_rule_t   capture_rule;
    oware_grandslam_rule_t grandslam_rule;
    oware_end_mode_t       end_mode;
    uint8_t                target_score;    /* used by FIRST_TO_N */
    uint16_t               cycle_ply_limit; /* 0 disables cycle detection */
    bool                   allow_agreed_end;
} oware_rules_t;

typedef struct {
    uint8_t  houses[OWARE_HOUSES]; /* 0-5 = player 0 (South), 6-11 = player 1 (North) */
    uint8_t  score[2];
    uint8_t  turn;                 /* 0 or 1 */
    uint16_t no_capture_plies;
} oware_state_t;

typedef struct {
    uint8_t landing;        /* index of last sown seed */
    uint8_t captured;       /* seeds captured this move */
    bool    was_grand_slam;
    bool    forced_feed;    /* move was made while opponent was empty */
} oware_move_result_t;

typedef enum {
    OWARE_OUT_NONE = 0,
    OWARE_OUT_P0   = 1,
    OWARE_OUT_P1   = 2,
    OWARE_OUT_DRAW = 3
} oware_outcome_t;

typedef struct {
    bool            over;
    oware_outcome_t outcome;
    uint8_t         score[2];
} oware_result_t;

void oware_rules_default(oware_rules_t *r);
void oware_init(oware_state_t *s);

bool oware_house_belongs_to(uint8_t house, uint8_t player);
int  oware_side_seeds(const oware_state_t *s, uint8_t player);
int  oware_board_seeds(const oware_state_t *s);

int  oware_legal_moves(const oware_state_t *s, const oware_rules_t *r,
                       uint8_t out[OWARE_SIDE]);
bool oware_is_legal(const oware_state_t *s, const oware_rules_t *r, uint8_t house);
bool oware_apply_move(oware_state_t *s, const oware_rules_t *r,
                      uint8_t house, oware_move_result_t *res);

bool oware_is_over(const oware_state_t *s, const oware_rules_t *r,
                   oware_result_t *res);
void oware_resolve_agreed(const oware_state_t *s, oware_result_t *res);

#endif /* OWARE_ENGINE_H */
```

- [ ] **Step 2: Write the shared test macros**

Create `oware_test.h`:

```c
#ifndef OWARE_TEST_H
#define OWARE_TEST_H
#include <stdio.h>

static int oware_tests_run = 0;
static int oware_tests_failed = 0;

#define CHECK(cond) do { \
    oware_tests_run++; \
    if (!(cond)) { \
        oware_tests_failed++; \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define TEST_REPORT() do { \
    printf("%d/%d checks passed\n", \
           oware_tests_run - oware_tests_failed, oware_tests_run); \
    return oware_tests_failed == 0 ? 0 : 1; \
} while (0)

#endif /* OWARE_TEST_H */
```

- [ ] **Step 3: Write the failing test for init + helpers**

Create `test_oware_engine.c`:

```c
#include "oware_engine.h"
#include "oware_test.h"

static void test_init(void) {
    oware_state_t s;
    oware_init(&s);
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) {
        CHECK(s.houses[i] == 4u);
    }
    CHECK(s.score[0] == 0u);
    CHECK(s.score[1] == 0u);
    CHECK(s.turn == 0u);
    CHECK(s.no_capture_plies == 0u);
    CHECK(oware_board_seeds(&s) == 48);
    CHECK(oware_side_seeds(&s, 0) == 24);
    CHECK(oware_side_seeds(&s, 1) == 24);
}

static void test_ownership(void) {
    CHECK(oware_house_belongs_to(0u, 0u));
    CHECK(oware_house_belongs_to(5u, 0u));
    CHECK(!oware_house_belongs_to(6u, 0u));
    CHECK(oware_house_belongs_to(6u, 1u));
    CHECK(oware_house_belongs_to(11u, 1u));
    CHECK(!oware_house_belongs_to(0u, 1u));
}

static void test_rules_default(void) {
    oware_rules_t r;
    oware_rules_default(&r);
    CHECK(r.capture_rule == OWARE_CAP_STANDARD);
    CHECK(r.grandslam_rule == OWARE_GS_NO_CAPTURE);
    CHECK(r.end_mode == OWARE_END_FIRST_TO_N);
    CHECK(r.target_score == 25u);
    CHECK(r.cycle_ply_limit == 100u);
}

int main(void) {
    test_init();
    test_ownership();
    test_rules_default();
    TEST_REPORT();
}
```

- [ ] **Step 4: Write the Makefile**

Create `Makefile`:

```make
# oware engine + tests
CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -fstack-protector-strong
CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror -g -O0 \
                -fsanitize=address,undefined
INCLUDES := -I.

LIB := liboware-engine.a
ENGINE_TEST := test_oware_engine

.PHONY: all test clean

all: $(LIB)

$(LIB): oware_engine.o
	$(AR) rcs $@ $^

oware_engine.o: oware_engine.c oware_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(ENGINE_TEST)
	./$(ENGINE_TEST)

$(ENGINE_TEST): test_oware_engine.c oware_engine.c oware_engine.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_engine.c oware_engine.c

clean:
	rm -f *.o $(LIB) $(ENGINE_TEST)
```

- [ ] **Step 5: Run the test to verify it fails (link error)**

Run: `make test`
Expected: FAIL — link errors for undefined `oware_init`, `oware_house_belongs_to`, etc.

- [ ] **Step 6: Write minimal implementation**

Create `oware_engine.c`:

```c
#include "oware_engine.h"
#include <string.h>

void oware_rules_default(oware_rules_t *r) {
    r->capture_rule    = OWARE_CAP_STANDARD;
    r->grandslam_rule  = OWARE_GS_NO_CAPTURE;
    r->end_mode        = OWARE_END_FIRST_TO_N;
    r->target_score    = 25u;
    r->cycle_ply_limit = 100u;
    r->allow_agreed_end = true;
}

void oware_init(oware_state_t *s) {
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) {
        s->houses[i] = 4u;
    }
    s->score[0] = 0u;
    s->score[1] = 0u;
    s->turn = 0u;
    s->no_capture_plies = 0u;
}

bool oware_house_belongs_to(uint8_t house, uint8_t player) {
    if (house >= OWARE_HOUSES) {
        return false;
    }
    return (house < OWARE_SIDE) == (player == 0u);
}

int oware_side_seeds(const oware_state_t *s, uint8_t player) {
    int total = 0;
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) {
        if (oware_house_belongs_to(i, player)) {
            total += (int)s->houses[i];
        }
    }
    return total;
}

int oware_board_seeds(const oware_state_t *s) {
    int total = 0;
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) {
        total += (int)s->houses[i];
    }
    return total;
}
```

- [ ] **Step 7: Run the test to verify it passes**

Run: `make test`
Expected: PASS — `N/N checks passed`, exit 0.

- [ ] **Step 8: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware engine: scaffold header, test harness, init + helpers"
```

---

### Task 2: Sowing core (`oware_simulate`, no capture yet)

**Files:**
- Modify: `oware_engine.c` (add static `oware_simulate`, used later; expose just enough to test via a temporary apply)
- Modify: `test_oware_engine.c`

We test sowing through a thin internal entry. Add a test-only declaration guarded so production stays clean.

- [ ] **Step 1: Add the failing sowing test**

Append to `test_oware_engine.c` (and call from `main`):

```c
/* test hook implemented in oware_engine.c */
bool oware__simulate_for_test(const oware_state_t *s, const oware_rules_t *r,
                              uint8_t house, oware_state_t *out,
                              oware_move_result_t *res);

static void test_sow_basic(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    /* player 0 plays house 2 (4 seeds) -> houses 3,4,5,6 each +1, house 2 -> 0 */
    oware_state_t out; oware_move_result_t res;
    CHECK(oware__simulate_for_test(&s, &r, 2u, &out, &res));
    CHECK(out.houses[2] == 0u);
    CHECK(out.houses[3] == 5u);
    CHECK(out.houses[4] == 5u);
    CHECK(out.houses[5] == 5u);
    CHECK(out.houses[6] == 5u);
    CHECK(res.landing == 6u);
}

static void test_sow_skip_origin(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    s.houses[0] = 12u; /* sowing 12 must skip house 0 on the wrap */
    oware_state_t out; oware_move_result_t res;
    CHECK(oware__simulate_for_test(&s, &r, 0u, &out, &res));
    CHECK(out.houses[0] == 0u);          /* origin emptied and skipped */
    CHECK(out.houses[1] == 5u);          /* every other house +1 */
    CHECK(out.houses[11] == 5u);
    CHECK(res.landing == 11u);           /* 12th seed lands in house 11, not 0 */
}
```

Add to `main()` before `TEST_REPORT()`: `test_sow_basic(); test_sow_skip_origin();`

- [ ] **Step 2: Run to verify it fails**

Run: `make test`
Expected: FAIL — undefined `oware__simulate_for_test`.

- [ ] **Step 3: Implement sowing in `oware_engine.c`**

Add above the public functions:

```c
/* Sow only (capture added in Task 3). Returns false if 'house' is not a
   valid own, non-empty source. Computes resulting board into *out. */
static bool oware_simulate(const oware_state_t *s, const oware_rules_t *r,
                           uint8_t house, oware_state_t *out,
                           oware_move_result_t *res) {
    uint8_t p = s->turn;
    (void)r;
    if (house >= OWARE_HOUSES) { return false; }
    if (!oware_house_belongs_to(house, p)) { return false; }
    if (s->houses[house] == 0u) { return false; }

    *out = *s;
    uint8_t seeds = out->houses[house];
    out->houses[house] = 0u;
    uint8_t i = house;
    while (seeds > 0u) {
        i = (uint8_t)((i + 1u) % OWARE_HOUSES);
        if (i == house) { continue; }            /* skip origin on wrap */
        out->houses[i] = (uint8_t)(out->houses[i] + 1u);
        seeds--;
    }

    res->landing = i;
    res->captured = 0u;
    res->was_grand_slam = false;
    res->forced_feed = false;
    return true;
}

bool oware__simulate_for_test(const oware_state_t *s, const oware_rules_t *r,
                              uint8_t house, oware_state_t *out,
                              oware_move_result_t *res) {
    return oware_simulate(s, r, house, out, res);
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware engine: sowing with 12+ origin-skip rule"
```

---

### Task 3: Capture — standard {2,3}, simple and chained

**Files:**
- Modify: `oware_engine.c` (extend `oware_simulate` with capture)
- Modify: `test_oware_engine.c`

- [ ] **Step 1: Add failing capture tests**

Append and call from `main()`:

```c
static void test_capture_simple(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    /* craft: player 0 to move; landing makes an opponent house exactly 3 */
    memset(s.houses, 0, sizeof(s.houses));
    s.houses[5] = 1u;   /* play this 1 seed -> lands in house 6 */
    s.houses[6] = 2u;   /* becomes 3 -> captured */
    oware_state_t out; oware_move_result_t res;
    CHECK(oware__simulate_for_test(&s, &r, 5u, &out, &res));
    CHECK(res.landing == 6u);
    CHECK(res.captured == 3u);
    CHECK(out.houses[6] == 0u);
    CHECK(out.score[0] == 3u);
}

static void test_capture_chained(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    memset(s.houses, 0, sizeof(s.houses));
    s.houses[5] = 3u;   /* lands in house 8 after 6,7,8 */
    s.houses[6] = 1u;   /* ->2 captured */
    s.houses[7] = 2u;   /* ->3 captured */
    s.houses[8] = 1u;   /* ->2 captured (landing) */
    oware_state_t out; oware_move_result_t res;
    CHECK(oware__simulate_for_test(&s, &r, 5u, &out, &res));
    CHECK(res.landing == 8u);
    CHECK(res.captured == 7u);          /* 2 + 3 + 2 */
    CHECK(out.houses[6] == 0u);
    CHECK(out.houses[7] == 0u);
    CHECK(out.houses[8] == 0u);
    CHECK(out.score[0] == 7u);
}

static void test_capture_stops_at_own_side(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    memset(s.houses, 0, sizeof(s.houses));
    /* player 1 to move; chain must stop when it reaches player 1's own houses */
    s.turn = 1u;
    s.houses[11] = 1u;  /* lands in house 0 (wrap) */
    s.houses[0]  = 2u;  /* ->3 captured (opponent of p1) */
    s.houses[1]  = 2u;  /* ->2? no: not on path; ensure stop at own side after 0 */
    oware_state_t out; oware_move_result_t res;
    CHECK(oware__simulate_for_test(&s, &r, 11u, &out, &res));
    CHECK(res.landing == 0u);
    CHECK(res.captured == 3u);          /* only house 0; house 11 is p1's own */
    CHECK(out.score[1] == 3u);
}
```

`#include <string.h>` is already in `oware_engine.c`; add `#include <string.h>` to `test_oware_engine.c` for `memset`.

- [ ] **Step 2: Run to verify it fails**

Run: `make test`
Expected: FAIL — captures not implemented (`res.captured` stays 0).

- [ ] **Step 3: Implement capture in `oware_simulate`**

Add a capture predicate near the top of `oware_engine.c`:

```c
static bool oware_capturable(uint8_t count, oware_capture_rule_t rule) {
    switch (rule) {
        case OWARE_CAP_STANDARD:   return (count == 2u) || (count == 3u);
        case OWARE_CAP_THREE_FOUR: return (count == 3u) || (count == 4u);
        default:                   return false;
    }
}
```

Replace the tail of `oware_simulate` (after computing `res->landing = i;`) with:

```c
    res->landing = i;
    res->captured = 0u;
    res->was_grand_slam = false;
    res->forced_feed = false;

    /* provisional capture chain: walk backward over opponent houses */
    uint8_t cap_idx[OWARE_SIDE];
    uint8_t cap_count = 0u;
    uint8_t j = i;
    for (;;) {
        if (oware_house_belongs_to(j, p)) { break; }          /* own side */
        if (!oware_capturable(out->houses[j], r->capture_rule)) { break; }
        cap_idx[cap_count] = j;
        cap_count++;
        if (cap_count >= OWARE_SIDE) { break; }
        j = (j == 0u) ? (uint8_t)(OWARE_HOUSES - 1u) : (uint8_t)(j - 1u);
    }

    /* (grand-slam handling arrives in Task 5; for now capture normally) */
    for (uint8_t c = 0; c < cap_count; c++) {
        out->score[p] = (uint8_t)(out->score[p] + out->houses[cap_idx[c]]);
        res->captured = (uint8_t)(res->captured + out->houses[cap_idx[c]]);
        out->houses[cap_idx[c]] = 0u;
    }
    return true;
```

Remove the now-duplicated `res->landing/captured/...` assignments left from Task 2.

- [ ] **Step 4: Run to verify it passes**

Run: `make test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware engine: chained capture on standard 2/3 rule"
```

---

### Task 4: Configurable capture rule {3,4}

**Files:**
- Modify: `test_oware_engine.c`

(The predicate already supports it; this task proves the variant and guards regressions.)

- [ ] **Step 1: Add failing test for the 3/4 variant**

Append and call from `main()`:

```c
static void test_capture_three_four(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    r.capture_rule = OWARE_CAP_THREE_FOUR;
    memset(s.houses, 0, sizeof(s.houses));
    s.houses[5] = 1u;
    s.houses[6] = 3u;   /* ->4 captured under {3,4}; NOT under {2,3} */
    oware_state_t out; oware_move_result_t res;
    CHECK(oware__simulate_for_test(&s, &r, 5u, &out, &res));
    CHECK(res.captured == 4u);
    CHECK(out.score[0] == 4u);

    /* and confirm a "2" is NOT captured under {3,4} */
    oware_init(&s);
    memset(s.houses, 0, sizeof(s.houses));
    s.houses[5] = 1u;
    s.houses[6] = 1u;   /* ->2: capturable under standard, not under {3,4} */
    CHECK(oware__simulate_for_test(&s, &r, 5u, &out, &res));
    CHECK(res.captured == 0u);
}
```

- [ ] **Step 2: Run to verify it fails or passes**

Run: `make test`
Expected: PASS immediately (predicate already supports {3,4}). If it fails, the predicate is wrong — fix `oware_capturable`.

- [ ] **Step 3: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware engine: test configurable 3/4 capture rule"
```

---

### Task 5: Grand-slam detection + four variants

**Files:**
- Modify: `oware_engine.c` (grand-slam branch in `oware_simulate`)
- Modify: `test_oware_engine.c`

- [ ] **Step 1: Add failing grand-slam tests**

Append and call from `main()`:

```c
/* Build a position where player 0's move would capture ALL of player 1's seeds.
   Player 1 owns 6..11. Put a single capturable seed only in house 6, rest empty. */
static void setup_grandslam(oware_state_t *s) {
    oware_init(s);
    memset(s->houses, 0, sizeof(s->houses));
    s->turn = 0u;
    s->houses[5] = 1u;   /* lands in 6 */
    s->houses[6] = 2u;   /* ->3, and it's the only opponent seed -> grand slam */
}

static void test_grandslam_no_capture(void) {
    oware_state_t s; oware_rules_t r;
    setup_grandslam(&s); oware_rules_default(&r);
    r.grandslam_rule = OWARE_GS_NO_CAPTURE;
    oware_state_t out; oware_move_result_t res;
    CHECK(oware__simulate_for_test(&s, &r, 5u, &out, &res));
    CHECK(res.was_grand_slam);
    CHECK(res.captured == 0u);       /* nothing captured */
    CHECK(out.houses[6] == 3u);      /* seeds remain sown */
    CHECK(out.score[0] == 0u);
}

static void test_grandslam_opponent_keeps(void) {
    oware_state_t s; oware_rules_t r;
    setup_grandslam(&s); oware_rules_default(&r);
    r.grandslam_rule = OWARE_GS_OPPONENT_KEEPS;
    /* give player 0 some board seeds that should sweep to opponent */
    s.houses[0] = 5u;
    oware_state_t out; oware_move_result_t res;
    CHECK(oware__simulate_for_test(&s, &r, 5u, &out, &res));
    CHECK(res.was_grand_slam);
    CHECK(out.score[0] == 3u);       /* player took the slam */
    CHECK(out.score[1] == 5u);       /* remaining board swept to opponent */
    CHECK(oware_board_seeds(&out) == 0);
}

static void test_grandslam_leave_last(void) {
    oware_state_t s; oware_rules_t r;
    oware_rules_default(&r);
    r.grandslam_rule = OWARE_GS_LEAVE_LAST;
    /* chain of two opponent houses, both capturable, together = all opp seeds */
    oware_init(&s);
    memset(s.houses, 0, sizeof(s.houses));
    s.turn = 0u;
    s.houses[5] = 2u;    /* lands in 7 after 6,7 */
    s.houses[6] = 1u;    /* ->2 (furthest back in chain) */
    s.houses[7] = 2u;    /* ->3 (landing) */
    oware_state_t out; oware_move_result_t res;
    CHECK(oware__simulate_for_test(&s, &r, 5u, &out, &res));
    CHECK(res.was_grand_slam);
    CHECK(out.houses[7] == 0u);      /* landing captured */
    CHECK(out.houses[6] == 2u);      /* furthest-back house spared */
    CHECK(res.captured == 3u);
    CHECK(out.score[0] == 3u);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test`
Expected: FAIL — `was_grand_slam` never set; variants not handled.

- [ ] **Step 3: Implement the grand-slam branch**

In `oware_simulate`, replace the "capture normally" block (from Task 3) with:

```c
    /* detect grand slam: would the capture empty the opponent's whole side? */
    uint8_t opp = (uint8_t)(p ^ 1u);
    int cap_total = 0;
    for (uint8_t c = 0; c < cap_count; c++) {
        cap_total += (int)out->houses[cap_idx[c]];
    }
    int opp_total = oware_side_seeds(out, opp);
    bool grand_slam = (cap_count > 0u) && (cap_total == opp_total);
    res->was_grand_slam = grand_slam;

    if (cap_count == 0u) {
        return true;
    }

    if (!grand_slam) {
        for (uint8_t c = 0; c < cap_count; c++) {
            out->score[p] = (uint8_t)(out->score[p] + out->houses[cap_idx[c]]);
            res->captured = (uint8_t)(res->captured + out->houses[cap_idx[c]]);
            out->houses[cap_idx[c]] = 0u;
        }
        return true;
    }

    switch (r->grandslam_rule) {
        case OWARE_GS_NO_CAPTURE:
            break;                       /* seeds stay sown; capture nothing */
        case OWARE_GS_FORBIDDEN:
            break;                       /* legality enforced in legal_moves */
        case OWARE_GS_OPPONENT_KEEPS:
            for (uint8_t c = 0; c < cap_count; c++) {
                out->score[p] = (uint8_t)(out->score[p] + out->houses[cap_idx[c]]);
                res->captured = (uint8_t)(res->captured + out->houses[cap_idx[c]]);
                out->houses[cap_idx[c]] = 0u;
            }
            for (uint8_t k = 0; k < OWARE_HOUSES; k++) {
                out->score[opp] = (uint8_t)(out->score[opp] + out->houses[k]);
                out->houses[k] = 0u;
            }
            break;
        case OWARE_GS_LEAVE_LAST:
            /* cap_idx is ordered landing-first; furthest-back is last entry */
            for (uint8_t c = 0; (c + 1u) < cap_count; c++) {
                out->score[p] = (uint8_t)(out->score[p] + out->houses[cap_idx[c]]);
                res->captured = (uint8_t)(res->captured + out->houses[cap_idx[c]]);
                out->houses[cap_idx[c]] = 0u;
            }
            break;
        default:
            break;
    }
    return true;
```

Note: `OWARE_GS_LEAVE_LAST` with a single-house chain (`cap_count == 1`) captures nothing, sparing that house — the intended "don't starve" behavior.

- [ ] **Step 4: Run to verify it passes**

Run: `make test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware engine: grand-slam detection and four variants"
```

---

### Task 6: Legal move generation + feeding obligation + forbidden slam

**Files:**
- Modify: `oware_engine.c` (`oware_legal_moves`, `oware_is_legal`)
- Modify: `test_oware_engine.c`

- [ ] **Step 1: Add failing tests**

Append and call from `main()`:

```c
static bool in_list(const uint8_t *a, int n, uint8_t v) {
    for (int k = 0; k < n; k++) { if (a[k] == v) { return true; } }
    return false;
}

static void test_legal_moves_basic(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    uint8_t mv[OWARE_SIDE];
    int n = oware_legal_moves(&s, &r, mv);
    CHECK(n == 6);                       /* all own houses non-empty */
    CHECK(in_list(mv, n, 0u));
    CHECK(in_list(mv, n, 5u));
    CHECK(!in_list(mv, n, 6u));          /* opponent house never legal */
}

static void test_feeding_obligation(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    memset(s.houses, 0, sizeof(s.houses));
    s.turn = 0u;
    /* opponent (6..11) all empty; only house 5 can reach them */
    s.houses[1] = 2u;   /* sows within own side -> does NOT feed -> illegal */
    s.houses[5] = 3u;   /* reaches houses 6,7,8 -> feeds -> legal */
    uint8_t mv[OWARE_SIDE];
    int n = oware_legal_moves(&s, &r, mv);
    CHECK(n == 1);
    CHECK(mv[0] == 5u);
}

static void test_forbidden_grandslam(void) {
    oware_state_t s; oware_rules_t r;
    setup_grandslam(&s); oware_rules_default(&r);
    r.grandslam_rule = OWARE_GS_FORBIDDEN;
    uint8_t mv[OWARE_SIDE];
    int n = oware_legal_moves(&s, &r, mv);
    CHECK(!in_list(mv, n, 5u));          /* the slam move is excluded */
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test`
Expected: FAIL — `oware_legal_moves` returns nothing / link error.

- [ ] **Step 3: Implement legal-move generation**

Add to `oware_engine.c`:

```c
int oware_legal_moves(const oware_state_t *s, const oware_rules_t *r,
                      uint8_t out[OWARE_SIDE]) {
    uint8_t p = s->turn;
    uint8_t opp = (uint8_t)(p ^ 1u);
    bool opp_empty = (oware_side_seeds(s, opp) == 0);
    int n = 0;
    for (uint8_t h = 0; h < OWARE_HOUSES; h++) {
        if (!oware_house_belongs_to(h, p)) { continue; }
        if (s->houses[h] == 0u) { continue; }
        oware_state_t tmp;
        oware_move_result_t res;
        if (!oware_simulate(s, r, h, &tmp, &res)) { continue; }
        if (res.was_grand_slam && (r->grandslam_rule == OWARE_GS_FORBIDDEN)) {
            continue;
        }
        if (opp_empty && (oware_side_seeds(&tmp, opp) == 0)) {
            continue;                    /* fails feeding obligation */
        }
        out[n] = h;
        n++;
    }
    return n;
}

bool oware_is_legal(const oware_state_t *s, const oware_rules_t *r, uint8_t house) {
    uint8_t mv[OWARE_SIDE];
    int n = oware_legal_moves(s, r, mv);
    for (int k = 0; k < n; k++) {
        if (mv[k] == house) { return true; }
    }
    return false;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware engine: legal moves, feeding obligation, forbidden slam"
```

---

### Task 7: Apply move (turn switch + cycle counter)

**Files:**
- Modify: `oware_engine.c` (`oware_apply_move`)
- Modify: `test_oware_engine.c`

- [ ] **Step 1: Add failing tests**

Append and call from `main()`:

```c
static void test_apply_move(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    oware_move_result_t res;
    CHECK(oware_apply_move(&s, &r, 2u, &res));
    CHECK(s.turn == 1u);                 /* turn switched */
    CHECK(s.houses[2] == 0u);
    CHECK(s.no_capture_plies == 1u);     /* no capture this move */
    CHECK(oware_board_seeds(&s) + s.score[0] + s.score[1] == 48); /* conservation */
}

static void test_apply_rejects_illegal(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    oware_move_result_t res;
    CHECK(!oware_apply_move(&s, &r, 6u, &res));  /* opponent house */
    CHECK(s.turn == 0u);                          /* unchanged */
}

static void test_apply_capture_resets_cycle(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    memset(s.houses, 0, sizeof(s.houses));
    s.turn = 0u; s.no_capture_plies = 7u;
    s.houses[5] = 1u; s.houses[6] = 2u;  /* capture */
    oware_move_result_t res;
    CHECK(oware_apply_move(&s, &r, 5u, &res));
    CHECK(res.captured == 3u);
    CHECK(s.no_capture_plies == 0u);     /* reset on capture */
    CHECK(s.score[0] == 3u);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test`
Expected: FAIL — `oware_apply_move` not implemented.

- [ ] **Step 3: Implement apply**

Add to `oware_engine.c`:

```c
bool oware_apply_move(oware_state_t *s, const oware_rules_t *r,
                      uint8_t house, oware_move_result_t *res) {
    if (!oware_is_legal(s, r, house)) {
        return false;
    }
    uint8_t opp = (uint8_t)(s->turn ^ 1u);
    bool was_feeding = (oware_side_seeds(s, opp) == 0);

    oware_state_t out;
    oware_move_result_t mr;
    (void)oware_simulate(s, r, house, &out, &mr);
    mr.forced_feed = was_feeding;

    if (mr.captured > 0u) {
        out.no_capture_plies = 0u;
    } else {
        out.no_capture_plies = (uint16_t)(s->no_capture_plies + 1u);
    }
    out.turn = (uint8_t)(s->turn ^ 1u);

    *s = out;
    if (res != NULL) {
        *res = mr;
    }
    return true;
}
```

Add `#include <stddef.h>` to `oware_engine.c` for `NULL` (or rely on `string.h`, which defines it).

- [ ] **Step 4: Run to verify it passes**

Run: `make test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware engine: apply move with turn switch and cycle counter"
```

---

### Task 8: End-of-game detection + agreed ending

**Files:**
- Modify: `oware_engine.c` (`oware_is_over`, `oware_resolve_agreed`, static `collect_each_side`)
- Modify: `test_oware_engine.c`

- [ ] **Step 1: Add failing tests**

Append and call from `main()`:

```c
static void test_over_first_to_n(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);   /* target 25 */
    s.score[0] = 25u;
    oware_result_t res;
    CHECK(oware_is_over(&s, &r, &res));
    CHECK(res.outcome == OWARE_OUT_P0);
}

static void test_over_no_move_collects(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    memset(s.houses, 0, sizeof(s.houses));
    s.turn = 0u;                 /* player 0 has no seeds -> cannot move */
    s.houses[6] = 10u;           /* all remaining belong to player 1 */
    s.houses[7] = 2u;
    oware_result_t res;
    CHECK(oware_is_over(&s, &r, &res));
    CHECK(res.score[1] == 12u);  /* player 1 collects own side */
    CHECK(res.outcome == OWARE_OUT_P1);
}

static void test_over_cycle_split(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    s.no_capture_plies = r.cycle_ply_limit;   /* trip the cycle */
    oware_result_t res;
    CHECK(oware_is_over(&s, &r, &res));
    CHECK(res.score[0] == 24u);  /* each side's 24 seeds go to its owner */
    CHECK(res.score[1] == 24u);
    CHECK(res.outcome == OWARE_OUT_DRAW);
}

static void test_resolve_agreed(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    oware_result_t res;
    oware_resolve_agreed(&s, &res);
    CHECK(res.over);
    CHECK(res.score[0] == 24u);
    CHECK(res.score[1] == 24u);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test`
Expected: FAIL — `oware_is_over` / `oware_resolve_agreed` not implemented.

- [ ] **Step 3: Implement end detection**

Add to `oware_engine.c`:

```c
static void oware_collect_each_side(oware_state_t *s) {
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) {
        if (s->houses[i] > 0u) {
            uint8_t owner = (i < OWARE_SIDE) ? 0u : 1u;
            s->score[owner] = (uint8_t)(s->score[owner] + s->houses[i]);
            s->houses[i] = 0u;
        }
    }
}

static void oware_set_outcome(oware_result_t *res) {
    if (res->score[0] > res->score[1]) {
        res->outcome = OWARE_OUT_P0;
    } else if (res->score[1] > res->score[0]) {
        res->outcome = OWARE_OUT_P1;
    } else {
        res->outcome = OWARE_OUT_DRAW;
    }
}

bool oware_is_over(const oware_state_t *s, const oware_rules_t *r,
                   oware_result_t *res) {
    res->over = false;
    res->outcome = OWARE_OUT_NONE;
    res->score[0] = s->score[0];
    res->score[1] = s->score[1];

    if (r->end_mode == OWARE_END_FIRST_TO_N) {
        if ((s->score[0] >= r->target_score) ||
            (s->score[1] >= r->target_score)) {
            res->over = true;
        }
    }

    if (!res->over && (oware_board_seeds(s) == 0)) {
        res->over = true;                /* all seeds captured */
    }

    if (!res->over && (r->cycle_ply_limit > 0u) &&
        (s->no_capture_plies >= r->cycle_ply_limit)) {
        oware_state_t tmp = *s;
        oware_collect_each_side(&tmp);
        res->score[0] = tmp.score[0];
        res->score[1] = tmp.score[1];
        res->over = true;
    }

    if (!res->over) {
        uint8_t mv[OWARE_SIDE];
        if (oware_legal_moves(s, r, mv) == 0) {
            oware_state_t tmp = *s;
            oware_collect_each_side(&tmp);
            res->score[0] = tmp.score[0];
            res->score[1] = tmp.score[1];
            res->over = true;
        }
    }

    if (res->over) {
        oware_set_outcome(res);
    }
    return res->over;
}

void oware_resolve_agreed(const oware_state_t *s, oware_result_t *res) {
    oware_state_t tmp = *s;
    oware_collect_each_side(&tmp);
    res->over = true;
    res->score[0] = tmp.score[0];
    res->score[1] = tmp.score[1];
    oware_set_outcome(res);
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware engine: end detection (first-to-N, all-captured, no-move, cycle, agreed)"
```

---

### Task 9: Seed-conservation property test + README + lib build

**Files:**
- Modify: `test_oware_engine.c`
- Create: `README.md`

- [ ] **Step 1: Add a conservation property test (random self-play)**

Append and call from `main()`:

```c
#include <stdlib.h>

static void test_conservation_random_selfplay(void) {
    oware_rules_t r;
    oware_rules_default(&r);
    for (int game = 0; game < 200; game++) {
        oware_state_t s;
        oware_init(&s);
        srand((unsigned)game + 1u);
        for (int ply = 0; ply < 300; ply++) {
            oware_result_t over;
            if (oware_is_over(&s, &r, &over)) { break; }
            uint8_t mv[OWARE_SIDE];
            int n = oware_legal_moves(&s, &r, mv);
            if (n == 0) { break; }
            oware_move_result_t mr;
            CHECK(oware_apply_move(&s, &r, mv[rand() % n], &mr));
            /* invariant: nothing created or destroyed, ever */
            CHECK(oware_board_seeds(&s) + (int)s.score[0] + (int)s.score[1] == 48);
            /* scores never exceed total */
            CHECK((int)s.score[0] + (int)s.score[1] <= 48);
        }
    }
}
```

- [ ] **Step 2: Run to verify it passes**

Run: `make test`
Expected: PASS — the invariant holds across thousands of random moves; ASan/UBSan clean.

- [ ] **Step 3: Write README**

Create `README.md`:

```markdown
# oware engine

Pure C99 rules engine for Oware (Abapa variant) — ZAKO OS Phase 6.

- I/O-free, no dynamic allocation, ~16-byte game state.
- Configurable variants: capture rule (2/3 or 3/4), four grand-slam rules,
  end mode (first-to-N or all-captured), cycle limit, agreed ending.

## Build

```bash
make            # builds liboware-engine.a
make test       # builds and runs unit tests (ASan + UBSan)
```

## API

See `oware_engine.h`. Core flow: `oware_init` → loop { `oware_is_over`?,
`oware_legal_moves`, `oware_apply_move` } → read `oware_result_t`.
```

- [ ] **Step 4: Verify the library target builds clean**

Run: `make clean && make && make test`
Expected: `liboware-engine.a` produced; tests pass; zero warnings (`-Werror`).

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware engine: conservation property test, README, lib build"
```

---

## Self-Review (completed during planning)

- **Spec coverage:** sowing + 12-skip (T2), capture 2/3 + chained + stop-at-own-side (T3), configurable 3/4 (T4), grand-slam detection + all four variants (T5), feeding obligation + forbidden slam (T6), apply/turn/cycle counter (T7), end modes first-to-N / all-captured / no-move collection / cycle split / agreed ending (T8), conservation invariant (T9). All §5 engine requirements from `DESIGN.md` are covered. AI (§6), store (§7), UI (§9) are out of scope for this plan by design.
- **Placeholder scan:** none — every step has concrete code and exact commands.
- **Type consistency:** `oware_simulate` signature, `oware_move_result_t` fields (`landing`, `captured`, `was_grand_slam`, `forced_feed`), `oware_outcome_t` values (`OWARE_OUT_*`), and `cap_idx` ordering (landing-first, furthest-back last — used by `LEAVE_LAST`) are consistent across all tasks.

## Open question carried from DESIGN.md §14

If "3/4" should mean **{2,3,4}** rather than **{3,4}**, change only `oware_capturable`'s `OWARE_CAP_THREE_FOUR` case and the Task 4 test expectation. No other code is affected.

## Next plans (after this is green)

- **Plan 2 — `oware_ai`:** alpha-beta minimax + eval + 3 difficulties (depends on this engine).
- **Plan 3 — `oware_store`:** pinned settings + vs-CPU ± + per-pair head-to-head (line-based file).
- **Plan 4 — `oware_ui` + `main`:** terminal render, input, menus, match/series flow; ties everything into the playable app.
```
