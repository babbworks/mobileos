#include "oware_ui.h"
#include "oware_test.h"
#include "oware_engine.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void test_house_for_key(void) {
    uint8_t h = 0xFFu;
    CHECK(oware_ui_house_for_key(0u, '1', &h) && h == 0u);
    CHECK(oware_ui_house_for_key(0u, '6', &h) && h == 5u);
    CHECK(oware_ui_house_for_key(1u, '1', &h) && h == 6u);
    CHECK(oware_ui_house_for_key(1u, '6', &h) && h == 11u);
    CHECK(!oware_ui_house_for_key(0u, '0', &h));
    CHECK(!oware_ui_house_for_key(0u, '7', &h));
    CHECK(!oware_ui_house_for_key(0u, 'x', &h));
}

static void test_parse_house(void) {
    uint8_t h = 0xFFu;
    CHECK(oware_ui_parse_house("  3", 0u, &h) && h == 2u);
    CHECK(oware_ui_parse_house("4", 1u, &h) && h == 9u);
    CHECK(!oware_ui_parse_house("", 0u, &h));
    CHECK(!oware_ui_parse_house("nope", 0u, &h));
}

static void test_render_board(void) {
    oware_state_t s;
    oware_init(&s);
    for (uint8_t i = 0; i < OWARE_HOUSES; i++) { s.houses[i] = i; }
    s.score[0] = 7u; s.score[1] = 3u;
    char buf[512];
    size_t n = oware_ui_render_board(&s, 0u, buf, sizeof(buf));
    CHECK(n > 0u);
    /* viewer 0: your houses (idx 0..5) in key order -> 0 1 2 3 4 5 */
    CHECK(strstr(buf, " 0  1  2  3  4  5") != NULL);
    /* opponent houses (idx 6..11) shown key6..key1 -> 11 10 9 8 7 6 */
    CHECK(strstr(buf, "11 10  9  8  7  6") != NULL);
    CHECK(strstr(buf, "You: 7") != NULL);
    CHECK(strstr(buf, "Opp: 3") != NULL);
}

/* Scripted IO: feeds queued input lines; captures output into a buffer. */
typedef struct {
    const char *const *lines; /* NULL-terminated array of input lines */
    size_t next;
    char out[8192];
    size_t out_len;
} script_io_t;

static bool script_read(oware_io_t *io, char *buf, size_t cap) {
    script_io_t *s = (script_io_t *)io->ctx;
    if (s->lines[s->next] == NULL) { return false; }
    (void)snprintf(buf, cap, "%s", s->lines[s->next]);
    s->next++;
    return true;
}

static void script_write(oware_io_t *io, const char *str) {
    script_io_t *s = (script_io_t *)io->ctx;
    size_t len = strlen(str);
    if ((s->out_len + len + 1u) < sizeof(s->out)) {
        (void)memcpy(s->out + s->out_len, str, len);
        s->out_len += len;
        s->out[s->out_len] = '\0';
    }
}

static void script_io_init(oware_io_t *io, script_io_t *s,
                           const char *const *lines) {
    s->lines = lines; s->next = 0u; s->out_len = 0u; s->out[0] = '\0';
    io->read_line = script_read; io->write_str = script_write; io->ctx = s;
}

static void test_play_game_two_player_quit(void) {
    static const char *const lines[] = { "q", NULL };
    script_io_t s; oware_io_t io; script_io_init(&io, &s, lines);
    oware_rules_t r; oware_rules_default(&r);
    oware_match_cfg_t m;
    memset(&m, 0, sizeof(m));
    m.side_is_ai[0] = false; m.side_is_ai[1] = false;
    oware_result_t res = oware_ui_play_game(&r, &m, &io);
    CHECK(res.over);
    CHECK(res.score[0] == 24u && res.score[1] == 24u);
    CHECK(strstr(s.out, "Opp:") != NULL);
}

static void test_play_game_illegal_then_legal(void) {
    static const char *const lines[] = { "7", "1", "q", NULL };
    script_io_t s; oware_io_t io; script_io_init(&io, &s, lines);
    oware_rules_t r; oware_rules_default(&r);
    oware_match_cfg_t m;
    memset(&m, 0, sizeof(m));
    oware_result_t res = oware_ui_play_game(&r, &m, &io);
    CHECK(strstr(s.out, "Illegal") != NULL);
    CHECK(res.over);
}

static void test_play_game_vs_ai_completes(void) {
    static const char *const lines[] = { "q", NULL };
    script_io_t s; oware_io_t io; script_io_init(&io, &s, lines);
    oware_rules_t r; oware_rules_default(&r);
    oware_match_cfg_t m;
    memset(&m, 0, sizeof(m));
    m.side_is_ai[0] = false; m.side_is_ai[1] = true;
    oware_ai_config_default(&m.ai_cfg, OWARE_AI_EASY);
    m.difficulty = OWARE_AI_EASY;
    oware_result_t res = oware_ui_play_game(&r, &m, &io);
    CHECK(res.over);
}

int main(void) {
    test_house_for_key();
    test_parse_house();
    test_render_board();
    test_play_game_two_player_quit();
    test_play_game_illegal_then_legal();
    test_play_game_vs_ai_completes();
    TEST_REPORT();
}
