#define _POSIX_C_SOURCE 200112L
#include "oware_store.h"
#include "oware_test.h"
#include <stdio.h>
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

int main(void) {
    test_init_defaults();
    test_default_path_env();
    test_default_path_home();
    test_sanitize();
    test_record_cpu();
    test_reset_cpu();
    test_pair_order_independent();
    test_record_pair();
    test_pair_capacity();
    test_save_format();
    test_load_roundtrip();
    test_load_missing_file();
    test_load_skips_malformed();
    TEST_REPORT();
}
