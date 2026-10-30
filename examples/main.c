#include "thread.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t tid_a;
static volatile sig_atomic_t tid_b;
static volatile sig_atomic_t tid_c;
static volatile sig_atomic_t tid_d;
static volatile sig_atomic_t tid_e;

static volatile sig_atomic_t done_a;
static volatile sig_atomic_t done_b;
static volatile sig_atomic_t done_c;
static volatile sig_atomic_t done_d;
static volatile sig_atomic_t done_e;

static void safe_printf(const char *fmt, ...)
{
    sigset_t block;
    sigset_t previous;
    sigemptyset(&block);
    sigaddset(&block, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &block, &previous);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);

    va_end(args);
    fflush(stdout);

    sigprocmask(SIG_SETMASK, &previous, NULL);
}

static volatile sig_atomic_t a_ready;
static volatile sig_atomic_t release_a;

static void thread_a(void)
{
    safe_printf("[Thread A] Running 0\n");
    a_ready = 1;

    while (!release_a) {
    }

    for (int i = 1; i < 4; i++) 
    {
        safe_printf("[Thread A] Running %d\n", i);
    }

    done_a = 1;
}

static void thread_b(void)
{
    for (int i = 0; i < 3; i++)
    {
        safe_printf("[Thread B] Sleeping for 3 quantums...\n");
        thread_sleep(3);
        safe_printf("[Thread B] Woke up. Count = %d\n", i + 1);
    }

    done_b = 1;
}

static void thread_c(void)
{
    while (!a_ready) {
    }

    safe_printf("[Thread C] Blocking Thread A\n");

    if (thread_block((int)tid_a) != 0)
    {
        safe_printf("[Thread C] Thread A was not blockable\n");
    } 
    else 
    {
        safe_printf("[Thread C] Sleeping 5 quantums before unblocking Thread A\n");
        thread_sleep(5);
        safe_printf("[Thread C] Unblocking Thread A\n");
        thread_unblock((int)tid_a);
    }

    release_a = 1;
    safe_printf("[Thread C] Exiting\n");
    done_c = 1;
}

static void thread_d(void)
{
    for (int i = 0; i < 3; i++)
    {
        safe_printf("[Thread D] Loop %d\n", i);
        thread_sleep(2);
    }

    safe_printf("[Thread D] Exiting\n");
    done_d = 1;
}

static void thread_e(void)
{
    for (int i = 0; i < 6; i++)
    {
        safe_printf("[Thread E] Running %d\n", i);
    }

    done_e = 1;
}

int main(void)
{
    alarm(20);
    safe_printf("[Main] Initializing thread system...\n");
    // Initialize the thread system with a quantum time of 100000
    if (thread_system_init(100000) != 0)
    {
        safe_printf("[Main] Failed to initialize.\n");

        return 1;
    }

    sigset_t block;
    sigset_t previous;
    sigemptyset(&block);
    sigaddset(&block, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &block, &previous);
    tid_a = thread_create(thread_a);
    tid_b = thread_create(thread_b);
    tid_c = thread_create(thread_c);
    tid_d = thread_create(thread_d);
    tid_e = thread_create(thread_e);
    sigprocmask(SIG_SETMASK, &previous, NULL);

    safe_printf("[Main] Created threads: A=%d B=%d C=%d D=%d E=%d\n",
                (int)tid_a, (int)tid_b, (int)tid_c, (int)tid_d, (int)tid_e);

    if (tid_a < 1 || tid_b < 1 || tid_c < 1 || tid_d < 1 || tid_e < 1)
    {
        thread_system_shutdown();

        return 1;
    }

    for (int i = 1; i <= 10; i++)
    {
        safe_printf("[Main] Loop %d\n", i);
    }

    while (!(done_a && done_b && done_c && done_d && done_e))
    {
    }

    thread_system_shutdown();
    safe_printf("[Main] All threads finished.\n");

    return 0;
}
