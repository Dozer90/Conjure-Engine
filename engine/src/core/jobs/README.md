# core/jobs

The engine's threading primitives: a worker pool with a priority-laned job queue, `parallel_for`,
the main-thread task queue and named dedicated threads. This is the only area of the engine that
creates `std::thread` / `std::jthread` objects (`docs/design/10_core.md` 2.7, C4).

Public headers: `conjure/core/jobs/job_system.h`, `job_handle.h`, `parallel_for.h`,
`main_thread.h`, `dedicated_thread.h` and the umbrella `jobs.h`. Library `conjure::core_jobs`,
depending only on `conjure::core_foundation` (std containers are used internally).

Toolchain requirement: the spec-mandated `dedicated_thread(std::string, std::function<void(std::stop_token)>)`
signature needs `<stop_token>` / `std::jthread`, i.e. libstdc++ 10+, MSVC 19.28+ or libc++ 18+
(Xcode 16+). libc++ 16 has neither and libc++ 17 only ships them behind `-fexperimental-library`.
`engine/src/core/jobs/CMakeLists.txt` checks for them at configure time and fails with a clear
message; every consumer of the umbrella header inherits the requirement.

## Types

| Type | Purpose |
|---|---|
| `job_system` | `start(desc)` / `stop()`, `schedule(fn)` and `schedule(span)` returning a `job_handle`, `wait(handle)`, `wait_idle()`, `parallel_for(count, grain, body)` and `parallel_for(count, body)`, `thread_index()`, `current()`, `stats()`. |
| `job_handle` | Shared reference to a 64-bit completion counter: `valid()`, `is_complete()`, `pending()`, `reset()`. Invalid handles are complete. |
| `job_system_desc` | `workerCount` (0 = inline, at most `MAX_WORKER_COUNT` = 256), `maxQueuedJobs` (default 16384, at most `MAX_QUEUED_JOBS` = 2^20), `threadNamePrefix`. |
| `job_system_stats` | `scheduled`, `executed`, `stolen`, `helped`, `inlineExecutions`, `queueFullFallbacks`, `maxQueueDepth`, `queuedNow`, `poolGrowths`, `poolCapacity`. `stolen` counts jobs executed by a worker other than the thread that queued them; a job a worker queues and runs itself is neither stolen nor helped, so `stolen + helped + inlineExecutions == executed` only holds without such jobs. |
| `job_priority` | `HIGH`, `NORMAL`, `LOW`; workers always take the highest lane with work. |
| `parallel_for.h` | `default_grain`, `range_count`, `parallel_for_range`, `parallel_for_each` (any callable, no `std::function` at the call site). |
| `main_thread.h` | `is_main_thread`, `set_main_thread`, `reset_main_thread`, `main_thread_id`, `post_to_main_thread`, `drain_main_thread_queue`, `main_thread_queue_size`. |
| `dedicated_thread` | Named `std::jthread` wrapper with a `std::stop_token` body; destruction requests a stop and joins. |

## Design

- **Structure.** `job_system` is a thin facade over the private `class job_system::state`
  (`job_system.cpp`), which owns the worker threads, the queue, the block pool, the wait loops and
  the statistics. Everything a scheduler reads outside a lock (`running`, `workerCount`, the queue
  depth, the active-job count) is an atomic, so a `stop()`/`start()` pair never races a reader.
- **Queue.** One mutex-guarded queue with three fixed-capacity ring lanes (one per priority) of
  `maxQueuedJobs` slots each. Jobs are `job_block`s taken from a pool; a lane holds block pointers.
  Popping moves the block's contents onto the executing thread's stack and returns the block to the
  pool inside the same critical section, so the pool only ever holds as many blocks as are queued.
  The pool starts at 16 blocks and doubles up to `maxQueuedJobs`; `stats().poolGrowths` counts the
  growths since `start()`, which lets tests assert that steady-state dispatch allocates nothing.
- **Workers.** `workerCount` threads named `<prefix>-<index>` (`SetThreadDescription` on Windows,
  `pthread_setname_np` elsewhere, in `thread_name.cpp`). An idle worker re-checks the queue a
  bounded number of times with `yield()` and then sleeps on a condition variable. `thread_index()`
  and `current()` are `thread_local`s set by the worker on entry (0 / `nullptr` everywhere else).
- **Inline execution.** With zero workers, before `start()`, after `stop()`, or when the queue is
  full, the scheduling thread runs the job itself before `schedule` returns; the handle is then
  already complete. Workers of a stopping system may still queue (they drain their own work), so a
  job that schedules a follow-up during `stop()` is never lost.
- **Waiting.** `wait()` and `wait_idle()` help: they pop and execute queued jobs until the counter
  reaches zero. Only when the queue is empty do they sleep, on a process-wide completion monitor
  (mutex + condition variable + blocked-waiter count). Finishing a job that drops a counter or the
  active count to zero, and queueing new work, wakes the monitor only when somebody is blocked, so
  the hot path never locks it. The shared monitor also lets a handle from one system be waited on
  through another. Waiting from a worker therefore never deadlocks, including on a single worker.
- **wait_idle() from inside a job.** Every thread keeps a chain of `execution_frame`s, one per
  queued job it is nested inside (through helping). Outside every job `wait_idle()` waits for the
  active count to reach zero. Inside a job the caller's own frames of that system stay active until
  it returns from them, so it publishes them to the system's `mIdleWaiterDepth` and waits for
  `active <= idleWaiterDepth`: jobs blocked in `wait_idle()` are excluded for *every* waiter, which
  is what lets two in-flight jobs that both call `wait_idle()` return instead of waiting for each
  other forever. Frames an enclosing `wait_idle()` on the same thread already published are not
  counted twice, so a nested call still waits for every unrelated job. The price is documented in
  `job_system.h`: only a call made outside every job guarantees a completely idle system.
- **parallel_for.** Splits `[0, count)` into `ceil(count / grain)` ranges; the caller runs range 0
  and every range that did not fit in the queue, then helps until the stack-allocated counter hits
  zero. The counter is 64 bits wide so no range count can truncate it. The body is referenced
  through a `(fn pointer, context)` pair, never copied, and all ranges are queued under one lock
  acquisition. Nested `parallel_for` and `parallel_for` from inside a job work because waiting
  helps. Inline when `count <= grain`, when there are no workers or when the system is not running.
- **Main thread.** The designated main thread is the first caller of `set_main_thread()` or the
  constructor of the first `job_system`; absent both, the first thread to call `is_main_thread()`.
  `post_to_main_thread` is a mutex-guarded vector; `drain_main_thread_queue` swaps it out before
  running, so tasks posted during a drain run on the next one. Called off the designated main
  thread (or before one is designated) it asserts, runs nothing, leaves the tasks queued and returns
  0; it compares against `main_thread_id()` and never claims the designation for the caller.
- **Errors.** `start()` returns `INVALID_STATE` when already running and `INVALID_ARGUMENT` for a
  `maxQueuedJobs` of zero or above `MAX_QUEUED_JOBS`, more than `MAX_WORKER_COUNT` (256) workers or
  an empty name prefix. Empty jobs/bodies assert and are skipped. Exceptions from jobs are
  unsupported (engine code never throws).

## Threading model

- `schedule`, `wait`, `wait_idle`, `parallel_for`, `stats`, `is_running`, `worker_count` are safe
  from any thread, including from inside jobs. `start`/`stop` are serialised by a lifecycle mutex;
  `stop()` must not be called from one of the system's own workers (asserts and returns).
- `job_handle` is safe to copy and query from any thread; the system that produced it does not
  need to be alive for `is_complete()`.
- `post_to_main_thread` / `main_thread_queue_size` are thread-safe; `drain_main_thread_queue`
  asserts and runs nothing off the main thread.
- `wait_idle()` from inside a job never waits for jobs that are themselves blocked in
  `wait_idle()`; see "Design" above.
- `dedicated_thread` is move-only; `request_stop()` may be called from any thread.

## Testing

`tests/core/jobs/` builds `conjure_core_jobs_tests` (`python tools/build.py --build-dir
build/core_jobs --target conjure_core_jobs_tests --test -R core_jobs`). One file per public header
(`test_job_system.cpp`, `test_job_handle.cpp`, `test_parallel_for.cpp`, `test_main_thread.cpp`,
`test_dedicated_thread.cpp`, `test_jobs.cpp`). Every behavioural case runs twice through the
`CONJURE_JOBS_WORKER_SUBCASES` macro with 0 and 8 workers (`[parallel]` in the name). The
allocation-free steady state of `parallel_for` is asserted through `stats().poolGrowths` /
`poolCapacity` staying constant across repeated dispatches. Global `operator new`/`delete` are
replaced by the shared `tests/support/src/tracked_global_new.cpp` that every test executable
compiles in, so `conjure::testing::allocation_scope` / `CONJURE_CHECK_NO_ALLOCATIONS` can gate a
dispatch on zero heap allocations when a test needs that. The main thread is designated by
`test_main.cpp` (`jobs::set_main_thread()`); the main-thread tests reset and re-claim it. Concurrency
coverage includes `wait()`/`wait_idle()` from inside jobs, two in-flight jobs calling `wait_idle()`
at once on 1 and 8 workers, and `stop()`/`start()` cycling underneath four producer threads.
