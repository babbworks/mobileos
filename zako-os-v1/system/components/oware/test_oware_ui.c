#include "oware_ui.h"
#include "oware_test.h"
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

int main(void) {
    test_house_for_key();
    test_parse_house();
    TEST_REPORT();
}
