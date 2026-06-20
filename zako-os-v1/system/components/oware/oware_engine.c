#include "oware_engine.h"
#include <string.h>

void oware_rules_default(oware_rules_t *r) {
    r->capture_rule    = OWARE_CAP_STANDARD;
    r->grandslam_rule  = OWARE_GS_NO_CAPTURE;
    r->end_mode        = OWARE_END_FIRST_TO_N;
    r->target_score    = 25u;
    r->cycle_ply_limit = 100u;
    r->allow_agreed_end = true;
}

void oware_init(oware_state_t *s) {
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) {
        s->houses[i] = 4u;
    }
    s->score[0] = 0u;
    s->score[1] = 0u;
    s->turn = 0u;
    s->no_capture_plies = 0u;
}

bool oware_house_belongs_to(uint8_t house, uint8_t player) {
    if (house >= OWARE_HOUSES) {
        return false;
    }
    return (house < OWARE_SIDE) == (player == 0u);
}

int oware_side_seeds(const oware_state_t *s, uint8_t player) {
    int total = 0;
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) {
        if (oware_house_belongs_to(i, player)) {
            total += (int)s->houses[i];
        }
    }
    return total;
}

int oware_board_seeds(const oware_state_t *s) {
    int total = 0;
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) {
        total += (int)s->houses[i];
    }
    return total;
}

static bool oware_capturable(uint8_t count, oware_capture_rule_t rule) {
    switch (rule) {
        case OWARE_CAP_STANDARD:   return (count == 2u) || (count == 3u);
        case OWARE_CAP_THREE_FOUR: return (count == 3u) || (count == 4u);
        default:                   return false;
    }
}

/* Sow + capture. Returns false if 'house' is not a
   valid own, non-empty source. Computes resulting board into *out. */
static bool oware_simulate(const oware_state_t *s, const oware_rules_t *r,
                           uint8_t house, oware_state_t *out,
                           oware_move_result_t *res) {
    uint8_t p = s->turn;
    if (house >= OWARE_HOUSES) { return false; }
    if (!oware_house_belongs_to(house, p)) { return false; }
    if (s->houses[house] == 0u) { return false; }

    *out = *s;
    uint8_t seeds = out->houses[house];
    out->houses[house] = 0u;
    uint8_t i = house;
    uint8_t last_placed = house;
    while (seeds > 0u) {
        i = (uint8_t)((i + 1u) % OWARE_HOUSES);
        if (i == house) {
            seeds--;                             /* origin counts as a position but receives no seed */
            continue;
        }
        out->houses[i] = (uint8_t)(out->houses[i] + 1u);
        last_placed = i;
        seeds--;
    }

    res->landing = last_placed;
    res->captured = 0u;
    res->was_grand_slam = false;
    res->forced_feed = false;

    /* provisional capture chain: walk backward over opponent houses */
    uint8_t cap_idx[OWARE_SIDE];
    uint8_t cap_count = 0u;
    uint8_t j = last_placed;
    for (;;) {
        if (oware_house_belongs_to(j, p)) { break; }          /* own side */
        if (!oware_capturable(out->houses[j], r->capture_rule)) { break; }
        cap_idx[cap_count] = j;
        cap_count++;
        if (cap_count >= OWARE_SIDE) { break; }
        j = (j == 0u) ? (uint8_t)(OWARE_HOUSES - 1u) : (uint8_t)(j - 1u);
    }

    /* (grand-slam handling arrives in Task 5; for now capture normally) */
    for (uint8_t c = 0; c < cap_count; c++) {
        out->score[p] = (uint8_t)(out->score[p] + out->houses[cap_idx[c]]);
        res->captured = (uint8_t)(res->captured + out->houses[cap_idx[c]]);
        out->houses[cap_idx[c]] = 0u;
    }
    return true;
}

bool oware__simulate_for_test(const oware_state_t *s, const oware_rules_t *r,
                              uint8_t house, oware_state_t *out,
                              oware_move_result_t *res) {
    return oware_simulate(s, r, house, out, res);
}
