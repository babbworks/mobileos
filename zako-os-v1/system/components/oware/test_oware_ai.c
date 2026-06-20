#include "oware_ai.h"
#include "oware_test.h"
#include <string.h>

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
    /* s.turn == 0; ask AI to move for player 1 */
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

int main(void) {
    test_config_default();
    test_choose_move_wrong_turn();
    test_choose_move_no_legal();
    TEST_REPORT();
}
