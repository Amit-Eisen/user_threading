# user_threading

A small user-level thread library in C. Threads share one process and are preempted round-robin by `setitimer(ITIMER_VIRTUAL)` and `SIGVTALRM`. Switching uses `sigsetjmp` / `siglongjmp`.

This is a teaching scheduler for Linux x86_64 with glibc. It is not a pthread replacement: threads do not run in parallel, and there is no join, mutex, or yield.

## Limits

- Linux x86_64 and glibc only. The saved stack pointer and program counter depend on glibc's pointer guard (`%fs:0x30`).
- `MAX_THREADS` is 10, including the main thread (id 0).
- Each worker stack is 64KB. The build uses `-mno-red-zone` because the timer handler runs on the thread stack.
- `thread_sleep` measures time in scheduler quantums, not wall-clock milliseconds.
- Do not call `printf` from a thread without blocking `SIGVTALRM` first. A context switch while libc holds the stdio lock deadlocks the process.

## API

| Call | Behavior |
| --- | --- |
| `thread_system_init(quantum_usecs)` | Start the scheduler. Quantum is 1 .. 1000000 microseconds. |
| `thread_system_shutdown()` | Stop the timer so the process can exit. |
| `thread_create(func)` | Run `func` on a new thread. If `func` returns, the thread is terminated. |
| `thread_exit(tid)` | Terminate a thread. Id 0 ends the process. |
| `thread_block(tid)` / `thread_unblock(tid)` | Park a worker and make it runnable again. |
| `thread_sleep(num_quantums)` | Suspend the caller for that many timer ticks. Main cannot sleep. |

## Layout

- `src/thread.h`, `src/thread.c` — public API and the scheduler
- `examples/main.c` — short demo (block, sleep, exit)
- `tests/` — create, capacity, block/unblock, sleep, exit, and bad ids
- `Makefile`, `Dockerfile`

## Build and run

On Linux:

```bash
make test
make demo
./build/demo
```

On Windows, use Docker. The image runs the tests and the demo once during the build, then again when the container starts:

```bash
docker build -t user_threading .
docker run --rm user_threading
```
