#include "oware_ai.h"
#include "oware_test.h"
#include <string.h>
#include <stdlib.h>

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
    TEST_REPORT();
}
