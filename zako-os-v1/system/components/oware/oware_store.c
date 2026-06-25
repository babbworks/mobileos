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
        (void)snprintf(out, OWARE_STORE_NAME_CAP, "ANON");
    }
}

void oware_store__sanitize_for_test(const char *in, char *out) {
    oware_store_sanitize(in, out);
}

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
    (void)snprintf(p->a, OWARE_STORE_NAME_CAP, "%s", a);
    (void)snprintf(p->b, OWARE_STORE_NAME_CAP, "%s", b);
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
                /* Names are copied verbatim: save() always writes them already
                   sanitized and normalized (strcmp(a,b) <= 0). A hand-edited
                   file with un-normalized names may key inconsistently. */
                (void)snprintf(p->a, OWARE_STORE_NAME_CAP, "%s", n1);
                (void)snprintf(p->b, OWARE_STORE_NAME_CAP, "%s", n2);
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
