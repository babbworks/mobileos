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

int main(void) {
    test_init();
    test_ownership();
    test_rules_default();
    test_sow_basic(); test_sow_skip_origin();
    TEST_REPORT();
}
