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
| `job_system` | `start(desc)` / `stop()`, `schedule(fn)` and `schedule(span)` returning a `job_handle`, `schedule(span, fence)` signalling a caller-owned `job_fence`, `wait(handle)` / `wait(fence)`, `wait_idle()`, `parallel_for(count, grain, body)` and `parallel_for(count, body)`, `thread_index()`, `current()`, `stats()`. |
| `job_handle` | Shared reference to a 64-bit completion counter: `valid()`, `is_complete()`, `pending()`, `reset()`. Invalid handles are complete. |
| `job_fence` | Caller-owned 64-bit completion counter for `schedule(span, fence)`: `is_complete()`, `pending()`. Not copyable or movable (queued jobs hold its address); batches accumulate on it until `wait(fence)`; reusable afterwards; destroying one with pending jobs asserts. |
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
- **Queue.** One lock-guarded queue with three fixed-capacity ring lanes (one per priority) of
  `maxQueuedJobs` slots each. Jobs are `job_block`s taken from a pool; a lane holds block pointers.
  Popping moves the block's contents onto the executing thread's stack and returns the block to the
  pool inside the same critical section, so the pool only ever holds as many blocks as are queued.
  The pool starts at 16 blocks and doubles up to `maxQueuedJobs`; `stats().poolGrowths` counts the
  growths since `start()`, which lets tests assert that steady-state dispatch allocates nothing.
  The lock is a spinlock that parks (`queue_lock`: a test-and-test-and-set word with exponential
  backoff between re-reads; after a bounded spin, `QUEUE_LOCK_SPIN_STEPS`, about the idle window, a
  waiter parks on a mutex and condition variable and the holder's unlock wakes it), not a
  `std::mutex`: the critical sections are a handful of pointer moves and a parked `std::mutex` waiter
  cost a kernel wake-up per queued job (`core.job_parallel_for_1m` with 16 workers: 64 ranges took
  0.100 ms with `std::mutex` and 0.042 ms in the performance round's enforced run, measured with the
  ticket spinlock the round first used; a flag spinlock without backoff, whose release starts a storm
  of exchanges on its line, landed in between). The ticket lock was replaced after review because it
  is FIFO: a waiter that lost its core (the engine's pool is `hardware_concurrency - 1` workers, so
  any other runnable thread preempts one) stalled every later ticket for a scheduler quantum, where a
  preempted waiter of the current lock delays nobody and a preempted holder puts the contenders to
  sleep as the `std::mutex` did. The parking also covers the two critical sections that are not
  short, pool growth and a closure copy that allocates. Re-measured with the current lock on an idle
  machine (`build/perf_final_dev_run.json`): `core.job_parallel_for_1m` 0.052 ms (p99 0.085),
  `scene.transform_propagation_10k_sparse` 0.136 and `_10k` 0.173: the dispatch-bound rows pay about
  0.01 ms for the compare-exchange and backoff path against the ticket's fetch-add, all far inside their
  budgets; the uncontended and lightly contended paths are the same single word. The lock
  word, `mQueuedCount` and `mActiveJobs` each sit between full cache lines of padding (`mActiveJobs`
  shares its line with `mIdleWaiterDepth`, written only when a job enters or leaves `wait_idle()`)
  and the statistics are counted per thread (`thread_stats`, one slot per worker and one shared by
  every other thread: the main thread, dedicated threads, workers of other systems; `stats()` sums
  the slots in use), so the lines a popping worker writes outside the critical section, its
  counters, are its own; the lane ring, the free list and the block itself are serialised by the lock
  and still move between cores once per pop. The two condition variables are
  `std::condition_variable_any` over that lock.
- **Workers.** `workerCount` threads named `<prefix>-<index>` (`SetThreadDescription` on Windows,
  `pthread_setname_np` elsewhere, in `thread_name.cpp`). `start()` prepares the names in the OS
  encoding (`detail::prepared_thread_name`) before creating the threads and returns only after every
  worker has named itself and entered the queue loop, so a worker allocates nothing once `start()`
  has returned and an allocation gate opened right after it is never charged a worker prologue. An
  idle worker keeps re-checking the queue for a bounded time (`IDLE_SPIN_WINDOW`, 50 us: a `pause`
  instruction per check, which hands the core's resources to its SMT sibling, and a `yield()` every
  64th so an oversubscribed machine's runnable threads still get the core) and then sleeps on a
  condition variable. The window hides the kernel wake-up of tens of microseconds for dispatches less
  than 50 us apart (the consecutive dispatched levels of the transform pass, which is what was
  measured); a stage boundary that runs main-thread systems or flushes command buffers between two
  dispatches still parks the workers, and the next dispatch pays one wake-up per worker. The price is
  per dispatch: every worker that was awake spins the window once the queue runs dry, so 16 workers
  cost 0.8 ms of CPU per dispatch (3-6 % of each core at 10-20 dispatches per 60 Hz frame) and
  nothing in a frame that dispatches nothing. `thread_index()` and `current()` are `thread_local`s
  set by the worker on entry (0 / `nullptr` everywhere else).
- **Inline execution.** With zero workers, before `start()`, after `stop()`, or when the queue is
  full, the scheduling thread runs the job itself before `schedule` returns; the handle is then
  already complete. Workers of a stopping system may still queue (they drain their own work), so a
  job that schedules a follow-up during `stop()` is never lost.
- **Handles versus fences.** `schedule(fn)` and `schedule(span)` allocate one `std::shared_ptr`
  counter per call (the handle may outlive the system and every queued block). `schedule(span,
  fence)` queues the same batch under a `job_fence` the caller owns, adding the batch's count to it
  before anything is queued; blocks then carry a plain pointer and no shared reference, so once the
  block pool has warmed up the dispatch allocates nothing (`test_job_system.cpp` gates it). This is
  the per-frame path: the ECS scheduler runs every stage on a fence held on `run_phase`'s stack, as
  `parallel_for` already did with its range counter.
- **Waiting.** `wait()` and `wait_idle()` help: they pop and execute queued jobs until the counter
  reaches zero. When the queue is empty they spin for the same `IDLE_SPIN_WINDOW` (the last range of
  a `parallel_for` is usually microseconds away) and only then sleep, on a process-wide completion
  monitor (mutex + condition variable + blocked-waiter count). Finishing a job that drops a counter or the
  active count to zero (or the active count to the published idle-waiter depth, below), and queueing
  new work, wakes the monitor only when somebody is blocked, so the hot path never locks it. The shared monitor also lets a handle from one system be waited on
  through another. Waiting from a worker therefore never deadlocks, including on a single worker.
- **wait_idle() from inside a job.** Every thread keeps a chain of `execution_frame`s, one per
  queued job it is nested inside (through helping). Outside every job `wait_idle()` waits for the
  active count to reach zero. Inside a job the caller's own frames of that system stay active until
  it returns from them, so it publishes them to the system's `mIdleWaiterDepth` and waits for
  `active <= idleWaiterDepth`: jobs blocked in `wait_idle()` are excluded for *every* waiter, which
  is what lets two in-flight jobs that both call `wait_idle()` return instead of waiting for each
  other forever. Frames an enclosing `wait_idle()` on the same thread already published are not
  counted twice, so a nested call still waits for every unrelated job. `finish_job` wakes the monitor
  whenever the active count drops to the depth, because the last sibling of the waiter's own batch
  (one `schedule(span)` handle, one fence, the ranges of one `parallel_for`) drops neither their
  shared counter nor the active count to zero and would otherwise leave the parked waiter asleep for
  good; `test_job_system.cpp` pins the batch and the `parallel_for` case. The price is documented in
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
at once on 1 and 8 workers, a `wait_idle()` inside a batch job and inside a `parallel_for` range whose
sibling of the same batch finishes after the waiter has parked, a worker of one system scheduling a
batch into another (counted in the external slot), and `stop()`/`start()` cycling underneath four
producer threads.
