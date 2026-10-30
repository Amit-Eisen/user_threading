#include "check.h"
#include "thread.h"

#include <stdio.h>
#include <unistd.h>

static void hog(void) {
    for (;;) {
    }
}

int main(void) {
    alarm(5);
    printf("test_capacity\n");
    expect(thread_system_init(100000) == 0, "init");

    int ids[MAX_THREADS];
    int created = 0;
    for (int i = 0; i < MAX_THREADS - 1; i++) {
        ids[created] = thread_create(hog);
        if (ids[created] < 1) {
            break;
        }
        created++;
    }
    expect(created == MAX_THREADS - 1, "created every worker slot");
    expect(thread_create(hog) == -1, "no free thread");

    int exited = 1;
    for (int i = 0; i < created; i++) {
        if (thread_exit(ids[i]) != 0) {
            exited = 0;
        }
    }
    expect(exited, "exit all workers");

    int again = thread_create(hog);
    expect(again > 0, "create after a slot is freed");
    if (again > 0) {
        expect(thread_exit(again) == 0, "exit recycled worker");
    }

    thread_system_shutdown();
    printf(check_failures ? "FAIL test_capacity\n" : "PASS test_capacity\n");
    return check_failures ? 1 : 0;
}
