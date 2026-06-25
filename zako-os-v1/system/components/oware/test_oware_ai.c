#include "oware_ai.h"
#include "oware_test.h"
#include <string.h>
#include <stdlib.h>

/* test hook implemented in oware_ai.c */
int oware_ai_eval_for_test(const oware_state_t *s, const oware_rules_t *r,
                           uint8_t player);

static void test_config_default(void) {
    oware_ai_config_t cfg;
    oware_ai_config_default(&cfg, OWARE_AI_MEDIUM);
    CHECK(cfg.difficulty == OWARE_AI_MEDIUM);
    CHECK(cfg.rng_seed == 1u);
}

static void test_choose_move_wrong_turn(void) {
    oware_state_t s;
    oware_rules_t r;
    oware_ai_config_t cfg;
    oware_init(&s);
    oware_rules_default(&r);
    oware_ai_config_default(&cfg, OWARE_AI_EASY);
    uint8_t house = 99u;
    CHECK(!oware_ai_choose_move(&s, &r, &cfg, 1u, &house));
}

static void test_choose_move_no_legal(void) {
    oware_state_t s;
    oware_rules_t r;
    oware_ai_config_t cfg;
    oware_init(&s);
    oware_rules_default(&r);
    oware_ai_config_default(&cfg, OWARE_AI_MEDIUM);
    memset(s.houses, 0, sizeof(s.houses));
    s.turn = 0u;
    uint8_t house = 99u;
    CHECK(!oware_ai_choose_move(&s, &r, &cfg, 0u, &house));
}

static void test_eval_material(void) {
    oware_state_t s;
    oware_rules_t r;
    oware_init(&s);
    oware_rules_default(&r);
    s.score[0] = 10u;
    s.score[1] = 4u;
    CHECK(oware_ai_eval_for_test(&s, &r, 0u) > oware_ai_eval_for_test(&s, &r, 1u));
}

static void test_eval_attack(void) {
    oware_state_t s;
    oware_rules_t r;
    oware_init(&s);
    oware_rules_default(&r);
    memset(s.houses, 0, sizeof(s.houses));
    int base = oware_ai_eval_for_test(&s, &r, 0u);
    s.houses[6] = 2u;
    CHECK(oware_ai_eval_for_test(&s, &r, 0u) > base);
}

static void test_eval_vulnerability(void) {
    oware_state_t s;
    oware_rules_t r;
    oware_init(&s);
    oware_rules_default(&r);
    memset(s.houses, 0, sizeof(s.houses));
    int base = oware_ai_eval_for_test(&s, &r, 0u);
    s.houses[2] = 2u;
    CHECK(oware_ai_eval_for_test(&s, &r, 0u) < base);
}

static void test_obvious_capture(void) {
    oware_state_t s;
    oware_rules_t r;
    oware_ai_config_t cfg;
    oware_init(&s);
    oware_rules_default(&r);
    oware_ai_config_default(&cfg, OWARE_AI_MEDIUM);
    memset(s.houses, 0, sizeof(s.houses));
    s.turn = 0u;
    s.houses[5] = 1u;
    s.houses[6] = 2u;
    s.houses[7] = 1u;
    s.houses[0] = 3u;
    uint8_t house = 99u;
    CHECK(oware_ai_choose_move(&s, &r, &cfg, 0u, &house));
    CHECK(house == 5u);
}

static void test_blunder_avoidance(void) {
    oware_state_t s;
    oware_rules_t r;
    oware_ai_config_t cfg;
    oware_init(&s);
    oware_rules_default(&r);
    oware_ai_config_default(&cfg, OWARE_AI_MEDIUM);
    memset(s.houses, 0, sizeof(s.houses));
    s.turn = 0u;
    s.houses[0] = 2u;
    s.houses[1] = 2u;
    s.houses[5] = 1u;
    s.houses[6] = 2u;
    s.houses[7] = 1u;
    s.houses[11] = 1u;
    uint8_t house = 99u;
    CHECK(oware_ai_choose_move(&s, &r, &cfg, 0u, &house));
    CHECK(house == 0u);
}

static void test_faster_win(void) {
    oware_state_t s;
    oware_rules_t r;
    oware_ai_config_t cfg;
    oware_init(&s);
    oware_rules_default(&r);
    oware_ai_config_default(&cfg, OWARE_AI_MEDIUM);
    memset(s.houses, 0, sizeof(s.houses));
    s.turn = 0u;
    s.score[0] = 22u;
    s.houses[5] = 1u;
    s.houses[6] = 2u;
    s.houses[7] = 1u;
    s.houses[0] = 4u;
    uint8_t house = 99u;
    CHECK(oware_ai_choose_move(&s, &r, &cfg, 0u, &house));
    CHECK(house == 5u);
}

static void test_easy_deterministic(void) {
    oware_state_t s;
    oware_rules_t r;
    oware_ai_config_t cfg;
    oware_init(&s);
    oware_rules_default(&r);
    oware_ai_config_default(&cfg, OWARE_AI_EASY);
    cfg.rng_seed = 42u;
    uint8_t a = 99u;
    uint8_t b = 99u;
    CHECK(oware_ai_choose_move(&s, &r, &cfg, 0u, &a));
    CHECK(oware_ai_choose_move(&s, &r, &cfg, 0u, &b));
    CHECK(a == b);
}

static void test_hard_avoids_blunder(void) {
    oware_state_t s;
    oware_rules_t r;
    oware_ai_config_t cfg;
    oware_init(&s);
    oware_rules_default(&r);
    oware_ai_config_default(&cfg, OWARE_AI_HARD);
    memset(s.houses, 0, sizeof(s.houses));
    s.turn = 0u;
    s.houses[0] = 2u;
    s.houses[1] = 2u;
    s.houses[5] = 1u;
    s.houses[6] = 2u;
    s.houses[7] = 1u;
    s.houses[11] = 1u;
    uint8_t house = 99u;
    CHECK(oware_ai_choose_move(&s, &r, &cfg, 0u, &house));
    CHECK(house == 0u);
}

static void test_hard_finds_capture(void) {
    oware_state_t s;
    oware_rules_t r;
    oware_ai_config_t cfg;
    oware_init(&s);
    oware_rules_default(&r);
    oware_ai_config_default(&cfg, OWARE_AI_HARD);
    memset(s.houses, 0, sizeof(s.houses));
    s.turn = 0u;
    s.houses[5] = 1u;
    s.houses[6] = 2u;
    s.houses[7] = 1u;
    s.houses[0] = 3u;
    uint8_t house = 99u;
    CHECK(oware_ai_choose_move(&s, &r, &cfg, 0u, &house));
    CHECK(house == 5u);
}

static void test_always_legal(void) {
    oware_rules_t r;
    oware_rules_default(&r);
    for (int trial = 0; trial < 100; trial++) {
        oware_state_t s;
        oware_ai_config_t cfg;
        oware_init(&s);
        srand((unsigned)trial + 1u);
        for (int scramble = 0; scramble < 20; scramble++) {
            uint8_t mv[OWARE_SIDE];
            int n = oware_legal_moves(&s, &r, mv);
            if (n == 0) {
                break;
            }
            oware_move_result_t mr;
            CHECK(oware_apply_move(&s, &r, mv[rand() % n], &mr));
        }

        for (int diff = 0; diff < 3; diff++) {
            for (uint8_t side = 0u; side < 2u; side++) {
                uint8_t mv[OWARE_SIDE];
                if (oware_legal_moves(&s, &r, mv) == 0) {
                    continue;
                }
                if (s.turn != side) {
                    continue;
                }
                oware_ai_config_default(&cfg, (oware_ai_difficulty_t)diff);
                cfg.rng_seed = (uint32_t)(trial + 1);
                uint8_t house = 99u;
                if (oware_ai_choose_move(&s, &r, &cfg, side, &house)) {
                    CHECK(oware_is_legal(&s, &r, house));
                }
            }
        }
    }
}

/* Verify the AI never returns an illegal move under each non-default rule
   config, for all three difficulties.  Uses a fixed seed so it is fully
   deterministic.  Covers: OWARE_GS_FORBIDDEN, OWARE_GS_OPPONENT_KEEPS,
   OWARE_GS_LEAVE_LAST, and OWARE_CAP_THREE_FOUR (in addition to the
   default config already exercised by test_always_legal). */
static void test_always_legal_rule_variants(void) {
    /* Five rule configurations: default followed by the four non-default ones */
    oware_rules_t configs[5];

    /* 0: default */
    oware_rules_default(&configs[0]);

    /* 1: grandslam forbidden */
    oware_rules_default(&configs[1]);
    configs[1].grandslam_rule = OWARE_GS_FORBIDDEN;

    /* 2: grandslam opponent keeps remaining board seeds */
    oware_rules_default(&configs[2]);
    configs[2].grandslam_rule = OWARE_GS_OPPONENT_KEEPS;

    /* 3: grandslam leave last in chain */
    oware_rules_default(&configs[3]);
    configs[3].grandslam_rule = OWARE_GS_LEAVE_LAST;

    /* 4: three-four capture rule */
    oware_rules_default(&configs[4]);
    configs[4].capture_rule = OWARE_CAP_THREE_FOUR;

    for (int ci = 0; ci < 5; ci++) {
        oware_rules_t *r = &configs[ci];
        for (int trial = 0; trial < 40; trial++) {
            oware_state_t s;
            oware_ai_config_t cfg;
            oware_init(&s);
            /* deterministic scramble seeded by (config_index, trial) */
            srand((unsigned)(ci * 100 + trial + 1));
            for (int scramble = 0; scramble < 20; scramble++) {
                oware_result_t over;
                if (oware_is_over(&s, r, &over)) { break; }
                uint8_t mv[OWARE_SIDE];
                int n = oware_legal_moves(&s, r, mv);
                if (n == 0) { break; }
                oware_move_result_t mr;
                CHECK(oware_apply_move(&s, r, mv[rand() % n], &mr));
            }

            oware_result_t over;
            if (oware_is_over(&s, r, &over)) { continue; }

            for (int diff = 0; diff < 3; diff++) {
                uint8_t mv[OWARE_SIDE];
                if (oware_legal_moves(&s, r, mv) == 0) { continue; }
                if (s.turn != 0u) { continue; } /* always player 0's turn to query */
                oware_ai_config_default(&cfg, (oware_ai_difficulty_t)diff);
                cfg.rng_seed = (uint32_t)(ci * 100 + trial + 1);
                uint8_t house = 99u;
                if (oware_ai_choose_move(&s, r, &cfg, 0u, &house)) {
                    CHECK(oware_is_legal(&s, r, house));
                }
            }
        }
    }
}

int main(void) {
    test_config_default();
    test_choose_move_wrong_turn();
    test_choose_move_no_legal();
    test_eval_material();
    test_eval_attack();
    test_eval_vulnerability();
    test_obvious_capture();
    test_blunder_avoidance();
    test_faster_win();
    test_easy_deterministic();
    test_hard_avoids_blunder();
    test_hard_finds_capture();
    test_always_legal();
    test_always_legal_rule_variants();
    TEST_REPORT();
}
