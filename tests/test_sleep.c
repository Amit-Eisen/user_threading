#include "check.h"
#include "thread.h"

#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t phase;
static volatile sig_atomic_t progress;

static void ticker(void) {
    while (phase != 2) {
        progress++;
    }
}

static void sleeper(void) {
    phase = 1;
    if (thread_sleep(3) != 0) {
        phase = -1;
        return;
    }
    phase = 2;
}

int main(void) {
    alarm(5);
    printf("test_sleep\n");
    expect(thread_system_init(100000) == 0, "init");

    int sleeper_id = thread_create(sleeper);
    int ticker_id = thread_create(ticker);
    expect(sleeper_id > 0 && ticker_id > 0, "create sleeper and ticker");

    while (phase == 0) {
    }
    clock_t started = clock();
    sig_atomic_t mark = progress;
    while (phase == 1) {
    }
    double seconds = (double)(clock() - started) / (double)CLOCKS_PER_SEC;

    printf("  sleep cpu time %.3fs\n", seconds);
    expect(phase == 2, "sleeper woke");
    expect(progress > mark, "another thread ran during the sleep");
    expect(seconds >= 0.15, "sleep lasted at least two quantums");
    expect(seconds < 2.0, "sleep returned");

    thread_exit(sleeper_id);
    thread_exit(ticker_id);
    thread_system_shutdown();
    printf(check_failures ? "FAIL test_sleep\n" : "PASS test_sleep\n");
    return check_failures ? 1 : 0;
}
