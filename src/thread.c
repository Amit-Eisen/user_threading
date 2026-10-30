#define _GNU_SOURCE

#include "thread.h"

#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/*
 * User-level round-robin threads on Linux x86_64 / glibc.
 *
 * Preemption is SIGVTALRM from setitimer(ITIMER_VIRTUAL). A context saved
 * inside the handler is resumed with siglongjmp; the handler then returns
 * and sigreturn restores the preempted user code. Voluntary switches
 * (sleep, block, exit) save user context directly.
 *
 * The handler runs on the thread stack, so every translation unit is built
 * with -mno-red-zone. An alternate signal stack would be shared and would
 * smash the frame this technique needs.
 *
 * SP and PC in glibc's jmp_buf are stored mangled with the TCB pointer guard.
 */

#if defined(__x86_64__)
#define JB_SP 6
#define JB_PC 7
#else
#error "This scheduler supports Linux x86_64 with glibc only."
#endif

typedef unsigned long address_t;

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_SLEEPING,
    THREAD_TERMINATED
} thread_state;

typedef struct {
    int tid;
    thread_state state;
    int sleep_quantums;
    thread_start_func entry;
    char stack[STACK_SIZE] __attribute__((aligned(16)));
    sigjmp_buf env;
} thread_t;

static thread_t threads[MAX_THREADS];
static int current_tid;
static int quantum_usecs;
static int initialized;
static volatile sig_atomic_t in_scheduler;

static address_t translate_address(address_t addr)
{
    __asm__ volatile("xor %%fs:0x30, %0\n\t"
                     "rol $0x11, %0"
                     : "+r"(addr));

    return addr;
}

static void signals_block(sigset_t *previous)
{
    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &block, previous);
}

static void signals_restore(const sigset_t *previous)
{
    sigprocmask(SIG_SETMASK, previous, NULL);
}

static int find_next_ready(void)
{
    int next = (current_tid + 1) % MAX_THREADS;

    for (int n = 0; n < MAX_THREADS - 1; n++)
    {
        if (threads[next].state == THREAD_READY) 
        {
            return next;
        }
        next = (next + 1) % MAX_THREADS;
    }

    return -1;
}

/*
 * in_signal: context was saved inside the timer handler, so SIGVTALRM must
 * stay blocked until that handler returns and sigreturn restores the mask.
 * Voluntary saves drop SIGVTALRM from the saved mask so the slice can fire
 * again when that thread resumes.
 */
static void switch_to(int next, int in_signal)
{
    volatile int next_tid = next;
    volatile int from_signal = in_signal;

    if (sigsetjmp(threads[current_tid].env, 1) != 0)
    {
        in_scheduler = 0;

        return;
    }

    if (!from_signal)
    {
        sigdelset(&threads[current_tid].env->__saved_mask, SIGVTALRM);
    }

    current_tid = (int)next_tid;
    threads[current_tid].state = THREAD_RUNNING;
    in_scheduler = 0;
    siglongjmp(threads[current_tid].env, 1);
}

static void schedule(int account_quantum, int in_signal)
{
    sigset_t previous;
    signals_block(&previous);

    if (in_scheduler)
    {
        signals_restore(&previous);

        return;
    }

    in_scheduler = 1;

    if (account_quantum)
    {
        for (int i = 0; i < MAX_THREADS; i++)
        {
            if (threads[i].state != THREAD_SLEEPING)
            {
                continue;
            }

            if (threads[i].sleep_quantums > 0)
            {
                threads[i].sleep_quantums--;
            }

            if (threads[i].sleep_quantums <= 0)
            {
                threads[i].state = THREAD_READY;
            }
        }

        if (threads[current_tid].state == THREAD_RUNNING)
        {
            threads[current_tid].state = THREAD_READY;
        }
    }

    int next = find_next_ready();

    if (next < 0)
    {
        if (threads[current_tid].state == THREAD_READY)
        {
            threads[current_tid].state = THREAD_RUNNING;
        }
        else
        {
            exit(0);
        }

        in_scheduler = 0;
        signals_restore(&previous);

        return;
    }

    switch_to(next, in_signal);
}

static void alarm_handler(int sig) 
{
    (void)sig;
    schedule(1, 1);
}

static void thread_startup(void) 
{
    thread_start_func fn = threads[current_tid].entry;
    fn();
    thread_exit(current_tid);
    exit(0);
}

static void arm_context(int tid) 
{
    volatile int id = tid;

    if (sigsetjmp(threads[id].env, 1) != 0)
    {
        return;
    }

    address_t top = (address_t)(threads[id].stack + STACK_SIZE);
    address_t sp = (top & ~(address_t)0xF) - (address_t)8;
    threads[id].env->__jmpbuf[JB_SP] = (long)translate_address(sp);
    threads[id].env->__jmpbuf[JB_PC] =
        (long)translate_address((address_t)thread_startup);
    sigemptyset(&threads[id].env->__saved_mask);
}

int thread_system_init(int quantum)
{
    if (initialized || quantum <= 0 || quantum > 1000000)
    {
        return -1;
    }

    quantum_usecs = quantum;
    current_tid = 0;
    in_scheduler = 0;

    for (int i = 0; i < MAX_THREADS; i++)
    {
        threads[i].tid = i;
        threads[i].state = THREAD_TERMINATED;
        threads[i].sleep_quantums = 0;
        threads[i].entry = NULL;
    }

    threads[0].state = THREAD_RUNNING;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGVTALRM);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
        return -1;
    }

    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    timer.it_value.tv_sec = quantum_usecs / 1000000;
    timer.it_value.tv_usec = quantum_usecs % 1000000;
    timer.it_interval = timer.it_value;
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL) < 0) {
        return -1;
    }

    initialized = 1;
    return 0;
}

void thread_system_shutdown(void) {
    if (!initialized) {
        return;
    }

    struct itimerval stop;
    memset(&stop, 0, sizeof(stop));
    setitimer(ITIMER_VIRTUAL, &stop, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGVTALRM, &sa, NULL);

    sigset_t unblock;
    sigemptyset(&unblock);
    sigaddset(&unblock, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &unblock, NULL);

    initialized = 0;
}

int thread_create(thread_start_func func) {
    sigset_t previous;
    signals_block(&previous);

    if (!initialized || func == NULL) {
        signals_restore(&previous);
        return -1;
    }

    int tid = -1;
    for (int i = 1; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_TERMINATED) {
            tid = i;
            break;
        }
    }
    if (tid < 0) {
        signals_restore(&previous);
        return -1;
    }

    threads[tid].tid = tid;
    threads[tid].entry = func;
    threads[tid].sleep_quantums = 0;
    arm_context(tid);
    threads[tid].state = THREAD_READY;

    signals_restore(&previous);
    return tid;
}

int thread_exit(int tid) {
    sigset_t previous;
    signals_block(&previous);

    if (!initialized || tid < 0 || tid >= MAX_THREADS ||
        threads[tid].state == THREAD_TERMINATED) {
        signals_restore(&previous);
        return -1;
    }

    if (tid == 0) {
        signals_restore(&previous);
        thread_system_shutdown();
        exit(0);
    }

    threads[tid].state = THREAD_TERMINATED;
    threads[tid].entry = NULL;
    threads[tid].sleep_quantums = 0;

    if (tid == current_tid) {
        schedule(0, 0);
        exit(0);
    }

    signals_restore(&previous);
    return 0;
}

int thread_block(int tid) {
    sigset_t previous;
    signals_block(&previous);

    if (!initialized || tid <= 0 || tid >= MAX_THREADS) {
        signals_restore(&previous);
        return -1;
    }
    if (threads[tid].state != THREAD_READY &&
        threads[tid].state != THREAD_RUNNING) {
        signals_restore(&previous);
        return -1;
    }

    threads[tid].state = THREAD_BLOCKED;
    if (tid == current_tid) {
        schedule(0, 0);
    }

    signals_restore(&previous);
    return 0;
}

int thread_unblock(int tid) {
    sigset_t previous;
    signals_block(&previous);

    if (!initialized || tid < 0 || tid >= MAX_THREADS ||
        threads[tid].state != THREAD_BLOCKED) {
        signals_restore(&previous);
        return -1;
    }

    threads[tid].state = THREAD_READY;
    signals_restore(&previous);
    return 0;
}

int thread_sleep(int num_quantums) {
    sigset_t previous;
    signals_block(&previous);

    if (!initialized || current_tid == 0 || num_quantums <= 0) {
        signals_restore(&previous);
        return -1;
    }

    threads[current_tid].state = THREAD_SLEEPING;
    threads[current_tid].sleep_quantums = num_quantums;
    schedule(0, 0);

    signals_restore(&previous);
    return 0;
}
