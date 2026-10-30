#include "check.h"
#include "thread.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t sleep_result = 1;
static volatile sig_atomic_t sleep_done;

static void reject_zero_sleep(void) {
    sleep_result = thread_sleep(0);
    sleep_done = 1;
}

int main(void) {
    alarm(5);
    printf("test_errors\n");

    expect(thread_system_init(0) == -1, "init rejects 0");
    expect(thread_system_init(-1) == -1, "init rejects negative");
    expect(thread_system_init(1000001) == -1, "init rejects quantum above 1s");
    expect(thread_create(reject_zero_sleep) == -1, "create before init fails");
    expect(thread_block(1) == -1, "block before init fails");
    expect(thread_unblock(1) == -1, "unblock before init fails");
    expect(thread_exit(1) == -1, "exit before init fails");
    expect(thread_sleep(1) == -1, "sleep before init fails");

    expect(thread_system_init(100000) == 0, "init accepts 100ms");
    expect(thread_system_init(100000) == -1, "second init fails");
    expect(thread_create(NULL) == -1, "create rejects NULL");
    expect(thread_block(-1) == -1, "block rejects -1");
    expect(thread_block(0) == -1, "block rejects main");
    expect(thread_block(MAX_THREADS) == -1, "block rejects MAX_THREADS");
    expect(thread_block(1) == -1, "block rejects a free slot");
    expect(thread_unblock(-1) == -1, "unblock rejects -1");
    expect(thread_unblock(0) == -1, "unblock rejects main");
    expect(thread_exit(-1) == -1, "exit rejects -1");
    expect(thread_exit(2) == -1, "exit rejects a free slot");
    expect(thread_sleep(1) == -1, "main cannot sleep");
    expect(thread_sleep(0) == -1, "sleep rejects 0");
    expect(thread_sleep(-3) == -1, "sleep rejects negative");

    int tid = thread_create(reject_zero_sleep);
    expect(tid > 0, "create worker");
    while (tid > 0 && !sleep_done) {
    }
    expect(sleep_result == -1, "worker sleep(0) fails");

    thread_system_shutdown();
    printf(check_failures ? "FAIL test_errors\n" : "PASS test_errors\n");
    return check_failures ? 1 : 0;
}
