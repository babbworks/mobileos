#ifndef OWARE_STORE_H
#define OWARE_STORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "oware_engine.h"   /* rule enums */
#include "oware_ai.h"       /* oware_ai_difficulty_t */

#define OWARE_STORE_MAX_PAIRS 64u
#define OWARE_STORE_NAME_CAP  16u   /* buffer size: up to 15 chars + NUL */

typedef struct {
    uint32_t wins;    /* human wins */
    uint32_t losses;  /* human losses */
    uint32_t draws;
} oware_record_t;

typedef struct {
    char     a[OWARE_STORE_NAME_CAP]; /* normalized so strcmp(a,b) <= 0 */
    char     b[OWARE_STORE_NAME_CAP];
    uint32_t wins_a;
    uint32_t wins_b;
    uint32_t draws;
} oware_pair_record_t;

typedef struct {
    oware_grandslam_rule_t grandslam_rule;
    oware_capture_rule_t   capture_rule;
    oware_end_mode_t       end_mode;
    uint8_t                target_score;
    oware_record_t         cpu[3]; /* indexed by oware_ai_difficulty_t */
    oware_pair_record_t    pairs[OWARE_STORE_MAX_PAIRS];
    uint8_t                pair_count;
} oware_store_t;

typedef enum {
    OWARE_GAME_LOSS = 0,
    OWARE_GAME_WIN  = 1,
    OWARE_GAME_DRAW = 2
} oware_game_result_t;

void  oware_store_init(oware_store_t *st);

/* Returns a file path. Uses $ZAKO_OWARE_HOME (a directory) if set and non-empty,
   else $HOME/.local/share/zako-oware. Writes into buf and returns buf. */
const char *oware_store_default_path(char *buf, size_t cap);

/* Load from path. On missing/unreadable file, leaves *st at defaults and
   returns false (normal on first run). Malformed lines are skipped. */
bool  oware_store_load(oware_store_t *st, const char *path);

/* Save to path, creating parent directories if needed. False on I/O error. */
bool  oware_store_save(const oware_store_t *st, const char *path);

/* Record a vs-CPU game result (human's perspective) at a difficulty. */
void  oware_store_record_cpu(oware_store_t *st, oware_ai_difficulty_t d,
                             oware_game_result_t human_result);
void  oware_store_reset_cpu(oware_store_t *st);

/* Find-or-create the head-to-head record for a name pair (order-independent).
   Returns NULL only if the table is full and the pair is new. */
oware_pair_record_t *oware_store_pair(oware_store_t *st,
                                      const char *name1, const char *name2);

/* Record a 2-player result from name1's perspective. False if the pair table
   was full and the pair could not be created. */
bool  oware_store_record_pair(oware_store_t *st, const char *name1,
                              const char *name2, oware_game_result_t name1_result);

#endif /* OWARE_STORE_H */
