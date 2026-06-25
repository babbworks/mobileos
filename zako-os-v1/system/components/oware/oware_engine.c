#include "oware_engine.h"
#include <stddef.h>

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
    while (seeds > 0u) {
        i = (uint8_t)((i + 1u) % OWARE_HOUSES);
        if (i == house) { continue; }            /* skip origin on wrap; seeds unchanged */
        out->houses[i] = (uint8_t)(out->houses[i] + 1u);
        seeds--;
    }

    res->landing = i;
    res->captured = 0u;
    res->was_grand_slam = false;
    res->forced_feed = false;

    /* provisional capture chain: walk backward over opponent houses */
    uint8_t cap_idx[OWARE_SIDE];
    uint8_t cap_count = 0u;
    uint8_t j = i;
    for (;;) {
        if (oware_house_belongs_to(j, p)) { break; }          /* own side */
        if (!oware_capturable(out->houses[j], r->capture_rule)) { break; }
        cap_idx[cap_count] = j;
        cap_count++;
        if (cap_count >= OWARE_SIDE) { break; }
        j = (j == 0u) ? (uint8_t)(OWARE_HOUSES - 1u) : (uint8_t)(j - 1u);
    }

    /* detect grand slam: would the capture empty the opponent's whole side? */
    uint8_t opp = (uint8_t)(p ^ 1u);
    int cap_total = 0;
    for (uint8_t c = 0; c < cap_count; c++) {
        cap_total += (int)out->houses[cap_idx[c]];
    }
    int opp_total = oware_side_seeds(out, opp);
    bool grand_slam = (cap_count > 0u) && (cap_total == opp_total);
    res->was_grand_slam = grand_slam;

    if (cap_count == 0u) {
        return true;
    }

    if (!grand_slam) {
        for (uint8_t c = 0; c < cap_count; c++) {
            out->score[p] = (uint8_t)(out->score[p] + out->houses[cap_idx[c]]);
            res->captured = (uint8_t)(res->captured + out->houses[cap_idx[c]]);
            out->houses[cap_idx[c]] = 0u;
        }
        return true;
    }

    switch (r->grandslam_rule) {
        case OWARE_GS_NO_CAPTURE:
            break;                       /* seeds stay sown; capture nothing */
        case OWARE_GS_FORBIDDEN:
            break;                       /* legality enforced in legal_moves */
        case OWARE_GS_OPPONENT_KEEPS:
            for (uint8_t c = 0; c < cap_count; c++) {
                out->score[p] = (uint8_t)(out->score[p] + out->houses[cap_idx[c]]);
                res->captured = (uint8_t)(res->captured + out->houses[cap_idx[c]]);
                out->houses[cap_idx[c]] = 0u;
            }
            for (uint8_t k = 0; k < OWARE_HOUSES; k++) {
                out->score[opp] = (uint8_t)(out->score[opp] + out->houses[k]);
                out->houses[k] = 0u;
            }
            break;
        case OWARE_GS_LEAVE_LAST:
            /* cap_idx is ordered landing-first; furthest-back is last entry */
            for (uint8_t c = 0; (c + 1u) < cap_count; c++) {
                out->score[p] = (uint8_t)(out->score[p] + out->houses[cap_idx[c]]);
                res->captured = (uint8_t)(res->captured + out->houses[cap_idx[c]]);
                out->houses[cap_idx[c]] = 0u;
            }
            break;
        default:
            break;
    }
    return true;
}

bool oware__simulate_for_test(const oware_state_t *s, const oware_rules_t *r,
                              uint8_t house, oware_state_t *out,
                              oware_move_result_t *res) {
    return oware_simulate(s, r, house, out, res);
}

int oware_legal_moves(const oware_state_t *s, const oware_rules_t *r,
                      uint8_t out[OWARE_SIDE]) {
    uint8_t p = s->turn;
    uint8_t opp = (uint8_t)(p ^ 1u);
    bool opp_empty = (oware_side_seeds(s, opp) == 0);
    int n = 0;
    for (uint8_t h = 0; h < OWARE_HOUSES; h++) {
        if (!oware_house_belongs_to(h, p)) { continue; }
        if (s->houses[h] == 0u) { continue; }
        oware_state_t tmp;
        oware_move_result_t res;
        if (!oware_simulate(s, r, h, &tmp, &res)) { continue; }
        if (res.was_grand_slam && (r->grandslam_rule == OWARE_GS_FORBIDDEN)) {
            continue;
        }
        if (opp_empty && (oware_side_seeds(&tmp, opp) == 0)) {
            continue;                    /* fails feeding obligation */
        }
        out[n] = h;
        n++;
    }
    return n;
}

bool oware_is_legal(const oware_state_t *s, const oware_rules_t *r, uint8_t house) {
    uint8_t mv[OWARE_SIDE];
    int n = oware_legal_moves(s, r, mv);
    for (int k = 0; k < n; k++) {
        if (mv[k] == house) { return true; }
    }
    return false;
}

bool oware_apply_move(oware_state_t *s, const oware_rules_t *r,
                      uint8_t house, oware_move_result_t *res) {
    if (!oware_is_legal(s, r, house)) {
        return false;
    }
    uint8_t opp = (uint8_t)(s->turn ^ 1u);
    bool was_feeding = (oware_side_seeds(s, opp) == 0);

    oware_state_t out;
    oware_move_result_t mr;
    (void)oware_simulate(s, r, house, &out, &mr);
    mr.forced_feed = was_feeding;

    if (mr.captured > 0u) {
        out.no_capture_plies = 0u;
    } else {
        out.no_capture_plies = (uint16_t)(s->no_capture_plies + 1u);
    }
    out.turn = (uint8_t)(s->turn ^ 1u);

    *s = out;
    if (res != NULL) {
        *res = mr;
    }
    return true;
}

static void oware_collect_each_side(oware_state_t *s) {
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) {
        if (s->houses[i] > 0u) {
            uint8_t owner = (i < OWARE_SIDE) ? 0u : 1u;
            s->score[owner] = (uint8_t)(s->score[owner] + s->houses[i]);
            s->houses[i] = 0u;
        }
    }
}

static void oware_set_outcome(oware_result_t *res) {
    if (res->score[0] > res->score[1]) {
        res->outcome = OWARE_OUT_P0;
    } else if (res->score[1] > res->score[0]) {
        res->outcome = OWARE_OUT_P1;
    } else {
        res->outcome = OWARE_OUT_DRAW;
    }
}

bool oware_is_over(const oware_state_t *s, const oware_rules_t *r,
                   oware_result_t *res) {
    res->over = false;
    res->outcome = OWARE_OUT_NONE;
    res->score[0] = s->score[0];
    res->score[1] = s->score[1];

    if (r->end_mode == OWARE_END_FIRST_TO_N) {
        if ((s->score[0] >= r->target_score) ||
            (s->score[1] >= r->target_score)) {
            res->over = true;
        }
    }

    if (!res->over && (oware_board_seeds(s) == 0)) {
        res->over = true;                /* all seeds captured */
    }

    if (!res->over && (r->cycle_ply_limit > 0u) &&
        (s->no_capture_plies >= r->cycle_ply_limit)) {
        oware_state_t tmp = *s;
        oware_collect_each_side(&tmp);
        res->score[0] = tmp.score[0];
        res->score[1] = tmp.score[1];
        res->over = true;
    }

    if (!res->over) {
        uint8_t mv[OWARE_SIDE];
        if (oware_legal_moves(s, r, mv) == 0) {
            oware_state_t tmp = *s;
            oware_collect_each_side(&tmp);
            res->score[0] = tmp.score[0];
            res->score[1] = tmp.score[1];
            res->over = true;
        }
    }

    if (res->over) {
        oware_set_outcome(res);
    }
    return res->over;
}

void oware_resolve_agreed(const oware_state_t *s, oware_result_t *res) {
    oware_state_t tmp = *s;
    oware_collect_each_side(&tmp);
    res->over = true;
    res->score[0] = tmp.score[0];
    res->score[1] = tmp.score[1];
    oware_set_outcome(res);
}
