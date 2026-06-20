#ifndef OWARE_AI_H
#define OWARE_AI_H

#include "oware_engine.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    OWARE_AI_EASY   = 0,
    OWARE_AI_MEDIUM = 1,
    OWARE_AI_HARD   = 2
} oware_ai_difficulty_t;

typedef struct {
    oware_ai_difficulty_t difficulty;
    uint32_t              rng_seed; /* 0 = default seed 1 */
} oware_ai_config_t;

void oware_ai_config_default(oware_ai_config_t *cfg, oware_ai_difficulty_t d);

/* Precondition: s->turn == ai_player and game not over.
   On success, *house is a ring index (0-11) valid for oware_apply_move. */
bool oware_ai_choose_move(const oware_state_t *s, const oware_rules_t *r,
                          const oware_ai_config_t *cfg, uint8_t ai_player,
                          uint8_t *house);

int oware_ai_eval_for_test(const oware_state_t *s, const oware_rules_t *r,
                           uint8_t player);

#endif /* OWARE_AI_H */
