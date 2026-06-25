# Oware UI + main — Implementation Plan (Plan 4 of 4)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Build `oware_ui` + `main.c` — the terminal numeric-board interface, input handling, menus, vs-computer / 2-player modes, and persistence wiring — producing a runnable `oware` binary that plays a complete game against the computer or another human and records results.

**Architecture:** All interaction goes through an injected `oware_io_t` (read_line/write_str callbacks). This makes the **entire app loop unit-testable** with scripted input — no real terminal needed. `oware_ui.c` holds the render, input parsing, single-game driver, and the menu/match loop. `main.c` is a thin wrapper: build a stdio-backed `oware_io_t`, load the store, call `oware_ui_run`, save the store.

**Tech Stack:** C99, GNU Make, `-Wall -Wextra -Wpedantic -Werror`, ASan+UBSan test build. Links engine + AI + store libs. No dynamic allocation; fixed buffers.

See `DESIGN.md §8/§9` for the spec. All paths relative to `zako-os-v1/system/components/oware/`.

## File Structure

| File | Responsibility |
|------|----------------|
| `oware_ui.h` | `oware_io_t`, `oware_match_cfg_t`, render/parse/play/run declarations |
| `oware_ui.c` | input mapping, board render, single-game driver, menu/match loop |
| `main.c` | stdio `oware_io_t`, store load/save, calls `oware_ui_run` |
| `test_oware_ui.c` | unit tests with a scripted `oware_io_t`; own `main()` |
| `Makefile` | extend: `oware` binary + `test_oware_ui` |

---

### Task 1: Scaffold + input mapping

**Files:**
- Create: `oware_ui.h`, `oware_ui.c`, `test_oware_ui.c`
- Modify: `Makefile`, `.gitignore`

- [ ] **Step 1: Write the header**

Create `oware_ui.h`:

```c
#ifndef OWARE_UI_H
#define OWARE_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "oware_engine.h"
#include "oware_ai.h"
#include "oware_store.h"

/* Injected I/O so the whole app loop is testable without real stdio. */
typedef struct oware_io {
    /* Read one line (without trailing newline) into buf (cap incl NUL).
       Returns false on EOF / no more input. */
    bool (*read_line)(struct oware_io *io, char *buf, size_t cap);
    /* Write a NUL-terminated string. */
    void (*write_str)(struct oware_io *io, const char *s);
    void *ctx;
} oware_io_t;

typedef struct {
    bool                  side_is_ai[2];
    oware_ai_config_t     ai_cfg;
    char                  name[2][OWARE_STORE_NAME_CAP];
    oware_ai_difficulty_t difficulty; /* vs-CPU record bucket */
    bool                  vs_cpu;
    uint8_t               human_side; /* which side the human plays in vs-CPU */
} oware_match_cfg_t;

/* '1'..'6' -> ring house index for player. p0 -> 0..5, p1 -> 6..11. */
bool   oware_ui_house_for_key(uint8_t player, char key, uint8_t *house);
/* Parse a house from a line (skips leading blanks; uses first char). */
bool   oware_ui_parse_house(const char *line, uint8_t player, uint8_t *house);

/* Render numeric board for `viewer` into buf. Returns bytes written (excl NUL). */
size_t oware_ui_render_board(const oware_state_t *s, uint8_t viewer,
                             char *buf, size_t cap);

/* Play one game to completion via io. Re-prompts on illegal human input.
   'q' as human input resolves the game by own-side split (agreed end). */
oware_result_t oware_ui_play_game(const oware_rules_t *r,
                                  const oware_match_cfg_t *m, oware_io_t *io);

/* Top-level loop: menu -> matches -> update+persist store. Returns on quit. */
void   oware_ui_run(oware_io_t *io, oware_store_t *store, const char *path);

#endif /* OWARE_UI_H */
```

- [ ] **Step 2: Extend the Makefile**

Add to the variables block:
```make
LIB_UI := liboware-ui.a
UI_TEST := test_oware_ui
APP := oware
```
Change `all:` to also build the app:
```make
all: $(LIB) $(LIB_AI) $(LIB_STORE) $(LIB_UI) $(APP)
```
Add rules:
```make
$(LIB_UI): oware_ui.o
	$(AR) rcs $@ $^

oware_ui.o: oware_ui.c oware_ui.h oware_engine.h oware_ai.h oware_store.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(APP): main.c oware_ui.c oware_store.c oware_ai.c oware_engine.c \
        oware_ui.h oware_store.h oware_ai.h oware_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ main.c oware_ui.c oware_store.c oware_ai.c oware_engine.c
```
Change `test:` to add the UI suite:
```make
test: $(ENGINE_TEST) $(AI_TEST) $(STORE_TEST) $(UI_TEST)
	./$(ENGINE_TEST)
	./$(AI_TEST)
	./$(STORE_TEST)
	./$(UI_TEST)
```
Add the UI test rule:
```make
$(UI_TEST): test_oware_ui.c oware_ui.c oware_store.c oware_ai.c oware_engine.c \
            oware_ui.h oware_store.h oware_ai.h oware_engine.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_ui.c oware_ui.c oware_store.c oware_ai.c oware_engine.c
```
Change `clean:` to include the new outputs:
```make
clean:
	rm -f *.o $(LIB) $(LIB_AI) $(LIB_STORE) $(LIB_UI) \
	      $(ENGINE_TEST) $(AI_TEST) $(STORE_TEST) $(UI_TEST) $(APP)
```
Append to `.gitignore`:
```
test_oware_ui
oware
```

> Note: `$(APP)` depends on `main.c`, created in Task 5. Until then, build/test with the explicit targets used in each task's commands (`make $(UI_TEST)` etc.), not bare `make all`. Task 5 makes `make all` whole.

- [ ] **Step 3: Write the failing test for input mapping**

Create `test_oware_ui.c`:

```c
#include "oware_ui.h"
#include "oware_test.h"
#include <string.h>

static void test_house_for_key(void) {
    uint8_t h = 0xFFu;
    CHECK(oware_ui_house_for_key(0u, '1', &h) && h == 0u);
    CHECK(oware_ui_house_for_key(0u, '6', &h) && h == 5u);
    CHECK(oware_ui_house_for_key(1u, '1', &h) && h == 6u);
    CHECK(oware_ui_house_for_key(1u, '6', &h) && h == 11u);
    CHECK(!oware_ui_house_for_key(0u, '0', &h));
    CHECK(!oware_ui_house_for_key(0u, '7', &h));
    CHECK(!oware_ui_house_for_key(0u, 'x', &h));
}

static void test_parse_house(void) {
    uint8_t h = 0xFFu;
    CHECK(oware_ui_parse_house("  3", 0u, &h) && h == 2u);
    CHECK(oware_ui_parse_house("4", 1u, &h) && h == 9u);
    CHECK(!oware_ui_parse_house("", 0u, &h));
    CHECK(!oware_ui_parse_house("nope", 0u, &h));
}

int main(void) {
    test_house_for_key();
    test_parse_house();
    TEST_REPORT();
}
```

- [ ] **Step 4: Run to verify it fails**

Run: `make $(UI_TEST)` (or `make test_oware_ui`)
Expected: FAIL — link errors for `oware_ui_house_for_key`, `oware_ui_parse_house`.

- [ ] **Step 5: Implement input mapping**

Create `oware_ui.c`:

```c
#include "oware_ui.h"
#include <stdio.h>
#include <string.h>

bool oware_ui_house_for_key(uint8_t player, char key, uint8_t *house) {
    if ((key < '1') || (key > '6')) {
        return false;
    }
    uint8_t k = (uint8_t)(key - '1');
    uint8_t base = (player == 0u) ? 0u : 6u;
    *house = (uint8_t)(base + k);
    return true;
}

bool oware_ui_parse_house(const char *line, uint8_t player, uint8_t *house) {
    size_t i = 0u;
    if (line == NULL) {
        return false;
    }
    while ((line[i] == ' ') || (line[i] == '\t')) {
        i++;
    }
    return oware_ui_house_for_key(player, line[i], house);
}
```

- [ ] **Step 6: Run to verify it passes**

Run: `make test_oware_ui`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware ui: scaffold header, Makefile, input mapping

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Board renderer

**Files:**
- Modify: `oware_ui.c`, `test_oware_ui.c`

- [ ] **Step 1: Add failing test**

Append and call from `main()`:

```c
#include "oware_engine.h"

static void test_render_board(void) {
    oware_state_t s;
    oware_init(&s);
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) { s.houses[i] = i; }
    s.score[0] = 7u; s.score[1] = 3u;
    char buf[512];
    size_t n = oware_ui_render_board(&s, 0u, buf, sizeof(buf));
    CHECK(n > 0u);
    /* viewer 0: your houses (idx 0..5) in key order -> 0 1 2 3 4 5 */
    CHECK(strstr(buf, " 0  1  2  3  4  5") != NULL);
    /* opponent houses (idx 6..11) shown key6..key1 -> 11 10 9 8 7 6 */
    CHECK(strstr(buf, "11 10  9  8  7  6") != NULL);
    CHECK(strstr(buf, "You: 7") != NULL);
    CHECK(strstr(buf, "Opp: 3") != NULL);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test_oware_ui`
Expected: FAIL — undefined `oware_ui_render_board`.

- [ ] **Step 3: Implement the renderer**

Add to `oware_ui.c`:

```c
size_t oware_ui_render_board(const oware_state_t *s, uint8_t viewer,
                             char *buf, size_t cap) {
    uint8_t yb = (viewer == 0u) ? 0u : 6u; /* your base */
    uint8_t ob = (viewer == 0u) ? 6u : 0u; /* opponent base */
    int n = snprintf(buf, cap,
        "\n"
        "  Opp:  [%2u %2u %2u %2u %2u %2u]\n"
        "  You:  [%2u %2u %2u %2u %2u %2u]\n"
        "  keys:    1  2  3  4  5  6\n"
        "  Score  You: %u   Opp: %u\n",
        (unsigned)s->houses[ob + 5u], (unsigned)s->houses[ob + 4u],
        (unsigned)s->houses[ob + 3u], (unsigned)s->houses[ob + 2u],
        (unsigned)s->houses[ob + 1u], (unsigned)s->houses[ob + 0u],
        (unsigned)s->houses[yb + 0u], (unsigned)s->houses[yb + 1u],
        (unsigned)s->houses[yb + 2u], (unsigned)s->houses[yb + 3u],
        (unsigned)s->houses[yb + 4u], (unsigned)s->houses[yb + 5u],
        (unsigned)s->score[viewer], (unsigned)s->score[viewer ^ 1u]);
    if (n < 0) {
        return 0u;
    }
    return (size_t)n;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test_oware_ui`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware ui: numeric board renderer

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Single-game driver with scripted I/O

**Files:**
- Modify: `oware_ui.c`, `test_oware_ui.c`

- [ ] **Step 1: Add a scripted-IO harness and failing tests**

Append to `test_oware_ui.c` (above `main()`):

```c
#include <stdlib.h>

/* Scripted IO: feeds queued input lines; captures output into a buffer. */
typedef struct {
    const char *const *lines; /* NULL-terminated array of input lines */
    size_t next;
    char out[8192];
    size_t out_len;
} script_io_t;

static bool script_read(oware_io_t *io, char *buf, size_t cap) {
    script_io_t *s = (script_io_t *)io->ctx;
    if (s->lines[s->next] == NULL) { return false; }
    (void)snprintf(buf, cap, "%s", s->lines[s->next]);
    s->next++;
    return true;
}

static void script_write(oware_io_t *io, const char *str) {
    script_io_t *s = (script_io_t *)io->ctx;
    size_t len = strlen(str);
    if ((s->out_len + len + 1u) < sizeof(s->out)) {
        (void)memcpy(s->out + s->out_len, str, len);
        s->out_len += len;
        s->out[s->out_len] = '\0';
    }
}

static void script_io_init(oware_io_t *io, script_io_t *s,
                           const char *const *lines) {
    s->lines = lines; s->next = 0u; s->out_len = 0u; s->out[0] = '\0';
    io->read_line = script_read; io->write_str = script_write; io->ctx = s;
}
```

Then the tests:

```c
static void test_play_game_two_player_quit(void) {
    /* Player 0 quits immediately -> agreed end, 24/24 split (draw). */
    static const char *const lines[] = { "q", NULL };
    script_io_t s; oware_io_t io; script_io_init(&io, &s, lines);
    oware_rules_t r; oware_rules_default(&r);
    oware_match_cfg_t m;
    memset(&m, 0, sizeof(m));
    m.side_is_ai[0] = false; m.side_is_ai[1] = false;
    oware_result_t res = oware_ui_play_game(&r, &m, &io);
    CHECK(res.over);
    CHECK(res.score[0] == 24u && res.score[1] == 24u);
    CHECK(strstr(s.out, "Opp:") != NULL); /* board was rendered */
}

static void test_play_game_illegal_then_legal(void) {
    /* Player 0: first an illegal key '7' (rejected), then '1' (legal), then quit. */
    static const char *const lines[] = { "7", "1", "q", NULL };
    script_io_t s; oware_io_t io; script_io_init(&io, &s, lines);
    oware_rules_t r; oware_rules_default(&r);
    oware_match_cfg_t m;
    memset(&m, 0, sizeof(m));
    oware_result_t res = oware_ui_play_game(&r, &m, &io);
    CHECK(strstr(s.out, "Illegal") != NULL); /* re-prompted */
    CHECK(res.over);
}

static void test_play_game_vs_ai_completes(void) {
    /* Human (side 0) just quits; AI side 1. Game ends, result valid. */
    static const char *const lines[] = { "q", NULL };
    script_io_t s; oware_io_t io; script_io_init(&io, &s, lines);
    oware_rules_t r; oware_rules_default(&r);
    oware_match_cfg_t m;
    memset(&m, 0, sizeof(m));
    m.side_is_ai[0] = false; m.side_is_ai[1] = true;
    oware_ai_config_default(&m.ai_cfg, OWARE_AI_EASY);
    m.difficulty = OWARE_AI_EASY;
    oware_result_t res = oware_ui_play_game(&r, &m, &io);
    CHECK(res.over);
}
```

Add calls in `main()`.

- [ ] **Step 2: Run to verify it fails**

Run: `make test_oware_ui`
Expected: FAIL — undefined `oware_ui_play_game`.

- [ ] **Step 3: Implement the game driver**

Add to `oware_ui.c`:

```c
oware_result_t oware_ui_play_game(const oware_rules_t *r,
                                  const oware_match_cfg_t *m, oware_io_t *io) {
    oware_state_t s;
    oware_init(&s);
    oware_result_t res;
    char buf[512];

    for (;;) {
        if (oware_is_over(&s, r, &res)) {
            break;
        }
        uint8_t p = s.turn;
        (void)oware_ui_render_board(&s, p, buf, sizeof(buf));
        io->write_str(io, buf);

        uint8_t house = 0u;
        if (m->side_is_ai[p]) {
            oware_ai_config_t cfg = m->ai_cfg;
            if (!oware_ai_choose_move(&s, r, &cfg, p, &house)) {
                break; /* no move; is_over resolves next loop */
            }
            char msg[48];
            (void)snprintf(msg, sizeof(msg), "Computer plays %u.\n",
                           (unsigned)((house % 6u) + 1u));
            io->write_str(io, msg);
        } else {
            bool got = false;
            while (!got) {
                io->write_str(io, "Your move (1-6, q=quit): ");
                char line[64];
                if (!io->read_line(io, line, sizeof(line))) {
                    oware_resolve_agreed(&s, &res);
                    return res;
                }
                if ((line[0] == 'q') || (line[0] == 'Q')) {
                    oware_resolve_agreed(&s, &res);
                    return res;
                }
                if (oware_ui_parse_house(line, p, &house) &&
                    oware_is_legal(&s, r, house)) {
                    got = true;
                } else {
                    io->write_str(io, "Illegal move, try again.\n");
                }
            }
        }
        oware_move_result_t mr;
        (void)oware_apply_move(&s, r, house, &mr);
    }

    (void)oware_ui_render_board(&s, 0u, buf, sizeof(buf));
    io->write_str(io, buf);
    return res;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test_oware_ui`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware ui: single-game driver with injected I/O

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Menu / match loop with persistence

**Files:**
- Modify: `oware_ui.c`, `test_oware_ui.c`

This implements `oware_ui_run`: main menu → vs-CPU (pick difficulty) / 2-player (enter names) / Records (view + reset) / Settings (pin grand-slam variant) / Quit, recording results into the store and saving after each game.

- [ ] **Step 1: Add failing tests**

Append and call from `main()`:

```c
static void test_run_vs_cpu_records(void) {
    /* Menu: 1 (vs CPU), difficulty 1 (easy), then in-game quit, then 5 (quit). */
    static const char *const lines[] = { "1", "1", "q", "5", NULL };
    script_io_t s; oware_io_t io; script_io_init(&io, &s, lines);
    oware_store_t st; oware_store_init(&st);
    oware_ui_run(&io, &st, "/tmp/oware_ui_run_test.dat");
    /* A vs-CPU game was recorded under easy (quit -> agreed 24/24 draw). */
    CHECK(st.cpu[OWARE_AI_EASY].wins + st.cpu[OWARE_AI_EASY].losses
          + st.cpu[OWARE_AI_EASY].draws == 1u);
    CHECK(st.cpu[OWARE_AI_EASY].draws == 1u);
    (void)remove("/tmp/oware_ui_run_test.dat");
}

static void test_run_two_player_records(void) {
    /* Menu: 2 (2-player), name1 Kofi, name2 Abena, quit game, then 5. */
    static const char *const lines[] = { "2", "Kofi", "Abena", "q", "5", NULL };
    script_io_t s; oware_io_t io; script_io_init(&io, &s, lines);
    oware_store_t st; oware_store_init(&st);
    oware_ui_run(&io, &st, "/tmp/oware_ui_run_2p.dat");
    oware_pair_record_t *p = oware_store_pair(&st, "Kofi", "Abena");
    CHECK(p != NULL);
    CHECK(p->draws == 1u); /* quit -> 24/24 draw */
    (void)remove("/tmp/oware_ui_run_2p.dat");
}

static void test_run_settings_pin(void) {
    /* Menu: 4 (settings), set grand-slam variant to 1 (forbidden), then 5. */
    static const char *const lines[] = { "4", "1", "5", NULL };
    script_io_t s; oware_io_t io; script_io_init(&io, &s, lines);
    oware_store_t st; oware_store_init(&st);
    oware_ui_run(&io, &st, "/tmp/oware_ui_settings.dat");
    CHECK(st.grandslam_rule == OWARE_GS_FORBIDDEN);
    (void)remove("/tmp/oware_ui_settings.dat");
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test_oware_ui`
Expected: FAIL — undefined `oware_ui_run`.

- [ ] **Step 3: Implement the menu/match loop**

Add to `oware_ui.c` (helpers first, then `oware_ui_run`):

```c
static void ui_rules_from_store(const oware_store_t *st, oware_rules_t *r) {
    oware_rules_default(r);
    r->grandslam_rule = st->grandslam_rule;
    r->capture_rule   = st->capture_rule;
    r->end_mode       = st->end_mode;
    r->target_score   = st->target_score;
}

static bool ui_prompt(oware_io_t *io, const char *prompt, char *buf, size_t cap) {
    io->write_str(io, prompt);
    return io->read_line(io, buf, cap);
}

static oware_game_result_t ui_result_for_side(const oware_result_t *res,
                                              uint8_t side) {
    if (res->outcome == OWARE_OUT_DRAW) { return OWARE_GAME_DRAW; }
    bool side_won = (res->outcome == ((side == 0u) ? OWARE_OUT_P0 : OWARE_OUT_P1));
    return side_won ? OWARE_GAME_WIN : OWARE_GAME_LOSS;
}

static void ui_do_vs_cpu(oware_io_t *io, oware_store_t *st, const char *path) {
    char line[64];
    if (!ui_prompt(io, "Difficulty (1=Easy 2=Medium 3=Hard): ", line, sizeof(line))) {
        return;
    }
    oware_ai_difficulty_t d = OWARE_AI_MEDIUM;
    if (line[0] == '1') { d = OWARE_AI_EASY; }
    else if (line[0] == '3') { d = OWARE_AI_HARD; }
    else { d = OWARE_AI_MEDIUM; }

    oware_match_cfg_t m;
    memset(&m, 0, sizeof(m));
    m.vs_cpu = true;
    m.human_side = 0u;
    m.side_is_ai[0] = false;
    m.side_is_ai[1] = true;
    m.difficulty = d;
    oware_ai_config_default(&m.ai_cfg, d);

    oware_rules_t r;
    ui_rules_from_store(st, &r);
    oware_result_t res = oware_ui_play_game(&r, &m, io);
    oware_store_record_cpu(st, d, ui_result_for_side(&res, m.human_side));
    (void)oware_store_save(st, path);
}

static void ui_do_two_player(oware_io_t *io, oware_store_t *st, const char *path) {
    char n1[64];
    char n2[64];
    if (!ui_prompt(io, "Player 1 name: ", n1, sizeof(n1))) { return; }
    if (!ui_prompt(io, "Player 2 name: ", n2, sizeof(n2))) { return; }

    oware_match_cfg_t m;
    memset(&m, 0, sizeof(m));
    m.vs_cpu = false;
    m.side_is_ai[0] = false;
    m.side_is_ai[1] = false;

    oware_rules_t r;
    ui_rules_from_store(st, &r);
    oware_result_t res = oware_ui_play_game(&r, &m, io);
    (void)oware_store_record_pair(st, n1, n2, ui_result_for_side(&res, 0u));
    (void)oware_store_save(st, path);
}

static void ui_show_records(oware_io_t *io, oware_store_t *st, const char *path) {
    char line[128];
    static const char *const names[3] = { "Easy", "Medium", "Hard" };
    io->write_str(io, "\n-- vs Computer (W-L-D) --\n");
    for (size_t i = 0u; i < 3u; i++) {
        (void)snprintf(line, sizeof(line), "  %-6s  %u-%u-%u\n", names[i],
                       (unsigned)st->cpu[i].wins, (unsigned)st->cpu[i].losses,
                       (unsigned)st->cpu[i].draws);
        io->write_str(io, line);
    }
    io->write_str(io, "-- Head to head --\n");
    for (uint8_t i = 0u; i < st->pair_count; i++) {
        (void)snprintf(line, sizeof(line), "  %s %u - %u %s (draws %u)\n",
                       st->pairs[i].a, (unsigned)st->pairs[i].wins_a,
                       (unsigned)st->pairs[i].wins_b, st->pairs[i].b,
                       (unsigned)st->pairs[i].draws);
        io->write_str(io, line);
    }
    if (ui_prompt(io, "Reset vs-CPU records? (y/N): ", line, sizeof(line))) {
        if ((line[0] == 'y') || (line[0] == 'Y')) {
            oware_store_reset_cpu(st);
            (void)oware_store_save(st, path);
        }
    }
}

static void ui_settings(oware_io_t *io, oware_store_t *st, const char *path) {
    char line[64];
    io->write_str(io,
        "\nGrand-slam rule: 0=NoCapture 1=Forbidden 2=OpponentKeeps 3=LeaveLast\n");
    if (!ui_prompt(io, "Pin variant (0-3): ", line, sizeof(line))) { return; }
    if ((line[0] >= '0') && (line[0] <= '3')) {
        st->grandslam_rule = (oware_grandslam_rule_t)(line[0] - '0');
        (void)oware_store_save(st, path);
        io->write_str(io, "Saved.\n");
    }
}

void oware_ui_run(oware_io_t *io, oware_store_t *store, const char *path) {
    for (;;) {
        char line[64];
        io->write_str(io,
            "\n== OWARE ==\n"
            "1) Play vs Computer\n"
            "2) Two-Player\n"
            "3) Records\n"
            "4) Settings\n"
            "5) Quit\n"
            "Choose: ");
        if (!io->read_line(io, line, sizeof(line))) {
            break;
        }
        switch (line[0]) {
            case '1': ui_do_vs_cpu(io, store, path);     break;
            case '2': ui_do_two_player(io, store, path); break;
            case '3': ui_show_records(io, store, path);  break;
            case '4': ui_settings(io, store, path);      break;
            case '5':
            case 'q':
            case 'Q':
                return;
            default:
                io->write_str(io, "Invalid choice.\n");
                break;
        }
    }
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test_oware_ui`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware ui: menu and match loop with store persistence

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 5: main.c (stdio wiring) + README + the `oware` binary

**Files:**
- Create: `main.c`
- Modify: `README.md`

- [ ] **Step 1: Write main.c**

Create `main.c`:

```c
#include "oware_ui.h"
#include "oware_store.h"
#include <stdio.h>
#include <string.h>

static bool stdio_read(oware_io_t *io, char *buf, size_t cap) {
    (void)io;
    if (fgets(buf, (int)cap, stdin) == NULL) {
        return false;
    }
    size_t len = strlen(buf);
    while ((len > 0u) && ((buf[len - 1u] == '\n') || (buf[len - 1u] == '\r'))) {
        buf[len - 1u] = '\0';
        len--;
    }
    return true;
}

static void stdio_write(oware_io_t *io, const char *s) {
    (void)io;
    (void)fputs(s, stdout);
    (void)fflush(stdout);
}

int main(void) {
    char path[512];
    (void)oware_store_default_path(path, sizeof(path));

    oware_store_t store;
    (void)oware_store_load(&store, path); /* false on first run is fine */

    oware_io_t io;
    io.read_line = stdio_read;
    io.write_str = stdio_write;
    io.ctx = NULL;

    oware_ui_run(&io, &store, path);
    (void)oware_store_save(&store, path);
    (void)fputs("Bye.\n", stdout);
    return 0;
}
```

- [ ] **Step 2: Build the full app and run a scripted smoke session**

Run:
```bash
make clean && make all && printf '1\n1\nq\n5\n' | ZAKO_OWARE_HOME=/tmp/oware_smoke ./oware
```
Expected: builds `oware` with zero warnings; the run prints the menu and a board, plays one quick vs-CPU game (you quit it), and exits with "Bye." A stats file appears at `/tmp/oware_smoke/oware.dat`.

- [ ] **Step 3: Verify the full test suite still passes**

Run: `make clean && make test`
Expected: all four suites pass (engine `64021`, AI `6920`, store `117`, UI suite green); ASan/UBSan clean; `-Werror` clean.

- [ ] **Step 4: Document in README**

Append to `README.md`:

```markdown

## oware (app)

The playable terminal game. Build and run:

```bash
make            # builds liboware-*.a and the `oware` binary
./oware         # play: vs Computer (Easy/Medium/Hard) or local 2-player
```

Numeric board, keys 1-6 to sow, `q` to concede a game. Pinned rule variant and
win/loss records persist to `$ZAKO_OWARE_HOME/oware.dat` (default
`$HOME/.local/share/zako-oware/oware.dat`). UI logic is driven through an
injected `oware_io_t`, so the whole menu+game loop is unit-tested without a tty.
```

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware ui: main.c stdio wiring, playable oware binary, README

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review (completed during planning)

- **Spec coverage (DESIGN.md §9):** numeric board render (T2); keypad 1-6 input, no touch (T1); vs-computer with difficulty + local 2-player with names (T4); message line for captures/illegal/AI-move (T3 driver writes prompts + "Illegal"/"Computer plays"); records view + reset (T4); settings pin grand-slam variant (T4); persistence load/save wired (T4/T5). Match/series: single games with persisted records implemented; multi-game best-of is deferred (noted below).
- **Placeholder scan:** none — every step has concrete code and commands.
- **Type consistency:** `oware_io_t` (read_line/write_str/ctx), `oware_match_cfg_t` fields, and the use of `oware_store_record_cpu`/`oware_store_record_pair`/`ui_result_for_side` are consistent across tasks. `house % 6u + 1u` key mapping matches `oware_ui_house_for_key`'s `base + (key-'1')`.
- **Testability:** the scripted `oware_io_t` lets Task 3/4 tests drive full games and menu navigation and assert on captured output + store mutations — no real tty.

## Scope notes / deferred (YAGNI for this plan)

- **Match/series best-of-N:** DESIGN §8/§10 mentions optional series. This plan ships single games with persisted ± records (the records ARE the running series tally). A best-of-N wrapper can be added later without touching engine/AI/store.
- **Settings:** pins the grand-slam variant (the headline configurable). Capture-rule and end-mode pinning use the same `oware_store` fields and can be added as further menu items identically if wanted.
- **Per-side AI difficulty / AI vs AI:** the driver supports `side_is_ai[2]` generally, but the menu only wires human-vs-CPU and human-vs-human, per spec.

This completes the 4-plan arc: engine → AI → store → UI = a playable Oware game.
```
