#include "oware_ui.h"
#include <stdio.h>
#include <string.h>

bool oware_ui_house_for_key(uint8_t player, char key, uint8_t *house) {
    if ((key < '1') || (key > '6')) {
        return false;
    }
    uint8_t k = (uint8_t)(key - '1');
    uint8_t base = (player == 0u) ? 0u : 6u;
    *house = (uint8_t)(base + k);
    return true;
}

bool oware_ui_parse_house(const char *line, uint8_t player, uint8_t *house) {
    size_t i = 0u;
    if (line == NULL) {
        return false;
    }
    while ((line[i] == ' ') || (line[i] == '\t')) {
        i++;
    }
    return oware_ui_house_for_key(player, line[i], house);
}

size_t oware_ui_render_board(const oware_state_t *s, uint8_t viewer,
                             char *buf, size_t cap) {
    uint8_t yb = (viewer == 0u) ? 0u : 6u; /* your base */
    uint8_t ob = (viewer == 0u) ? 6u : 0u; /* opponent base */
    int n = snprintf(buf, cap,
        "\n"
        "  Opp:  [%2u %2u %2u %2u %2u %2u]\n"
        "  You:  [%2u %2u %2u %2u %2u %2u]\n"
        "  keys:    1  2  3  4  5  6\n"
        "  Score  You: %u   Opp: %u\n",
        (unsigned)s->houses[ob + 5u], (unsigned)s->houses[ob + 4u],
        (unsigned)s->houses[ob + 3u], (unsigned)s->houses[ob + 2u],
        (unsigned)s->houses[ob + 1u], (unsigned)s->houses[ob + 0u],
        (unsigned)s->houses[yb + 0u], (unsigned)s->houses[yb + 1u],
        (unsigned)s->houses[yb + 2u], (unsigned)s->houses[yb + 3u],
        (unsigned)s->houses[yb + 4u], (unsigned)s->houses[yb + 5u],
        (unsigned)s->score[viewer], (unsigned)s->score[viewer ^ 1u]);
    if (n < 0) {
        return 0u;
    }
    return (size_t)n;
}

oware_result_t oware_ui_play_game(const oware_rules_t *r,
                                  const oware_match_cfg_t *m, oware_io_t *io) {
    oware_state_t s;
    oware_init(&s);
    oware_result_t res;
    char buf[512];

    for (;;) {
        if (oware_is_over(&s, r, &res)) {
            break;
        }
        uint8_t p = s.turn;
        (void)oware_ui_render_board(&s, p, buf, sizeof(buf));
        io->write_str(io, buf);

        uint8_t house = 0u;
        if (m->side_is_ai[p]) {
            oware_ai_config_t cfg = m->ai_cfg;
            if (!oware_ai_choose_move(&s, r, &cfg, p, &house)) {
                break; /* no move; is_over resolves next loop */
            }
            char msg[48];
            (void)snprintf(msg, sizeof(msg), "Computer plays %u.\n",
                           (unsigned)((house % 6u) + 1u));
            io->write_str(io, msg);
        } else {
            bool got = false;
            while (!got) {
                io->write_str(io, "Your move (1-6, q=quit): ");
                char line[64];
                if (!io->read_line(io, line, sizeof(line))) {
                    oware_resolve_agreed(&s, &res);
                    return res;
                }
                if ((line[0] == 'q') || (line[0] == 'Q')) {
                    oware_resolve_agreed(&s, &res);
                    return res;
                }
                if (oware_ui_parse_house(line, p, &house) &&
                    oware_is_legal(&s, r, house)) {
                    got = true;
                } else {
                    io->write_str(io, "Illegal move, try again.\n");
                }
            }
        }
        oware_move_result_t mr;
        (void)oware_apply_move(&s, r, house, &mr);
    }

    (void)oware_ui_render_board(&s, 0u, buf, sizeof(buf));
    io->write_str(io, buf);
    return res;
}
