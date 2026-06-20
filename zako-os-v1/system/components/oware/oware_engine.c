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
