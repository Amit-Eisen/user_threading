#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int check_failures;

static void expect(int cond, const char *msg) {
    if (cond) {
        printf("  ok   %s\n", msg);
    } else {
        fprintf(stderr, "  FAIL %s\n", msg);
        check_failures++;
    }
    fflush(stdout);
    fflush(stderr);
}

#endif
