#ifndef THREAD_H
#define THREAD_H

#define MAX_THREADS 10
/* Per-thread stack. printf and the signal frame need more than 4KB. */
#define STACK_SIZE (64 * 1024)

typedef void (*thread_start_func)(void);

/*
 * Start the scheduler. quantum_usecs is the virtual-time slice (1 .. 1000000).
 * Call this once, before any other function. Returns 0, or -1 on error.
 */
int thread_system_init(int quantum_usecs);

/* Stop the timer so the process can exit. Safe to call more than once. */
void thread_system_shutdown(void);

/*
 * Start a thread at func. The thread is terminated if func returns.
 * Returns a thread id in 1 .. MAX_THREADS-1, or -1 if func is NULL or no
 * slot is free. Id 0 is the main thread.
 */
int thread_create(thread_start_func func);

/*
 * Terminate a thread. Terminating id 0 ends the process.
 * Returns 0, or -1 if tid is invalid or already terminated.
 */
int thread_exit(int tid);

/*
 * Move a thread to BLOCKED until thread_unblock. The main thread cannot
 * be blocked. Returns 0, or -1 on error.
 */
int thread_block(int tid);

/* Move a BLOCKED thread back to READY. Returns 0, or -1 on error. */
int thread_unblock(int tid);

/*
 * Suspend the calling thread for num_quantums timer ticks.
 * The main thread cannot sleep. Returns 0, or -1 on error.
 */
int thread_sleep(int num_quantums);

#endif
