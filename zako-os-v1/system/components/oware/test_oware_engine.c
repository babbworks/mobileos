#include "oware_engine.h"
#include "oware_test.h"
#include <string.h>

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

static void test_capture_simple(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    /* craft: player 0 to move; landing makes an opponent house exactly 3 */
    memset(s.houses, 0, sizeof(s.houses));
    s.houses[5] = 1u;   /* play this 1 seed -> lands in house 6 */
    s.houses[6] = 2u;   /* becomes 3 -> captured */
    s.houses[7] = 1u;   /* extra seed so this is not a grand slam */
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
    s.houses[9] = 1u;   /* extra seed so this is not a grand slam */
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

static void test_capture_three_four(void) {
    oware_state_t s; oware_rules_t r;
    oware_init(&s); oware_rules_default(&r);
    r.capture_rule = OWARE_CAP_THREE_FOUR;
    memset(s.houses, 0, sizeof(s.houses));
    s.houses[5] = 1u;
    s.houses[6] = 3u;   /* ->4 captured under {3,4}; NOT under {2,3} */
    s.houses[7] = 1u;   /* extra seed so this is not a grand slam */
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
    s.houses[7] = 1u;  /* extra seed so not a grand slam */
    oware_move_result_t res;
    CHECK(oware_apply_move(&s, &r, 5u, &res));
    CHECK(res.captured == 3u);
    CHECK(s.no_capture_plies == 0u);     /* reset on capture */
    CHECK(s.score[0] == 3u);
}

int main(void) {
    test_init();
    test_ownership();
    test_rules_default();
    test_sow_basic(); test_sow_skip_origin();
    test_capture_simple(); test_capture_chained(); test_capture_stops_at_own_side();
    test_capture_three_four();
    test_grandslam_no_capture(); test_grandslam_opponent_keeps(); test_grandslam_leave_last();
    test_legal_moves_basic(); test_feeding_obligation(); test_forbidden_grandslam();
    test_apply_move(); test_apply_rejects_illegal(); test_apply_capture_resets_cycle();
    TEST_REPORT();
}
