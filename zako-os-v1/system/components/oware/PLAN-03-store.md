# Oware Store — Implementation Plan (Plan 3 of 4)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Build `oware_store` — persistence for pinned rule settings, vs-computer ± records (per difficulty), and per-named-pair head-to-head records, as a small line-based file. Pure C99, no dynamic allocation, fully unit-tested host-side.

**Architecture:** A fixed-size `oware_store_t` struct (settings + 3 CPU records + a bounded array of 64 pair records). Reuses `oware_engine.h` rule enums and `oware_ai.h` difficulty enum. File I/O confined to `oware_store_load`/`oware_store_save`; everything else is pure struct manipulation. Names are sanitized (uppercase alnum, ≤15 chars) and pair keys normalized to a stable order so lookups are order-independent.

**Tech Stack:** C99, GNU Make, gcc/clang `-Wall -Wextra -Wpedantic -Werror`, ASan+UBSan test build. Uses libc `<stdio.h>` (fopen/fgets/fprintf/sscanf), `<stdlib.h>` (getenv), `<string.h>`, `<sys/stat.h>` (mkdir).

See `DESIGN.md §7` for the spec. All paths relative to `zako-os-v1/system/components/oware/`.

## File Structure

| File | Responsibility |
|------|----------------|
| `oware_store.h` | `oware_store_t`, record structs, result enum, function declarations |
| `oware_store.c` | init, default path, sanitize/normalize, CPU + pair record updates, save, load |
| `test_oware_store.c` | unit tests; own `main()` |
| `Makefile` | extend: add `liboware-store.a` + `test_oware_store` |

---

### Task 1: Scaffold — header, Makefile, init, default path

**Files:**
- Create: `oware_store.h`
- Create: `oware_store.c`
- Create: `test_oware_store.c`
- Modify: `Makefile`

- [ ] **Step 1: Write the header**

Create `oware_store.h`:

```c
#ifndef OWARE_STORE_H
#define OWARE_STORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "oware_engine.h"   /* rule enums */
#include "oware_ai.h"       /* oware_ai_difficulty_t */

#define OWARE_STORE_MAX_PAIRS 64u
#define OWARE_STORE_NAME_CAP  16u   /* buffer size: up to 15 chars + NUL */

typedef struct {
    uint32_t wins;    /* human wins */
    uint32_t losses;  /* human losses */
    uint32_t draws;
} oware_record_t;

typedef struct {
    char     a[OWARE_STORE_NAME_CAP]; /* normalized so strcmp(a,b) <= 0 */
    char     b[OWARE_STORE_NAME_CAP];
    uint32_t wins_a;
    uint32_t wins_b;
    uint32_t draws;
} oware_pair_record_t;

typedef struct {
    oware_grandslam_rule_t grandslam_rule;
    oware_capture_rule_t   capture_rule;
    oware_end_mode_t       end_mode;
    uint8_t                target_score;
    oware_record_t         cpu[3]; /* indexed by oware_ai_difficulty_t */
    oware_pair_record_t    pairs[OWARE_STORE_MAX_PAIRS];
    uint8_t                pair_count;
} oware_store_t;

typedef enum {
    OWARE_GAME_LOSS = 0,
    OWARE_GAME_WIN  = 1,
    OWARE_GAME_DRAW = 2
} oware_game_result_t;

void  oware_store_init(oware_store_t *st);

/* Returns a file path. Uses $ZAKO_OWARE_HOME (a directory) if set and non-empty,
   else $HOME/.local/share/zako-oware. Writes into buf and returns buf. */
const char *oware_store_default_path(char *buf, size_t cap);

/* Load from path. On missing/unreadable file, leaves *st at defaults and
   returns false (normal on first run). Malformed lines are skipped. */
bool  oware_store_load(oware_store_t *st, const char *path);

/* Save to path, creating parent directories if needed. False on I/O error. */
bool  oware_store_save(const oware_store_t *st, const char *path);

/* Record a vs-CPU game result (human's perspective) at a difficulty. */
void  oware_store_record_cpu(oware_store_t *st, oware_ai_difficulty_t d,
                             oware_game_result_t human_result);
void  oware_store_reset_cpu(oware_store_t *st);

/* Find-or-create the head-to-head record for a name pair (order-independent).
   Returns NULL only if the table is full and the pair is new. */
oware_pair_record_t *oware_store_pair(oware_store_t *st,
                                      const char *name1, const char *name2);

/* Record a 2-player result from name1's perspective. False if the pair table
   was full and the pair could not be created. */
bool  oware_store_record_pair(oware_store_t *st, const char *name1,
                              const char *name2, oware_game_result_t name1_result);

#endif /* OWARE_STORE_H */
```

- [ ] **Step 2: Extend the Makefile**

In `Makefile`, after the AI definitions add `LIB_STORE` and `STORE_TEST`, and wire them into `all`, `test`, and `clean`. Apply these edits:

Change the variables block to add:
```make
LIB_STORE := liboware-store.a
STORE_TEST := test_oware_store
```

Change `all:` to:
```make
all: $(LIB) $(LIB_AI) $(LIB_STORE)
```

Add build rules (next to the AI ones):
```make
$(LIB_STORE): oware_store.o
	$(AR) rcs $@ $^

oware_store.o: oware_store.c oware_store.h oware_engine.h oware_ai.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
```

Change `test:` to:
```make
test: $(ENGINE_TEST) $(AI_TEST) $(STORE_TEST)
	./$(ENGINE_TEST)
	./$(AI_TEST)
	./$(STORE_TEST)
```

Add the store test build rule:
```make
$(STORE_TEST): test_oware_store.c oware_store.c oware_engine.c oware_ai.c oware_store.h oware_engine.h oware_ai.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_store.c oware_store.c oware_engine.c oware_ai.c
```

Change `clean:` to:
```make
clean:
	rm -f *.o $(LIB) $(LIB_AI) $(LIB_STORE) $(ENGINE_TEST) $(AI_TEST) $(STORE_TEST)
```

Add `test_oware_store` to `.gitignore`.

- [ ] **Step 3: Write the failing test for init + default path**

Create `test_oware_store.c`:

```c
#include "oware_store.h"
#include "oware_test.h"
#include <stdlib.h>
#include <string.h>

/* test hook: defined in oware_store.c, not exposed in the header */
void oware_store__sanitize_for_test(const char *in, char *out);

static void test_init_defaults(void) {
    oware_store_t st;
    oware_store_init(&st);
    CHECK(st.grandslam_rule == OWARE_GS_NO_CAPTURE);
    CHECK(st.capture_rule == OWARE_CAP_STANDARD);
    CHECK(st.end_mode == OWARE_END_FIRST_TO_N);
    CHECK(st.target_score == 25u);
    CHECK(st.pair_count == 0u);
    CHECK(st.cpu[0].wins == 0u && st.cpu[2].draws == 0u);
}

static void test_default_path_env(void) {
    char buf[256];
    setenv("ZAKO_OWARE_HOME", "/tmp/zako-oware-test", 1);
    oware_store_default_path(buf, sizeof(buf));
    CHECK(strcmp(buf, "/tmp/zako-oware-test/oware.dat") == 0);
    unsetenv("ZAKO_OWARE_HOME");
}

static void test_default_path_home(void) {
    char buf[256];
    unsetenv("ZAKO_OWARE_HOME");
    setenv("HOME", "/home/tester", 1);
    oware_store_default_path(buf, sizeof(buf));
    CHECK(strcmp(buf, "/home/tester/.local/share/zako-oware/oware.dat") == 0);
}

static void test_sanitize(void) {
    char out[OWARE_STORE_NAME_CAP];
    oware_store__sanitize_for_test("Kofi Mensah!", out);
    CHECK(strcmp(out, "KOFIMENSAH") == 0);          /* upper, alnum only */
    oware_store__sanitize_for_test("abcdefghijklmnopqrstuvwxyz", out);
    CHECK(strlen(out) == 15u);                       /* truncated to 15 */
    oware_store__sanitize_for_test("", out);
    CHECK(strcmp(out, "ANON") == 0);                 /* empty -> fallback */
}

int main(void) {
    test_init_defaults();
    test_default_path_env();
    test_default_path_home();
    test_sanitize();
    TEST_REPORT();
}
```

- [ ] **Step 4: Run to verify it fails**

Run: `make test`
Expected: FAIL — link errors for `oware_store_init`, `oware_store_default_path`, `oware_store__sanitize_for_test`.

- [ ] **Step 5: Implement init, default path, sanitize**

Create `oware_store.c`:

```c
#include "oware_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void oware_store_init(oware_store_t *st) {
    st->grandslam_rule = OWARE_GS_NO_CAPTURE;
    st->capture_rule   = OWARE_CAP_STANDARD;
    st->end_mode       = OWARE_END_FIRST_TO_N;
    st->target_score   = 25u;
    for (size_t i = 0; i < 3u; i++) {
        st->cpu[i].wins = 0u;
        st->cpu[i].losses = 0u;
        st->cpu[i].draws = 0u;
    }
    for (size_t i = 0; i < OWARE_STORE_MAX_PAIRS; i++) {
        st->pairs[i].a[0] = '\0';
        st->pairs[i].b[0] = '\0';
        st->pairs[i].wins_a = 0u;
        st->pairs[i].wins_b = 0u;
        st->pairs[i].draws = 0u;
    }
    st->pair_count = 0u;
}

const char *oware_store_default_path(char *buf, size_t cap) {
    const char *home = getenv("ZAKO_OWARE_HOME");
    if ((home != NULL) && (home[0] != '\0')) {
        (void)snprintf(buf, cap, "%s/oware.dat", home);
    } else {
        const char *h = getenv("HOME");
        if (h == NULL) { h = "."; }
        (void)snprintf(buf, cap, "%s/.local/share/zako-oware/oware.dat", h);
    }
    return buf;
}

static void oware_store_sanitize(const char *in, char *out) {
    size_t o = 0u;
    if (in != NULL) {
        for (size_t i = 0u; (in[i] != '\0') && ((o + 1u) < OWARE_STORE_NAME_CAP); i++) {
            char c = in[i];
            if ((c >= 'a') && (c <= 'z')) {
                c = (char)((c - 'a') + 'A');
            }
            if (((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9'))) {
                out[o] = c;
                o++;
            }
        }
    }
    out[o] = '\0';
    if (o == 0u) {
        (void)strcpy(out, "ANON");
    }
}

void oware_store__sanitize_for_test(const char *in, char *out) {
    oware_store_sanitize(in, out);
}
```

- [ ] **Step 6: Run to verify it passes**

Run: `make test`
Expected: PASS (all three suites; store suite `N/N checks passed`).

- [ ] **Step 7: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware store: scaffold header, Makefile, init, default path, sanitize

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: vs-CPU records

**Files:**
- Modify: `oware_store.c`
- Modify: `test_oware_store.c`

- [ ] **Step 1: Add failing tests**

Append and call from `main()`:

```c
static void test_record_cpu(void) {
    oware_store_t st;
    oware_store_init(&st);
    oware_store_record_cpu(&st, OWARE_AI_MEDIUM, OWARE_GAME_WIN);
    oware_store_record_cpu(&st, OWARE_AI_MEDIUM, OWARE_GAME_WIN);
    oware_store_record_cpu(&st, OWARE_AI_MEDIUM, OWARE_GAME_LOSS);
    oware_store_record_cpu(&st, OWARE_AI_HARD,   OWARE_GAME_DRAW);
    CHECK(st.cpu[OWARE_AI_MEDIUM].wins == 2u);
    CHECK(st.cpu[OWARE_AI_MEDIUM].losses == 1u);
    CHECK(st.cpu[OWARE_AI_HARD].draws == 1u);
    CHECK(st.cpu[OWARE_AI_EASY].wins == 0u);
}

static void test_reset_cpu(void) {
    oware_store_t st;
    oware_store_init(&st);
    oware_store_record_cpu(&st, OWARE_AI_EASY, OWARE_GAME_WIN);
    oware_store_reset_cpu(&st);
    CHECK(st.cpu[OWARE_AI_EASY].wins == 0u);
    CHECK(st.cpu[OWARE_AI_MEDIUM].losses == 0u);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test`
Expected: FAIL — undefined `oware_store_record_cpu`, `oware_store_reset_cpu`.

- [ ] **Step 3: Implement**

Add to `oware_store.c`:

```c
void oware_store_record_cpu(oware_store_t *st, oware_ai_difficulty_t d,
                            oware_game_result_t human_result) {
    int i = (int)d;
    if ((i < 0) || (i > 2)) { return; }
    switch (human_result) {
        case OWARE_GAME_WIN:  st->cpu[i].wins++;   break;
        case OWARE_GAME_LOSS: st->cpu[i].losses++; break;
        case OWARE_GAME_DRAW: st->cpu[i].draws++;  break;
        default: break;
    }
}

void oware_store_reset_cpu(oware_store_t *st) {
    for (size_t i = 0u; i < 3u; i++) {
        st->cpu[i].wins = 0u;
        st->cpu[i].losses = 0u;
        st->cpu[i].draws = 0u;
    }
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware store: vs-CPU win/loss/draw records and reset

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Head-to-head pair records

**Files:**
- Modify: `oware_store.c`
- Modify: `test_oware_store.c`

- [ ] **Step 1: Add failing tests**

Append and call from `main()`:

```c
static void test_pair_order_independent(void) {
    oware_store_t st;
    oware_store_init(&st);
    oware_pair_record_t *p1 = oware_store_pair(&st, "Kofi", "Abena");
    oware_pair_record_t *p2 = oware_store_pair(&st, "abena", "KOFI");
    CHECK(p1 != NULL);
    CHECK(p1 == p2);                 /* same record regardless of order/case */
    CHECK(st.pair_count == 1u);
    CHECK(strcmp(p1->a, "ABENA") == 0); /* normalized: a <= b */
    CHECK(strcmp(p1->b, "KOFI") == 0);
}

static void test_record_pair(void) {
    oware_store_t st;
    oware_store_init(&st);
    /* Kofi beats Abena, then Abena beats Kofi, then a draw */
    CHECK(oware_store_record_pair(&st, "Kofi", "Abena", OWARE_GAME_WIN));
    CHECK(oware_store_record_pair(&st, "Abena", "Kofi", OWARE_GAME_WIN));
    CHECK(oware_store_record_pair(&st, "Kofi", "Abena", OWARE_GAME_DRAW));
    oware_pair_record_t *p = oware_store_pair(&st, "Kofi", "Abena");
    /* a=ABENA, b=KOFI */
    CHECK(p->wins_a == 1u);          /* Abena's win */
    CHECK(p->wins_b == 1u);          /* Kofi's win */
    CHECK(p->draws == 1u);
    CHECK(st.pair_count == 1u);
}

static void test_pair_capacity(void) {
    oware_store_t st;
    oware_store_init(&st);
    char n1[8], n2[8];
    for (unsigned i = 0; i < OWARE_STORE_MAX_PAIRS; i++) {
        (void)snprintf(n1, sizeof(n1), "P%uA", i);
        (void)snprintf(n2, sizeof(n2), "P%uB", i);
        CHECK(oware_store_pair(&st, n1, n2) != NULL);
    }
    CHECK(st.pair_count == OWARE_STORE_MAX_PAIRS);
    CHECK(oware_store_pair(&st, "NEW", "PAIR") == NULL); /* table full */
}
```

Add `#include <stdio.h>` to `test_oware_store.c` for `snprintf`.

- [ ] **Step 2: Run to verify it fails**

Run: `make test`
Expected: FAIL — undefined `oware_store_pair`, `oware_store_record_pair`.

- [ ] **Step 3: Implement**

Add to `oware_store.c`:

```c
oware_pair_record_t *oware_store_pair(oware_store_t *st,
                                      const char *name1, const char *name2) {
    char s1[OWARE_STORE_NAME_CAP];
    char s2[OWARE_STORE_NAME_CAP];
    oware_store_sanitize(name1, s1);
    oware_store_sanitize(name2, s2);

    const char *a = s1;
    const char *b = s2;
    if (strcmp(s1, s2) > 0) {
        a = s2;
        b = s1;
    }
    for (uint8_t i = 0u; i < st->pair_count; i++) {
        if ((strcmp(st->pairs[i].a, a) == 0) && (strcmp(st->pairs[i].b, b) == 0)) {
            return &st->pairs[i];
        }
    }
    if (st->pair_count >= OWARE_STORE_MAX_PAIRS) {
        return NULL;
    }
    oware_pair_record_t *p = &st->pairs[st->pair_count];
    (void)strncpy(p->a, a, OWARE_STORE_NAME_CAP - 1u);
    p->a[OWARE_STORE_NAME_CAP - 1u] = '\0';
    (void)strncpy(p->b, b, OWARE_STORE_NAME_CAP - 1u);
    p->b[OWARE_STORE_NAME_CAP - 1u] = '\0';
    p->wins_a = 0u;
    p->wins_b = 0u;
    p->draws = 0u;
    st->pair_count++;
    return p;
}

bool oware_store_record_pair(oware_store_t *st, const char *name1,
                             const char *name2, oware_game_result_t name1_result) {
    char s1[OWARE_STORE_NAME_CAP];
    oware_store_sanitize(name1, s1);
    oware_pair_record_t *p = oware_store_pair(st, name1, name2);
    if (p == NULL) {
        return false;
    }
    if (name1_result == OWARE_GAME_DRAW) {
        p->draws++;
        return true;
    }
    bool name1_is_a = (strcmp(p->a, s1) == 0);
    bool name1_won = (name1_result == OWARE_GAME_WIN);
    bool winner_is_a = name1_won ? name1_is_a : (!name1_is_a);
    if (winner_is_a) {
        p->wins_a++;
    } else {
        p->wins_b++;
    }
    return true;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware store: head-to-head pair records with normalized keys

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Save to file

**Files:**
- Modify: `oware_store.c`
- Modify: `test_oware_store.c`

- [ ] **Step 1: Add failing test**

Append and call from `main()`:

```c
static void test_save_format(void) {
    oware_store_t st;
    oware_store_init(&st);
    st.grandslam_rule = OWARE_GS_FORBIDDEN;     /* 1 */
    st.capture_rule = OWARE_CAP_THREE_FOUR;     /* 1 */
    st.target_score = 30u;
    oware_store_record_cpu(&st, OWARE_AI_EASY, OWARE_GAME_WIN);
    (void)oware_store_record_pair(&st, "Kofi", "Abena", OWARE_GAME_WIN);

    const char *path = "/tmp/oware_store_save_test.dat";
    CHECK(oware_store_save(&st, path));

    FILE *f = fopen(path, "r");
    CHECK(f != NULL);
    char buf[1024];
    size_t n = fread(buf, 1u, sizeof(buf) - 1u, f);
    buf[n] = '\0';
    (void)fclose(f);
    CHECK(strstr(buf, "variant=1") != NULL);
    CHECK(strstr(buf, "capture=1") != NULL);
    CHECK(strstr(buf, "end=0 30") != NULL);
    CHECK(strstr(buf, "cpu easy 1 0 0") != NULL);
    CHECK(strstr(buf, "pair ABENA KOFI 0 1 0") != NULL); /* Kofi(b) won */
    (void)remove(path);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test`
Expected: FAIL — undefined `oware_store_save`.

- [ ] **Step 3: Implement save + mkparent**

Add to `oware_store.c`:

```c
static void oware_store_mkparent(const char *path) {
    char tmp[512];
    size_t len = strlen(path);
    if ((len + 1u) > sizeof(tmp)) { return; }
    (void)strcpy(tmp, path);
    char *slash = strrchr(tmp, '/');
    if (slash == NULL) { return; }      /* no directory component */
    *slash = '\0';
    for (char *p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            (void)mkdir(tmp, 0777);
            *p = '/';
        }
    }
    (void)mkdir(tmp, 0777);
}

bool oware_store_save(const oware_store_t *st, const char *path) {
    static const char *const diff_names[3] = { "easy", "medium", "hard" };
    oware_store_mkparent(path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        return false;
    }
    (void)fprintf(f, "# zako-oware store v1\n");
    (void)fprintf(f, "variant=%d\n", (int)st->grandslam_rule);
    (void)fprintf(f, "capture=%d\n", (int)st->capture_rule);
    (void)fprintf(f, "end=%d %u\n", (int)st->end_mode, (unsigned)st->target_score);
    for (size_t i = 0u; i < 3u; i++) {
        (void)fprintf(f, "cpu %s %u %u %u\n", diff_names[i],
                      (unsigned)st->cpu[i].wins,
                      (unsigned)st->cpu[i].losses,
                      (unsigned)st->cpu[i].draws);
    }
    for (uint8_t i = 0u; i < st->pair_count; i++) {
        (void)fprintf(f, "pair %s %s %u %u %u\n",
                      st->pairs[i].a, st->pairs[i].b,
                      (unsigned)st->pairs[i].wins_a,
                      (unsigned)st->pairs[i].wins_b,
                      (unsigned)st->pairs[i].draws);
    }
    return (fclose(f) == 0);
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware store: save to line-based file with parent dir creation

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 5: Load from file + round-trip

**Files:**
- Modify: `oware_store.c`
- Modify: `test_oware_store.c`

- [ ] **Step 1: Add failing tests**

Append and call from `main()`:

```c
static void test_load_roundtrip(void) {
    oware_store_t a;
    oware_store_init(&a);
    a.grandslam_rule = OWARE_GS_LEAVE_LAST;   /* 3 */
    a.target_score = 21u;
    oware_store_record_cpu(&a, OWARE_AI_HARD, OWARE_GAME_LOSS);
    (void)oware_store_record_pair(&a, "Ama", "Yaw", OWARE_GAME_WIN);

    const char *path = "/tmp/oware_store_rt_test.dat";
    CHECK(oware_store_save(&a, path));

    oware_store_t b;
    CHECK(oware_store_load(&b, path));
    CHECK(b.grandslam_rule == OWARE_GS_LEAVE_LAST);
    CHECK(b.target_score == 21u);
    CHECK(b.cpu[OWARE_AI_HARD].losses == 1u);
    CHECK(b.pair_count == 1u);
    oware_pair_record_t *p = oware_store_pair(&b, "Ama", "Yaw");
    CHECK(p->wins_a == 1u);                    /* AMA <= YAW, Ama won */
    (void)remove(path);
}

static void test_load_missing_file(void) {
    oware_store_t st;
    /* deliberately dirty it first to prove load resets to defaults */
    oware_store_init(&st);
    st.target_score = 99u;
    CHECK(!oware_store_load(&st, "/tmp/oware_definitely_missing_98765.dat"));
    CHECK(st.target_score == 25u);             /* reset to defaults */
}

static void test_load_skips_malformed(void) {
    const char *path = "/tmp/oware_store_bad_test.dat";
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    (void)fputs("# zako-oware store v1\n", f);
    (void)fputs("garbage line that means nothing\n", f);
    (void)fputs("variant=2\n", f);
    (void)fputs("cpu medium 5 3 1\n", f);
    (void)fputs("pair ZID FROD 7 2 0\n", f);
    (void)fputs("\n", f);
    (void)fclose(f);

    oware_store_t st;
    CHECK(oware_store_load(&st, path));
    CHECK(st.grandslam_rule == OWARE_GS_OPPONENT_KEEPS); /* 2 */
    CHECK(st.cpu[OWARE_AI_MEDIUM].wins == 5u);
    CHECK(st.pair_count == 1u);
    CHECK(st.pairs[0].wins_a == 7u);
    (void)remove(path);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test`
Expected: FAIL — undefined `oware_store_load`.

- [ ] **Step 3: Implement load**

Add to `oware_store.c`:

```c
bool oware_store_load(oware_store_t *st, const char *path) {
    oware_store_init(st);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return false;
    }
    char line[256];
    while (fgets(line, (int)sizeof(line), f) != NULL) {
        int iv = 0;
        unsigned u1 = 0u;
        unsigned u2 = 0u;
        unsigned u3 = 0u;
        char word[32];
        char n1[OWARE_STORE_NAME_CAP];
        char n2[OWARE_STORE_NAME_CAP];

        if ((line[0] == '#') || (line[0] == '\n') || (line[0] == '\0')) {
            continue;
        }
        if (sscanf(line, "variant=%d", &iv) == 1) {
            if ((iv >= 0) && (iv <= 3)) {
                st->grandslam_rule = (oware_grandslam_rule_t)iv;
            }
        } else if (sscanf(line, "capture=%d", &iv) == 1) {
            if ((iv == 0) || (iv == 1)) {
                st->capture_rule = (oware_capture_rule_t)iv;
            }
        } else if (sscanf(line, "end=%d %u", &iv, &u1) == 2) {
            if ((iv == 0) || (iv == 1)) {
                st->end_mode = (oware_end_mode_t)iv;
            }
            if (u1 <= 48u) {
                st->target_score = (uint8_t)u1;
            }
        } else if (sscanf(line, "cpu %31s %u %u %u", word, &u1, &u2, &u3) == 4) {
            int idx = -1;
            if (strcmp(word, "easy") == 0) { idx = 0; }
            else if (strcmp(word, "medium") == 0) { idx = 1; }
            else if (strcmp(word, "hard") == 0) { idx = 2; }
            if (idx >= 0) {
                st->cpu[idx].wins = u1;
                st->cpu[idx].losses = u2;
                st->cpu[idx].draws = u3;
            }
        } else if (sscanf(line, "pair %15s %15s %u %u %u", n1, n2, &u1, &u2, &u3) == 5) {
            if (st->pair_count < OWARE_STORE_MAX_PAIRS) {
                oware_pair_record_t *p = &st->pairs[st->pair_count];
                (void)strncpy(p->a, n1, OWARE_STORE_NAME_CAP - 1u);
                p->a[OWARE_STORE_NAME_CAP - 1u] = '\0';
                (void)strncpy(p->b, n2, OWARE_STORE_NAME_CAP - 1u);
                p->b[OWARE_STORE_NAME_CAP - 1u] = '\0';
                p->wins_a = u1;
                p->wins_b = u2;
                p->draws = u3;
                st->pair_count++;
            }
        } else {
            /* unknown / malformed line: skip */
        }
    }
    (void)fclose(f);
    return true;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware store: tolerant line-based load with round-trip tests

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 6: README + final build

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Document the store in the component README**

Append to `README.md`:

```markdown

## oware store

Persistence for pinned rule settings, vs-computer ± records (per difficulty),
and per-named-pair head-to-head records, in a tolerant line-based file
(`$ZAKO_OWARE_HOME/oware.dat` or `$HOME/.local/share/zako-oware/oware.dat`).
See `oware_store.h`. Names are sanitized (uppercase alnum, ≤15 chars); pair keys
are order-independent; the table holds up to 64 pairs (no dynamic allocation).
```

- [ ] **Step 2: Full clean build + test**

Run: `make clean && make && make test`
Expected: `liboware-store.a` builds; all three suites pass; zero warnings under `-Werror`; ASan/UBSan clean.

- [ ] **Step 3: Commit**

```bash
git add zako-os-v1/system/components/oware/
git commit -m "oware store: document store in README

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review (completed during planning)

- **Spec coverage (DESIGN.md §7):** pinned settings (variant/capture/end/target) persisted (T1,T4,T5); vs-CPU ± per difficulty + reset (T2); per-pair head-to-head with order-independent normalized keys (T3); fixed 64-pair capacity, no heap (T3); line-based file format (T4); tolerant load skipping malformed lines, missing-file → defaults (T5); env-overridable path (T1). All covered.
- **Placeholder scan:** none — every step has concrete code and exact commands.
- **Type consistency:** reuses `oware_ai_difficulty_t` (`OWARE_AI_EASY/MEDIUM/HARD`) for CPU indexing and engine rule enums for settings; `oware_game_result_t` (`OWARE_GAME_WIN/LOSS/DRAW`) used consistently across `record_cpu` and `record_pair`; `oware_store__sanitize_for_test` mirrors the engine/AI hidden-test-hook pattern.
- **Decisions:** names stored as single uppercase alnum tokens (no quoting) — simpler, robust `sscanf` parsing, at the cost of not preserving spaces/case in display names (acceptable for a scoreboard; documented). This is a deliberate simplification of DESIGN.md §7's quoted-name sketch.

## Next plan

- **Plan 4 — `oware_ui` + `main`:** terminal numeric board, input, menus, vs-CPU / 2-player modes, match/series flow — wires the engine, AI, and this store into the playable app.
```
