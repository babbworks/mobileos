#include "oware_ui.h"
#include <stdio.h>   /* snprintf only; no direct terminal I/O here */
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
    memset(&res, 0, sizeof(res));
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
                oware_resolve_agreed(&s, &res); /* no legal move: resolve now */
                return res;
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

    (void)oware_ui_render_board(&s, s.turn, buf, sizeof(buf));
    io->write_str(io, buf);
    return res;
}

static void ui_rules_from_store(const oware_store_t *st, oware_rules_t *r) {
    oware_rules_default(r);
    r->grandslam_rule = st->grandslam_rule;
    r->capture_rule   = st->capture_rule;
    r->end_mode       = st->end_mode;
    r->target_score   = st->target_score;
}

static bool ui_prompt(oware_io_t *io, const char *prompt, char *buf, size_t cap) {
    io->write_str(io, prompt);
    return io->read_line(io, buf, cap);
}

static oware_game_result_t ui_result_for_side(const oware_result_t *res,
                                              uint8_t side) {
    if (res->outcome == OWARE_OUT_DRAW) {
        return OWARE_GAME_DRAW;
    }
    bool side_won = (res->outcome == ((side == 0u) ? OWARE_OUT_P0 : OWARE_OUT_P1));
    return side_won ? OWARE_GAME_WIN : OWARE_GAME_LOSS;
}

static void ui_do_vs_cpu(oware_io_t *io, oware_store_t *st, const char *path) {
    char line[64];
    if (!ui_prompt(io, "Difficulty (1=Easy 2=Medium 3=Hard): ", line, sizeof(line))) {
        return;
    }
    oware_ai_difficulty_t d = OWARE_AI_MEDIUM;
    if (line[0] == '1') { d = OWARE_AI_EASY; }
    else if (line[0] == '3') { d = OWARE_AI_HARD; }

    oware_match_cfg_t m;
    memset(&m, 0, sizeof(m));
    m.vs_cpu = true;
    m.human_side = 0u;
    m.side_is_ai[0] = false;
    m.side_is_ai[1] = true;
    m.difficulty = d;
    oware_ai_config_default(&m.ai_cfg, d);

    oware_rules_t r;
    ui_rules_from_store(st, &r);
    oware_result_t res = oware_ui_play_game(&r, &m, io);
    oware_store_record_cpu(st, d, ui_result_for_side(&res, m.human_side));
    (void)oware_store_save(st, path);
}

static void ui_do_two_player(oware_io_t *io, oware_store_t *st, const char *path) {
    char n1[64];
    char n2[64];
    if (!ui_prompt(io, "Player 1 name: ", n1, sizeof(n1))) { return; }
    if (!ui_prompt(io, "Player 2 name: ", n2, sizeof(n2))) { return; }

    oware_match_cfg_t m;
    memset(&m, 0, sizeof(m));
    m.vs_cpu = false;
    m.side_is_ai[0] = false;
    m.side_is_ai[1] = false;

    oware_rules_t r;
    ui_rules_from_store(st, &r);
    oware_result_t res = oware_ui_play_game(&r, &m, io);
    (void)oware_store_record_pair(st, n1, n2, ui_result_for_side(&res, 0u));
    (void)oware_store_save(st, path);
}

static void ui_show_records(oware_io_t *io, oware_store_t *st, const char *path) {
    char line[128];
    static const char *const names[3] = { "Easy", "Medium", "Hard" };
    io->write_str(io, "\n-- vs Computer (W-L-D) --\n");
    for (size_t i = 0u; i < 3u; i++) {
        (void)snprintf(line, sizeof(line), "  %-6s  %u-%u-%u\n", names[i],
                       (unsigned)st->cpu[i].wins, (unsigned)st->cpu[i].losses,
                       (unsigned)st->cpu[i].draws);
        io->write_str(io, line);
    }
    io->write_str(io, "-- Head to head --\n");
    for (uint8_t i = 0u; i < st->pair_count; i++) {
        (void)snprintf(line, sizeof(line), "  %s %u - %u %s (draws %u)\n",
                       st->pairs[i].a, (unsigned)st->pairs[i].wins_a,
                       (unsigned)st->pairs[i].wins_b, st->pairs[i].b,
                       (unsigned)st->pairs[i].draws);
        io->write_str(io, line);
    }
    if (ui_prompt(io, "Reset vs-CPU records? (y/N): ", line, sizeof(line))) {
        if ((line[0] == 'y') || (line[0] == 'Y')) {
            oware_store_reset_cpu(st);
            (void)oware_store_save(st, path);
        }
    }
}

static void ui_settings(oware_io_t *io, oware_store_t *st, const char *path) {
    char line[64];
    io->write_str(io,
        "\nGrand-slam rule: 0=NoCapture 1=Forbidden 2=OpponentKeeps 3=LeaveLast\n");
    if (!ui_prompt(io, "Pin variant (0-3): ", line, sizeof(line))) { return; }
    if ((line[0] >= '0') && (line[0] <= '3')) {
        st->grandslam_rule = (oware_grandslam_rule_t)(line[0] - '0');
        (void)oware_store_save(st, path);
        io->write_str(io, "Saved.\n");
    }
}

void oware_ui_run(oware_io_t *io, oware_store_t *store, const char *path) {
    for (;;) {
        char line[64];
        io->write_str(io,
            "\n== OWARE ==\n"
            "1) Play vs Computer\n"
            "2) Two-Player\n"
            "3) Records\n"
            "4) Settings\n"
            "5) Quit\n"
            "Choose: ");
        if (!io->read_line(io, line, sizeof(line))) {
            break;
        }
        switch (line[0]) {
            case '1': ui_do_vs_cpu(io, store, path);     break;
            case '2': ui_do_two_player(io, store, path); break;
            case '3': ui_show_records(io, store, path);  break;
            case '4': ui_settings(io, store, path);      break;
            case '5':
            case 'q':
            case 'Q':
                return;
            default:
                io->write_str(io, "Invalid choice.\n");
                break;
        }
    }
}
