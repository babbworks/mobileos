#ifndef OWARE_ENGINE_H
#define OWARE_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#define OWARE_HOUSES 12u
#define OWARE_SIDE    6u
#define OWARE_SEEDS  48u

typedef enum {
    OWARE_CAP_STANDARD   = 0, /* capture on house reaching 2 or 3 */
    OWARE_CAP_THREE_FOUR = 1  /* capture on house reaching 3 or 4 */
} oware_capture_rule_t;

typedef enum {
    OWARE_GS_NO_CAPTURE     = 0, /* legal move, captures nothing (default) */
    OWARE_GS_FORBIDDEN      = 1, /* move removed from legal set */
    OWARE_GS_OPPONENT_KEEPS = 2, /* slam captured; remaining board seeds to opponent */
    OWARE_GS_LEAVE_LAST     = 3  /* capture chain except its furthest-back house */
} oware_grandslam_rule_t;

typedef enum {
    OWARE_END_FIRST_TO_N  = 0, /* ends instant a side reaches target_score */
    OWARE_END_ALL_CAPTURED = 1 /* no early stop; play until board empty/terminal */
} oware_end_mode_t;

typedef struct {
    oware_capture_rule_t   capture_rule;
    oware_grandslam_rule_t grandslam_rule;
    oware_end_mode_t       end_mode;
    uint8_t                target_score;    /* used by FIRST_TO_N */
    uint16_t               cycle_ply_limit; /* 0 disables cycle detection */
    bool                   allow_agreed_end;
} oware_rules_t;

typedef struct {
    uint8_t  houses[OWARE_HOUSES]; /* 0-5 = player 0 (South), 6-11 = player 1 (North) */
    uint8_t  score[2];
    uint8_t  turn;                 /* 0 or 1 */
    uint16_t no_capture_plies;
} oware_state_t;

typedef struct {
    uint8_t landing;        /* index of last sown seed */
    uint8_t captured;       /* seeds captured this move */
    bool    was_grand_slam;
    bool    forced_feed;    /* move was made while opponent was empty */
} oware_move_result_t;

typedef enum {
    OWARE_OUT_NONE = 0,
    OWARE_OUT_P0   = 1,
    OWARE_OUT_P1   = 2,
    OWARE_OUT_DRAW = 3
} oware_outcome_t;

typedef struct {
    bool            over;
    oware_outcome_t outcome;
    uint8_t         score[2];
} oware_result_t;

void oware_rules_default(oware_rules_t *r);
void oware_init(oware_state_t *s);

bool oware_house_belongs_to(uint8_t house, uint8_t player);
int  oware_side_seeds(const oware_state_t *s, uint8_t player);
int  oware_board_seeds(const oware_state_t *s);

int  oware_legal_moves(const oware_state_t *s, const oware_rules_t *r,
                       uint8_t out[OWARE_SIDE]);
bool oware_is_legal(const oware_state_t *s, const oware_rules_t *r, uint8_t house);
bool oware_apply_move(oware_state_t *s, const oware_rules_t *r,
                      uint8_t house, oware_move_result_t *res);

bool oware_is_over(const oware_state_t *s, const oware_rules_t *r,
                   oware_result_t *res);
void oware_resolve_agreed(const oware_state_t *s, oware_result_t *res);

#endif /* OWARE_ENGINE_H */
