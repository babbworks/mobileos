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
