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
