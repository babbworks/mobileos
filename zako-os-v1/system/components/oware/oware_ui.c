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
