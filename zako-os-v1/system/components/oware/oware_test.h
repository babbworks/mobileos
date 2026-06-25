#ifndef OWARE_TEST_H
#define OWARE_TEST_H
/* WARNING: static counters below mean this header must be included in exactly
   ONE translation unit per test binary. */
#include <stdio.h>

static int oware_tests_run = 0;
static int oware_tests_failed = 0;

#define CHECK(cond) do { \
    oware_tests_run++; \
    if (!(cond)) { \
        oware_tests_failed++; \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define TEST_REPORT() do { \
    printf("%d/%d checks passed\n", \
           oware_tests_run - oware_tests_failed, oware_tests_run); \
    return oware_tests_failed == 0 ? 0 : 1; \
} while (0)

#endif /* OWARE_TEST_H */
