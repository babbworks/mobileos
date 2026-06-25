#ifndef OWARE_UI_H
#define OWARE_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "oware_engine.h"
#include "oware_ai.h"
#include "oware_store.h"

/* Injected I/O so the whole app loop is testable without real stdio. */
typedef struct oware_io {
    /* Read one line (without trailing newline) into buf (cap incl NUL).
       Returns false on EOF / no more input. */
    bool (*read_line)(struct oware_io *io, char *buf, size_t cap);
    /* Write a NUL-terminated string. */
    void (*write_str)(struct oware_io *io, const char *s);
    void *ctx;
} oware_io_t;

typedef struct {
    bool                  side_is_ai[2];
    oware_ai_config_t     ai_cfg;
    char                  name[2][OWARE_STORE_NAME_CAP];
    oware_ai_difficulty_t difficulty; /* vs-CPU record bucket */
    bool                  vs_cpu;
    uint8_t               human_side; /* which side the human plays in vs-CPU */
} oware_match_cfg_t;

/* '1'..'6' -> ring house index for player. p0 -> 0..5, p1 -> 6..11. */
bool   oware_ui_house_for_key(uint8_t player, char key, uint8_t *house);
/* Parse a house from a line (skips leading blanks; uses first char). */
bool   oware_ui_parse_house(const char *line, uint8_t player, uint8_t *house);

/* Render numeric board for `viewer` into buf. Returns bytes written (excl NUL). */
size_t oware_ui_render_board(const oware_state_t *s, uint8_t viewer,
                             char *buf, size_t cap);

/* Play one game to completion via io. Re-prompts on illegal human input.
   'q' as human input resolves the game by own-side split (agreed end). */
oware_result_t oware_ui_play_game(const oware_rules_t *r,
                                  const oware_match_cfg_t *m, oware_io_t *io);

/* Top-level loop: menu -> matches -> update+persist store. Returns on quit. */
void   oware_ui_run(oware_io_t *io, oware_store_t *store, const char *path);

#endif /* OWARE_UI_H */
