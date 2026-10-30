#include "check.h"
#include "thread.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t ran;
static volatile sig_atomic_t hits;

static void once(void) {
    ran = 1;
}

static void spinner(void) {
    for (;;) {
        hits++;
    }
}

static int all_slots_free(void) {
    int ids[MAX_THREADS];
    int created = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        int id = thread_create(spinner);
        if (id < 1) {
            break;
        }
        ids[created++] = id;
    }

    int filled = (created == MAX_THREADS - 1);
    for (int i = 0; i < created; i++) {
        thread_exit(ids[i]);
    }
    return filled;
}

int main(void) {
    alarm(5);
    printf("test_exit\n");
    expect(thread_system_init(1000) == 0, "init");

    int tid = thread_create(once);
    expect(tid > 0, "create a thread that returns");
    while (tid > 0 && !ran) {
    }

    int freed = 0;
    for (int attempt = 0; attempt < 1000 && !freed; attempt++) {
        freed = all_slots_free();
    }
    expect(freed, "returning thread frees its slot");

    hits = 0;
    int spin = thread_create(spinner);
    expect(spin > 0, "create spinner");
    while (spin > 0 && hits == 0) {
    }
    expect(thread_exit(spin) == 0, "exit spinner");
    sig_atomic_t stopped = hits;
    for (volatile int i = 0; i < 2000000; i++) {
    }
    expect(hits == stopped, "exited spinner stays stopped");

    thread_system_shutdown();
    printf(check_failures ? "FAIL test_exit\n" : "PASS test_exit\n");
    return check_failures ? 1 : 0;
}
