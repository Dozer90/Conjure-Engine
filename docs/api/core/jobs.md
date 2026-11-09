# Jobs and threading

📘 **Public API wiki** · 🛠 Handbook: [Core module](../../handbook/modules/core.md)

How to run work in parallel in Conjure: the `job_system` worker pool, `parallel_for` over ranges, the main-thread
task queue and named `dedicated_thread`s. Include the umbrella `conjure/core/jobs/jobs.h` (or the individual headers
named below); everything lives in `namespace conjure::core::jobs`. This is the only place in the engine that
creates threads: game and module code never uses `std::thread` directly.

## At a glance

| You want to... | Use |
|---|---|
| Run one piece of work in the background and wait for it later | `job_system::schedule(job)` then `wait(handle)` (`conjure/core/jobs/job_system.h`) |
| Process every element of an array in parallel | `parallel_for_each(system, span, body)` (`conjure/core/jobs/parallel_for.h`) |
| Split a numeric range `[0, count)` across threads | `parallel_for_range(system, count, grain, body)` or `job_system::parallel_for` |
| Run a batch of jobs and wait for all of them at once | `schedule(std::span<const std::function<void()>>)` |
| Do something that must happen on the main thread (window, GPU, editor state) from a job | `post_to_main_thread(task)` (`conjure/core/jobs/main_thread.h`) |
| Check whether you are on the main thread or a worker | `is_main_thread()`, `job_system::thread_index()` |
| Run a long-lived blocking loop (file watcher, pipe reader, audio streaming) | `dedicated_thread` (`conjure/core/jobs/dedicated_thread.h`) |
| Poll whether earlier work has finished without blocking | `job_handle::is_complete()` (`conjure/core/jobs/job_handle.h`) |
| Make a test or tool deterministic | Start the system with `workerCount = 0`: every job runs inline |

## Quick example

```cpp
#include "conjure/core/jobs/jobs.h"

#include <algorithm>
#include <span>
#include <vector>

using namespace conjure;
using namespace conjure::core::jobs;

bool simulate(std::vector<f32> &positions, std::span<const f32> velocities, f32 deltaSeconds) {
    job_system system = {};
    job_system_desc desc = {};
    desc.workerCount = 4;
    if (!system.start(desc).has_value()) {
        return false;
    }

    // Data-parallel: 256 elements per range, the caller helps until every range is done.
    parallel_for_range(system, positions.size(), 256, [&](usize begin, usize end) {
        for (usize index = begin; index < end; ++index) {
            positions[index] += velocities[index] * deltaSeconds;
        }
    });

    // One background job, waited on later. Waiting helps run queued jobs, so it never deadlocks.
    f32 maximum = 0.0f;
    const job_handle scan = system.schedule([&positions, &maximum] {
        for (const f32 value : positions) {
            maximum = std::max(maximum, value);
        }
    });
    system.wait(scan);   // maximum is safe to read from here on

    system.stop();   // also done by the destructor
    return true;
}
```

In a running game you do not create the `job_system`: the engine owns one and hands it to you. The example starts
its own so it is complete on its own.

## Which tool when

| Situation | Tool | Why |
|---|---|---|
| The same operation on many elements | `parallel_for_each` / `parallel_for_range` | One dispatch, no allocation after warm-up, the caller participates, finishes before it returns. |
| A few independent tasks whose results you need this frame | `schedule()` + `wait()` | Fire and collect; `wait` executes queued jobs meanwhile. |
| Work that may finish any time (streaming, compression) | `schedule()` + `is_complete()` polled later | Never block the frame on it. |
| A result that must be applied on the main thread | `post_to_main_thread()` from the job | The engine drains the queue once per frame, before the first update phase. |
| A loop that blocks in an OS call or sleeps | `dedicated_thread` | Blocking a worker would starve the pool. |
| Many tiny tasks | `schedule(span)` batch, or `parallel_for` | One handle for all; per-job scheduling overhead dominates sub-microsecond work. |

> [!TIP]
> Reach for `parallel_for` first. It is the fastest path (the body is referenced, not copied, and ranges are queued
> under one lock), it cannot leak work past the call, and with `workerCount = 0` it degrades to a plain loop.

## The job system

`job_system` is a pool of worker threads sharing one queue with three priority lanes (`job_priority::HIGH`,
`NORMAL`, `LOW`; workers always take the highest lane with work). Configure it with `job_system_desc`:

| Field | Default | Meaning |
|---|---|---|
| `workerCount` | 0 | Worker threads. 0 means every job runs inline on the caller. At most `MAX_WORKER_COUNT` (256). |
| `maxQueuedJobs` | 16384 | Queue capacity per lane. When a lane is full the caller runs the job itself. At most `MAX_QUEUED_JOBS` (2^20). |
| `threadNamePrefix` | `"conjure-worker"` | Workers are named `<prefix>-<index>` in debuggers and profilers. |

`start(desc)` returns `result<void>`: `INVALID_STATE` when already running, `INVALID_ARGUMENT` for a zero or oversized
queue, too many workers or an empty name prefix. `stop()` lets the workers finish everything queued, joins them and
is safe to call twice; the destructor calls it.

`schedule(job, priority)` queues one `std::function<void()>` and returns a `job_handle`. `schedule(span, priority)`
queues a batch under a single handle that completes when every job in it has run. `wait(handle)` blocks until the
handle completes, **executing queued jobs on the calling thread while it waits**, so waiting from inside a job or
on a single worker never deadlocks. `wait_idle()` waits until no job is queued or running.

```cpp
void load_textures();        // your own code
void load_meshes();
void load_sounds();
void draw_loading_screen();

void load_level(job_system &system) {
    std::vector<std::function<void()>> tasks = {};
    tasks.emplace_back([] { load_textures(); });
    tasks.emplace_back([] { load_meshes(); });
    tasks.emplace_back([] { load_sounds(); });

    const job_handle loading = system.schedule(std::span<const std::function<void()>>(tasks), job_priority::LOW);
    while (!loading.is_complete()) {
        draw_loading_screen();   // keep rendering; the workers finish the batch meanwhile
    }
    // Alternatively block here: system.wait(loading) makes this thread help with the queued jobs.
}
```

Inline execution happens in four cases: zero workers, before `start()`, after `stop()` (all deterministic) and when
a lane is full (timing dependent). In every case the handle is already complete when `schedule` returns.

> [!WARNING]
> A job must not block waiting for something only another job will do, unless that other job is queued in the same
> system (then `wait` helps). Blocking on a mutex held by a job that is still queued, or spinning on a flag another
> queued job sets, can starve the pool. Schedule the dependency as a job and `wait()` on its handle instead.

> [!WARNING]
> Capture by reference only what outlives the job. A job scheduled with `schedule()` may run after the function that
> scheduled it has returned; `wait()` on the handle (or `wait_idle()`) before the referenced data goes out of scope.
> `parallel_for` has no such hazard: it returns only after every range has finished.

> [!CAUTION]
> Jobs must not throw (engine code never throws).

> [!WARNING]
> `stop()` must not be called from one of the system's own workers (it asserts and returns). `wait_idle()` called
> from inside a job returns once every *other* job has finished; only a call made outside every job guarantees a
> completely idle system.

### Handles

`job_handle` is a cheap, copyable reference to a completion counter. `valid()` is false for a default-constructed
handle, which also reports itself complete so waiting on it is a no-op. `is_complete()` and `pending()` are safe from
any thread and keep working after the system that produced the handle has stopped. `reset()` drops the reference;
the jobs still run.

### Knowing where you are

`job_system::thread_index()` is 0 on the main thread and on any thread that is not a worker, and `1..N` on the
workers. `job_system::current()` returns the system a worker belongs to, or `nullptr` elsewhere. Use the index to
pick a per-thread scratch buffer without locking (`workerCount + 1` slots). `stats()` returns counters since
`start()` (scheduled, executed, queue depth, ...) for profiling and tests; `job_system_stats` in
`conjure/core/jobs/job_system.h` documents each field and 🛠 Handbook: [Core module](../../handbook/modules/core.md)
explains how the scheduler uses them.

## parallel_for

`job_system::parallel_for(count, grain, body)` splits `[0, count)` into ranges of at most `grain` elements, queues
them, runs the first range and any that did not fit on the caller, then helps until every range is done. The
overload without a grain calls `body(index)` per element with `grain = max(1, count / (workerCount * 4))`. The free
templates in `conjure/core/jobs/parallel_for.h` accept any callable:

```cpp
std::vector<u32> values(1000, 0);

parallel_for_each(system, std::span<u32>(values), [](u32 &value) {   // default grain
    value += 1;
});
parallel_for_each(system, std::span<u32>(values), 16, [](u32 &value) {   // 16 elements per job
    value *= 2;
});
parallel_for_range(system, values.size(), 64, [&values](usize begin, usize end) {
    for (usize index = begin; index < end; ++index) {
        values[index] += static_cast<u32>(index);
    }
});
```

`default_grain(count, workerCount)` and `range_count(count, grain)` are `constexpr` so you can reason about the
split. Nesting works: a body may call `parallel_for` again or schedule jobs. When `count <= grain`, there are no
workers or the system is not running, the whole range runs inline on the caller.

> [!TIP]
> Pick a grain so each range does at least a few microseconds of work. Too fine and the queue traffic dominates; too
> coarse and the caller ends up doing most of it alone. The default grain (four ranges per worker) is a good start.

> [!WARNING]
> Ranges run concurrently. Write only to `values[index]` for indices in your own range; accumulate totals into
> per-range locals or `std::atomic`s, never into a shared plain variable.

## The main thread

Some things only work on the main thread: window and input calls, GPU submission, editor state, `event_bus::publish`
and `dispatch_queued`. `conjure/core/jobs/main_thread.h` lets other threads hand work over:

```cpp
std::vector<u8> decode_texture(std::string_view path);   // your own code, any thread
void upload_to_gpu(const std::vector<u8> &pixels);       // your own code, main thread only

void stream_texture(job_system &system, std::string path) {
    // Owned copy moved into the job: the job outlives this call, so a string_view would dangle.
    static_cast<void>(system.schedule([path = std::move(path)] {
        std::vector<u8> pixels = decode_texture(path);        // heavy, off the main thread
        post_to_main_thread([pixels = std::move(pixels)] {
            upload_to_gpu(pixels);                            // runs on the next drain, on the main thread
        });
    }));
}
```

`post_to_main_thread(task)` is thread-safe and queues the task; `drain_main_thread_queue()` runs every task posted
so far, in order, and returns how many ran. Tasks posted during a drain run on the next one. The engine drains the
queue once per frame, before the first update phase; if you build your own loop (a tool, a test), call it yourself.
`main_thread_queue_size()` reports what is waiting.

`is_main_thread()` is true on the designated main thread: the first thread that called `set_main_thread()` or
constructed a `job_system` (the engine does this at startup), or, when neither has happened yet, the first thread
to ask. `main_thread_id()` returns its id without claiming anything; `reset_main_thread()` forgets the designation
(tests, restarting the engine in-process).

> [!IMPORTANT]
> `drain_main_thread_queue()` called off the designated main thread asserts, runs nothing and leaves the tasks queued.
> In a standalone tool, call `set_main_thread()` once at startup before draining.

> [!TIP]
> For a *message* rather than a *task* ("asset X finished loading"), prefer `event_bus::enqueue` so the main thread
> handles it through the normal subscriber path (📘 API: [Time, events and random](time-and-events.md)).

## Dedicated threads

`dedicated_thread` is a named `std::jthread` for the few loops that genuinely block: file watchers, child-process
pipe readers, audio streaming. The body receives a `std::stop_token`; the destructor requests a stop and joins.

```cpp
bool poll_file_system_for_changes();   // your own code: returns quickly, may block briefly
void run_game();

void run_with_asset_watcher() {
    std::atomic<int> changes = 0;
    {
        dedicated_thread watcher("asset-watcher", [&changes](std::stop_token stop) {
            while (!stop.stop_requested()) {
                if (poll_file_system_for_changes()) {
                    changes.fetch_add(1);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        run_game();
    }   // request_stop() + join(): the loop above sees stop_requested() and exits
}
```

`request_stop()` signals without waiting, `join()` waits without signalling, `joinable()`, `name()` and `get_id()`
describe the thread. It is move-only. A dedicated thread is neither a worker (`thread_index()` is 0) nor the main
thread, so it uses `post_to_main_thread` or `event_bus::enqueue` to report back and may schedule jobs and wait on
them like any other thread.

> [!WARNING]
> The body must check `stop_requested()` regularly and return when it is set; otherwise the destructor blocks
> forever. Wake-ups through a condition variable should use `std::condition_variable_any::wait(lock, stop, predicate)`
> so a stop request interrupts the wait.

## What is safe from a worker

Safe from any thread, including inside jobs: `schedule`, `wait`, `wait_idle`, `parallel_for`, `stats`,
`post_to_main_thread`, `main_thread_queue_size`, `is_main_thread`, `job_handle` queries, `event_bus::enqueue`,
logging, `random::default_engine()` (per thread), every function in 📘 API: [Math](math.md), and `memory::counters`
/ `frame_allocator::allocate`.

Main thread only: `event::emit`, `event::subscribe`, `event_bus::publish` / `subscribe` / `dispatch_queued`,
`drain_main_thread_queue`, window, input and GPU calls, and anything a module documents as main-thread. Not
synchronised at all (one thread at a time, your choice which): `arena`, `pool_allocator`, `stopwatch`,
`manual_clock`, containers.

## API summary

| Signature (short) | Purpose |
|---|---|
| `job_system_desc{workerCount, maxQueuedJobs, threadNamePrefix}` | Configuration |
| `result<void> job_system::start(const job_system_desc &)`, `void stop()` | Lifecycle |
| `bool is_running() const`, `u32 worker_count() const` | State |
| `job_handle schedule(std::function<void()> job, job_priority = NORMAL)` | One job |
| `job_handle schedule(std::span<const std::function<void()>>, job_priority = NORMAL)` | A batch under one handle |
| `void wait(const job_handle &)`, `void wait_idle()` | Block and help |
| `void parallel_for(usize count, usize grain, const std::function<void(usize, usize)> &)` | Range split |
| `void parallel_for(usize count, const std::function<void(usize)> &)` | Per index, default grain |
| `static u32 thread_index()`, `static job_system *current()` | Where am I |
| `job_system_stats stats() const` | Counters since `start()` |
| `job_handle`: `valid()`, `is_complete()`, `pending()`, `reset()`, `operator==` | Completion handle |
| `constexpr usize default_grain(usize count, u32 workerCount)`, `range_count(usize count, usize grain)` | Split arithmetic |
| `parallel_for_range(system, count, grain, body)` | Any callable `body(begin, end)` |
| `parallel_for_each(system, span, [grain,] body)` | Any callable `body(element)` |
| `bool is_main_thread()`, `void set_main_thread()`, `void reset_main_thread()`, `std::thread::id main_thread_id()` | Main-thread identity |
| `void post_to_main_thread(std::function<void()>)`, `usize drain_main_thread_queue()`, `usize main_thread_queue_size()` | Main-thread queue |
| `dedicated_thread(std::string name, std::function<void(std::stop_token)> body)` | Start a named thread |
| `void request_stop()`, `bool stop_requested() const`, `bool joinable() const`, `void join()` | Control |
| `const std::string &name() const`, `std::thread::id get_id() const` | Identity |
| `MAX_WORKER_COUNT`, `MAX_QUEUED_JOBS`, `JOB_PRIORITY_COUNT` | Limits |

## See also

- 📘 API: [Time, events and random](time-and-events.md) for `event_bus::enqueue` and per-thread random engines
- 📘 API: [Errors and results](errors-and-results.md) for checking the `result<void>` from `start()`
- 📘 API: [Memory](memory.md) for which allocators are thread-safe
- 🛠 Handbook: [Core module](../../handbook/modules/core.md)
