#include "oware_ai.h"

#define OWARE_AI_WIN_BASE       100000
#define OWARE_AI_EASY_RANDOM_PCT 20
#define OWARE_AI_DEPTH_EASY      1
#define OWARE_AI_DEPTH_MEDIUM    5
#define OWARE_AI_DEPTH_HARD     10
#define OWARE_AI_MOBILITY_EST    3

#define W_MAT   100
#define W_ATTACK 8
#define W_VULN   8
#define W_HOARD  3
#define W_MOB    2

#define OWARE_AI_SCORE_MIN (-2000000000)
#define OWARE_AI_SCORE_MAX  2000000000

static uint32_t g_oware_ai_rng;

static bool oware_ai_capturable(uint8_t count, oware_capture_rule_t rule) {
    switch (rule) {
        case OWARE_CAP_STANDARD:   return (count == 2u) || (count == 3u);
        case OWARE_CAP_THREE_FOUR: return (count == 3u) || (count == 4u);
        default:                   return false;
    }
}

static bool oware_ai_one_short_of_capture(uint8_t count, oware_capture_rule_t rule) {
    if (count >= 255u) {
        return false;
    }
    return oware_ai_capturable((uint8_t)(count + 1u), rule);
}

static int oware_ai_mobility(const oware_state_t *s, const oware_rules_t *r,
                             uint8_t player) {
    if (s->turn == player) {
        uint8_t mv[OWARE_SIDE];
        return oware_legal_moves(s, r, mv);
    }
    return OWARE_AI_MOBILITY_EST;
}

static int oware_ai_eval(const oware_state_t *s, const oware_rules_t *r,
                           uint8_t player) {
    uint8_t opp = (uint8_t)(player ^ 1u);
    int score = (int)s->score[player] - (int)s->score[opp];
    score *= W_MAT;

    for (uint8_t i = 0; i < OWARE_HOUSES; i++) {
        if (oware_house_belongs_to(i, opp)) {
            if (oware_ai_one_short_of_capture(s->houses[i], r->capture_rule)) {
                score += W_ATTACK;
            }
        } else if (oware_house_belongs_to(i, player)) {
            if (oware_ai_one_short_of_capture(s->houses[i], r->capture_rule)) {
                score -= W_VULN;
            }
        }
    }

    uint8_t max_hoard = 0u;
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) {
        if (oware_house_belongs_to(i, player) && (s->houses[i] > max_hoard)) {
            max_hoard = s->houses[i];
        }
    }
    if (max_hoard > 12u) {
        max_hoard = 12u;
    }
    score += (int)max_hoard * W_HOARD;
    score += oware_ai_mobility(s, r, player) * W_MOB;

    return score;
}

static int oware_ai_terminal(const oware_result_t *res, uint8_t turn, int depth) {
    switch (res->outcome) {
        case OWARE_OUT_P0:
            if (turn == 0u) {
                return OWARE_AI_WIN_BASE + depth;
            }
            return -OWARE_AI_WIN_BASE - depth;
        case OWARE_OUT_P1:
            if (turn == 1u) {
                return OWARE_AI_WIN_BASE + depth;
            }
            return -OWARE_AI_WIN_BASE - depth;
        case OWARE_OUT_DRAW:
            return 0;
        default:
            return 0;
    }
}

static uint32_t oware_ai_rng_next(void) {
    g_oware_ai_rng = (g_oware_ai_rng * 1103515245u) + 12345u;
    return (g_oware_ai_rng >> 16) & 0x7fffu;
}

static void oware_ai_rng_seed(uint32_t seed) {
    g_oware_ai_rng = (seed == 0u) ? 1u : seed;
}

static void oware_ai_order_moves(const oware_state_t *s, const oware_rules_t *r,
                                 const uint8_t *mv, int n, uint8_t *out,
                                 int *out_n) {
    int scored[OWARE_SIDE];
    uint8_t i;

    for (i = 0; i < (uint8_t)n; i++) {
        oware_move_result_t mr;
        oware_state_t tmp = *s;
        (void)oware_apply_move(&tmp, r, mv[i], &mr);
        scored[i] = (int)mr.captured;
    }

    *out_n = n;
    for (i = 0; i < (uint8_t)n; i++) {
        int best_j = 0;
        int j;
        for (j = 1; j < n; j++) {
            if (scored[j] > scored[best_j]) {
                best_j = j;
            }
        }
        out[i] = mv[(uint8_t)best_j];
        scored[best_j] = -1;
    }
}

static int oware_ai_negamax(oware_state_t *s, const oware_rules_t *r, int depth,
                            int alpha, int beta, bool capture_order) {
    oware_result_t over;
    uint8_t mv[OWARE_SIDE];
    uint8_t ordered[OWARE_SIDE];
    int n;
    int i;
    int best;

    if (oware_is_over(s, r, &over)) {
        return oware_ai_terminal(&over, s->turn, depth);
    }
    if (depth == 0) {
        return oware_ai_eval(s, r, s->turn);
    }

    n = oware_legal_moves(s, r, mv);
    if (n == 0) {
        if (oware_is_over(s, r, &over)) {
            return oware_ai_terminal(&over, s->turn, depth);
        }
        return oware_ai_eval(s, r, s->turn);
    }

    if (capture_order) {
        oware_ai_order_moves(s, r, mv, n, ordered, &n);
    } else {
        for (i = 0; i < n; i++) {
            ordered[i] = mv[i];
        }
    }

    best = OWARE_AI_SCORE_MIN;
    for (i = 0; i < n; i++) {
        oware_state_t next = *s;
        oware_move_result_t mr;
        int child;

        if (!oware_apply_move(&next, r, ordered[i], &mr)) {
            continue;
        }
        child = -oware_ai_negamax(&next, r, depth - 1, -beta, -alpha,
                                  capture_order);
        if (child > best) {
            best = child;
        }
        if (child > alpha) {
            alpha = child;
        }
        if (alpha >= beta) {
            break;
        }
    }
    return best;
}

static int oware_ai_search_move(const oware_state_t *s, const oware_rules_t *r,
                                int depth, bool capture_order, uint8_t ai_player,
                                bool random_ties, uint8_t *best_house) {
    uint8_t mv[OWARE_SIDE];
    int n = oware_legal_moves(s, r, mv);
    int best_score = OWARE_AI_SCORE_MIN;
    int tie[OWARE_SIDE];
    int tie_n = 0;
    int i;

    if (n == 0) {
        return OWARE_AI_SCORE_MIN;
    }

    for (i = 0; i < n; i++) {
        oware_state_t next = *s;
        oware_move_result_t mr;
        oware_result_t over;
        int score;

        if (!oware_apply_move(&next, r, mv[i], &mr)) {
            continue;
        }
        if (oware_is_over(&next, r, &over)) {
            score = -oware_ai_terminal(&over, next.turn, depth - 1);
        } else if (depth <= 1) {
            score = oware_ai_eval(&next, r, ai_player);
        } else {
            score = -oware_ai_negamax(&next, r, depth - 1,
                                      OWARE_AI_SCORE_MIN, OWARE_AI_SCORE_MAX,
                                      capture_order);
        }

        if (score > best_score) {
            best_score = score;
            tie_n = 0;
            tie[tie_n] = i;
            tie_n++;
        } else if (score == best_score) {
            tie[tie_n] = i;
            tie_n++;
        }
    }

    if (tie_n == 0) {
        *best_house = mv[0];
        return best_score;
    }

    if (random_ties) {
        *best_house = mv[tie[(int)(oware_ai_rng_next() % (uint32_t)tie_n)]];
    } else {
        *best_house = mv[tie[0]];
    }
    return best_score;
}

void oware_ai_config_default(oware_ai_config_t *cfg, oware_ai_difficulty_t d) {
    cfg->difficulty = d;
    cfg->rng_seed = 1u;
}

int oware_ai_eval_for_test(const oware_state_t *s, const oware_rules_t *r,
                           uint8_t player) {
    return oware_ai_eval(s, r, player);
}

bool oware_ai_choose_move(const oware_state_t *s, const oware_rules_t *r,
                          const oware_ai_config_t *cfg, uint8_t ai_player,
                          uint8_t *house) {
    uint8_t mv[OWARE_SIDE];
    int n;
    int depth;
    bool capture_order = false;
    bool iterative = false;
    bool random_ties = false;
    uint8_t best = OWARE_HOUSES;   /* sentinel: illegal; caught by oware_is_legal */
    int d;

    if (s->turn != ai_player) {
        return false;
    }
    n = oware_legal_moves(s, r, mv);
    if (n == 0) {
        return false;
    }

    oware_ai_rng_seed(cfg->rng_seed);

    switch (cfg->difficulty) {
        case OWARE_AI_EASY:
            depth = OWARE_AI_DEPTH_EASY;
            random_ties = true;
            if ((int)(oware_ai_rng_next() % 100u) < OWARE_AI_EASY_RANDOM_PCT) {
                *house = mv[(int)(oware_ai_rng_next() % (uint32_t)n)];
                return true;
            }
            break;
        case OWARE_AI_MEDIUM:
            depth = OWARE_AI_DEPTH_MEDIUM;
            break;
        case OWARE_AI_HARD:
            depth = OWARE_AI_DEPTH_HARD;
            iterative = true;
            capture_order = true;
            break;
        default:
            depth = OWARE_AI_DEPTH_MEDIUM;
            break;
    }

    if (iterative) {
        for (d = 1; d <= depth; d++) {
            (void)oware_ai_search_move(s, r, d, capture_order, ai_player,
                                        random_ties, &best);
        }
    } else {
        (void)oware_ai_search_move(s, r, depth, capture_order, ai_player,
                                   random_ties, &best);
    }

    *house = best;
    return oware_is_legal(s, r, best);
}
