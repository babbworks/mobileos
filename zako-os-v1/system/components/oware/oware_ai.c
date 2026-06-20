#include "oware_ai.h"

void oware_ai_config_default(oware_ai_config_t *cfg, oware_ai_difficulty_t d) {
    cfg->difficulty = d;
    cfg->rng_seed = 1u;
}

bool oware_ai_choose_move(const oware_state_t *s, const oware_rules_t *r,
                          const oware_ai_config_t *cfg, uint8_t ai_player,
                          uint8_t *house) {
    (void)cfg;
    (void)house;
    if (s->turn != ai_player) {
        return false;
    }
    uint8_t mv[OWARE_SIDE];
    if (oware_legal_moves(s, r, mv) == 0) {
        return false;
    }
    return false;
}

int oware_ai_eval_for_test(const oware_state_t *s, const oware_rules_t *r,
                           uint8_t player) {
    (void)s;
    (void)r;
    (void)player;
    return 0;
}
