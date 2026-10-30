#include "check.h"
#include "thread.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t hits;

static void worker(void) {
    for (;;) {
        hits++;
    }
}

int main(void) {
    alarm(5);
    printf("test_block\n");
    expect(thread_system_init(1000) == 0, "init");

    int tid = thread_create(worker);
    expect(tid > 0, "create");
    while (tid > 0 && hits == 0) {
    }
    expect(thread_unblock(tid) == -1, "unblock of a running thread fails");

    expect(thread_block(tid) == 0, "block");
    sig_atomic_t frozen = hits;
    for (volatile int i = 0; i < 2000000; i++) {
    }
    expect(hits == frozen, "blocked thread does not run");

    expect(thread_unblock(tid) == 0, "unblock");
    int guard = 0;
    while (hits == frozen && guard < 100000000) {
        guard++;
    }
    expect(hits != frozen, "unblocked thread runs");

    expect(thread_exit(tid) == 0, "exit running thread");
    sig_atomic_t stopped = hits;
    for (volatile int i = 0; i < 2000000; i++) {
    }
    expect(hits == stopped, "exited thread does not run");

    thread_system_shutdown();
    printf(check_failures ? "FAIL test_block\n" : "PASS test_block\n");
    return check_failures ? 1 : 0;
}
