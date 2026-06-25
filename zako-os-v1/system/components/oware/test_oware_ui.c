#include "oware_ui.h"
#include "oware_test.h"
#include "oware_engine.h"
#include <string.h>

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

int main(void) {
    test_house_for_key();
    test_parse_house();
    test_render_board();
    TEST_REPORT();
}
