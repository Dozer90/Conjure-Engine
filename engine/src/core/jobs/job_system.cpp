#include "conjure/core/jobs/job_system.h"

#include "core/jobs/cpu_pause.h"
#include "core/jobs/thread_name.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::jobs;

/*===========================================================================*/
/*===========================================================================*/
// Thread-local identity and the shared completion monitor

namespace {
    // Blocks the pool starts with; it doubles on demand up to maxQueuedJobs.
    constexpr usize INITIAL_POOL_BLOCKS = 16;

    // One spin-wait step: a pause instruction (jobs::detail::cpu_pause) hands the core's execution resources
    // to its SMT sibling (the worker that is doing the work) for a few dozen cycles, where a yield()
    // syscall keeps them busy. Every YIELD_EVERY_PAUSES steps the spinner yields once so a runnable
    // thread of an oversubscribed machine still gets the core.
    constexpr u32 YIELD_EVERY_PAUSES = 64;

    void spin_step(u32 &steps) noexcept {
        if ((++steps % YIELD_EVERY_PAUSES) == 0) {
            std::this_thread::yield();
        } else {
            jobs::detail::cpu_pause();
        }
    }

    // How long a thread with nothing to run keeps re-checking (a spin_step between checks) before it
    // sleeps: an idle worker on the queue condition variable, a thread in wait() or wait_idle() on the
    // completion monitor. A sleeping thread costs a kernel wake-up of tens of microseconds on both
    // sides of a dispatch (the workers when the ranges are queued, the waiter when the last range
    // finishes), which is more than a level of a 10,000-entity transform pass does in work; a window
    // this long covers the gap between back-to-back dispatches of one frame, and it is bounded so an
    // idle pool never burns a core beyond it (measured against the scene and core benchmarks,
    // tests/benchmarks/README.md).
    constexpr std::chrono::microseconds IDLE_SPIN_WINDOW(50);

    // The spin window of one thread: armed by the first empty check, over when the clock passes the
    // deadline, re-armed after the thread ran a job (another may follow right away).
    struct spin_window {
        std::chrono::steady_clock::time_point deadline = {};
        u32 steps = 0;
        bool armed = false;

        // True while the window is open; takes one spin step (pause, or a yield every
        // YIELD_EVERY_PAUSES) so the caller only re-checks its condition.
        [[nodiscard]] bool keep_spinning() noexcept {
            const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            if (!armed) {
                armed = true;
                deadline = now + IDLE_SPIN_WINDOW;
                steps = 0;
            } else if (now >= deadline) {
                return false;
            }
            spin_step(steps);
            return true;
        }

        void reset() noexcept {
            armed = false;
        }
    };

    thread_local job_system *sCurrentSystem = nullptr;
    thread_local u32 sThreadIndex = 0;

    // One frame per queued job the calling thread is currently executing (nested through helping),
    // innermost first. wait_idle() walks the chain to find the jobs the caller is itself inside of:
    // they stay active until the caller returns from them, so they cannot be waited for.
    struct execution_frame {
        const void *system = nullptr;      // the job_system::state whose job this frame runs
        execution_frame *parent = nullptr;
        bool excludedFromIdle = false;     // published to the system's idle-waiter depth
    };

    thread_local execution_frame *sInnermostFrame = nullptr;

    // One monitor for every job system in the process: a handle produced by one system can be
    // waited on through another, and blocked waiters never reference a system that may be gone.
    // blockedWaiters lets the hot path skip the lock and notify when nobody is sleeping.
    struct completion_monitor {
        std::mutex mutex = {};
        std::condition_variable condition = {};
        std::atomic<u32> blockedWaiters = 0;
    };

    completion_monitor &completion() noexcept {
        static completion_monitor monitor = {};
        return monitor;
    }

    void notify_blocked_waiters() {
        completion_monitor &monitor = completion();
        if (monitor.blockedWaiters.load(std::memory_order_seq_cst) == 0) {
            return;
        }
        // Taking the lock pairs with the waiter's check-then-wait so the wake-up cannot be lost.
        {
            const std::scoped_lock lock(monitor.mutex);
        }
        monitor.condition.notify_all();
    }

    template <typename Predicate>
    void block_until(Predicate &&ready) {
        completion_monitor &monitor = completion();
        std::unique_lock lock(monitor.mutex);
        monitor.blockedWaiters.fetch_add(1, std::memory_order_seq_cst);
        monitor.condition.wait(lock, ready);
        monitor.blockedWaiters.fetch_sub(1, std::memory_order_seq_cst);
    }

    // Spin steps a thread takes on the queue lock before it parks, at least: the bound is checked
    // between backoff runs, so a thread takes up to QUEUE_LOCK_MAX_BACKOFF_STEPS - 1 more. A critical
    // section is a handful of pointer moves, so the lock is normally free again within a few steps;
    // the bound (about the IDLE_SPIN_WINDOW at a pause per step and a yield every 64th) is for the
    // sections that are not (pool growth, a closure copy that allocates) and for a holder that lost
    // its core.
    constexpr u32 QUEUE_LOCK_SPIN_STEPS = 1024;

    // Longest run of spin steps between two re-reads of the lock word. Doubling the run after every
    // failed attempt keeps the release of a contended lock from starting a storm of exchanges on its
    // line: most spinners are mid-run when the word changes and only a few race for it.
    constexpr u32 QUEUE_LOCK_MAX_BACKOFF_STEPS = 16;

    // The queue lock. Its critical sections are a handful of pointer moves, so a spinner hands it
    // over in the time a contended std::mutex spends parking and waking one waiter (about 1.7 us per
    // queued job on Windows, measured as the whole cost of core.job_parallel_for_1m with 16 workers:
    // 64 ranges took 100 us however little work each held). A test-and-test-and-set word with
    // exponential backoff, not a FIFO ticket: a ticket lock stalls every later ticket behind a waiter
    // that lost its core (the pool is sized to hardware_concurrency - 1 workers, so any other runnable
    // thread preempts one), where here a preempted waiter delays nobody. Waiting is bounded: after
    // QUEUE_LOCK_SPIN_STEPS the thread parks on a mutex and condition variable and the holder's
    // unlock wakes it (the CONTENDED state), so a long critical section or a preempted holder puts
    // the contenders to sleep like the std::mutex did instead of keeping every core busy.
    class queue_lock {
        private:
            static constexpr u32 UNLOCKED = 0;
            static constexpr u32 LOCKED = 1;
            static constexpr u32 CONTENDED = 2;   // locked, and a thread is or may be parked

            std::atomic<u32> mState = UNLOCKED;
            [[maybe_unused]] std::array<byte, CACHE_LINE_SIZE> mStatePadding = {};   // park words off the hot line
            std::mutex mParkMutex = {};
            std::condition_variable mParkCondition = {};

        public:
            void lock() {
                if (try_acquire()) {
                    return;
                }
                u32 steps = 0;
                u32 backoff = 1;
                while (steps < QUEUE_LOCK_SPIN_STEPS) {
                    for (u32 step = 0; step < backoff; ++step) {
                        spin_step(steps);
                    }
                    if (mState.load(std::memory_order_relaxed) == UNLOCKED && try_acquire()) {
                        return;
                    }
                    backoff = std::min(backoff * 2, QUEUE_LOCK_MAX_BACKOFF_STEPS);
                }
                park();
            }

            void unlock() {
                if (mState.exchange(UNLOCKED, std::memory_order_release) != CONTENDED) {
                    return;
                }
                // Taking the mutex pairs with the parker's exchange-then-wait so the wake-up cannot be lost.
                {
                    const std::scoped_lock guard(mParkMutex);
                }
                mParkCondition.notify_one();
            }

        private:
            [[nodiscard]] bool try_acquire() noexcept {
                u32 expected = UNLOCKED;
                return mState.compare_exchange_strong(
                  expected, LOCKED, std::memory_order_acquire, std::memory_order_relaxed
                );
            }

            // Marks the lock contended and sleeps until an unlock hands it over. A lock acquired here
            // stays CONTENDED, so its own unlock wakes the next parked thread (a spurious notify when
            // nobody is parked costs one uncontended mutex round trip).
            void park() {
                std::unique_lock guard(mParkMutex);
                while (mState.exchange(CONTENDED, std::memory_order_acquire) != UNLOCKED) {
                    mParkCondition.wait(guard);
                }
            }
    };

    usize lane_index(job_priority priority) noexcept {
        const usize index = static_cast<usize>(priority);
        return index < JOB_PRIORITY_COUNT ? index : static_cast<usize>(job_priority::NORMAL);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Internal state: the scheduler behind the public job_system facade

class job_system::state {
    private:
        enum class job_kind : u8 {
            FUNCTION,
            RANGE
        };

        // One queued unit of work. Blocks live in the pool; a pointer to one sits in a lane while
        // the job is queued and the contents are moved onto the executing thread's stack when popped.
        struct job_block {
            job_kind kind = job_kind::FUNCTION;
            std::function<void()> function = {};
            detail::range_invoke_fn invoke = nullptr;
            const void *context = nullptr;
            usize begin = 0;
            usize end = 0;
            detail::job_counter *counter = nullptr;
            std::shared_ptr<detail::job_counter> sharedCounter = nullptr;   // keeps a handle's counter alive
            std::thread::id scheduler = {};
        };

        // Fixed-capacity ring of block pointers; one per priority.
        struct lane {
            std::vector<job_block *> slots = {};
            usize head = 0;
            usize count = 0;
        };

        job_system *mOwner = nullptr;              // the facade workers report through current()
        std::atomic<u32> mWorkerCount = 0;         // a hint for schedulers; relaxed reads are enough
        std::atomic<bool> mRunning = false;
        std::mutex mLifecycleMutex = {};           // serialises start() and stop()

        // Worker names in the OS encoding, one per worker (index - 1), built by start() before it
        // creates the threads so a worker's prologue allocates nothing.
        std::vector<detail::prepared_thread_name> mWorkerNames = {};

        // Per-thread statistics: one slot per worker (index 1..N) and one shared by every other thread
        // (0: the main thread, dedicated threads, workers of other systems). Slots are two cache lines
        // apart with the counters leading, so on the 64-byte-line targets no worker's counting shares
        // a line with another worker's whatever the array's own alignment; snapshot() sums the slots
        // in use. Explicit padding instead of alignas (MSVC C4324), as the audio rings do.
        static constexpr usize THREAD_STATS_COUNTERS = 6;
        static constexpr usize THREAD_STATS_PADDING =
          2 * CACHE_LINE_SIZE - THREAD_STATS_COUNTERS * sizeof(std::atomic<u64>);

        struct thread_stats {
            std::atomic<u64> scheduled = 0;
            std::atomic<u64> executed = 0;
            std::atomic<u64> stolen = 0;
            std::atomic<u64> helped = 0;
            std::atomic<u64> inlineExecutions = 0;
            std::atomic<u64> queueFullFallbacks = 0;
            [[maybe_unused]] std::array<byte, THREAD_STATS_PADDING> padding = {};
        };
        static_assert(sizeof(thread_stats) == 2 * CACHE_LINE_SIZE, "thread_stats must span exactly two cache lines");

        // Queue and block pool, guarded by mQueueLock. The lock word (read by every waiting thread)
        // and the two counters every pop and push touch each sit between full lines of padding
        // (mActiveJobs shares its line with mIdleWaiterDepth, written only when a job enters or leaves
        // wait_idle()), so on the 64-byte-line targets no line holding one of them holds anything
        // another thread writes: with 16 workers draining a dispatch, a shared line bounces once per
        // field per job.
        [[maybe_unused]] std::array<byte, CACHE_LINE_SIZE> mLockLeadPadding = {};
        queue_lock mQueueLock = {};
        [[maybe_unused]] std::array<byte, CACHE_LINE_SIZE> mLockTailPadding = {};
        std::condition_variable_any mWorkerCondition = {};
        std::array<lane, JOB_PRIORITY_COUNT> mLanes = {};
        usize mMaxQueuedJobs = 0;
        bool mAcceptingJobs = false;
        bool mStopRequested = false;
        std::vector<std::unique_ptr<job_block[]>> mChunks = {};
        std::vector<job_block *> mFreeBlocks = {};

        // Workers that have finished their prologue (also guarded by mQueueLock). start() blocks on
        // mStartCondition until every worker it created is counted.
        u32 mStartedWorkers = 0;
        std::condition_variable_any mStartCondition = {};

        // Lock-free mirrors read outside the lock.
        [[maybe_unused]] std::array<byte, CACHE_LINE_SIZE> mQueuedLeadPadding = {};
        std::atomic<usize> mQueuedCount = 0;
        [[maybe_unused]] std::array<byte, CACHE_LINE_SIZE> mQueuedTailPadding = {};
        std::atomic<usize> mActiveJobs = 0;        // queued + executing jobs that went through the queue
        std::atomic<usize> mIdleWaiterDepth = 0;   // active jobs owned by threads inside wait_idle()
        [[maybe_unused]] std::array<byte, CACHE_LINE_SIZE> mActiveTailPadding = {};

        std::vector<std::thread> mWorkers = {};

        // Statistics. The per-thread counters are summed by snapshot(); the pool figures change
        // under the lock only.
        std::atomic<usize> mMaxQueueDepth = 0;
        std::atomic<usize> mPoolCapacity = 0;
        std::atomic<u32> mPoolGrowths = 0;
        std::array<thread_stats, static_cast<usize>(MAX_WORKER_COUNT) + 1> mThreadStats = {};

    public:
        explicit state(job_system *owner);
        ~state();

        state(const state &) = delete;
        state &operator=(const state &) = delete;

    public:
        [[nodiscard]] result<void> start(const job_system_desc &desc);
        void stop();
        [[nodiscard]] bool is_running() const noexcept;
        [[nodiscard]] u32 worker_count() const noexcept;

        [[nodiscard]] job_handle schedule(std::function<void()> job, job_priority priority);
        [[nodiscard]] job_handle schedule(std::span<const std::function<void()>> jobs, job_priority priority);
        void schedule(std::span<const std::function<void()>> jobs, detail::job_counter &counter, job_priority priority);
        void wait(const detail::job_counter &counter);
        void wait_idle();
        void parallel_for(usize count, usize grain, detail::range_invoke_fn invoke, const void *context);

        [[nodiscard]] job_system_stats snapshot() const noexcept;

    private:
        [[nodiscard]] bool inline_only() const noexcept;

        // Block pool (mQueueLock held).
        void grow_pool_locked();
        [[nodiscard]] job_block *acquire_block_locked();
        void release_block_locked(job_block *block);

        // Scheduling.
        [[nodiscard]] static u64 count_valid(std::span<const std::function<void()>> jobs) noexcept;
        void enqueue_batch(
          std::span<const std::function<void()>> jobs,
          u64 validCount,
          detail::job_counter *counter,
          const std::shared_ptr<detail::job_counter> &shared,
          job_priority priority
        );

        // Queue (mQueueLock held unless stated otherwise).
        [[nodiscard]] job_block *begin_enqueue_locked(bool fromOwnWorker);
        void commit_enqueue_locked(job_block *block, job_priority priority);
        [[nodiscard]] bool pop_locked(job_block &out);
        [[nodiscard]] bool try_pop(job_block &out);
        void notify_pushed(usize pushed);

        // Execution.
        void finish_job(detail::job_counter *counter, bool wasQueued);
        void execute(job_block &job, bool helping);
        void run_inline(const std::function<void()> &job, detail::job_counter *counter);
        void run_range_inline(detail::range_invoke_fn invoke, const void *context, usize begin, usize end);

        // Waiting.
        [[nodiscard]] bool is_idle_for(usize ownJobs) const noexcept;

        // Statistics.
        [[nodiscard]] thread_stats &my_stats() noexcept;
        [[nodiscard]] u64 sum_stats(std::atomic<u64> thread_stats::*field) const noexcept;

        // Workers.
        [[nodiscard]] bool next_job(job_block &out);
        void worker_main(u32 index);
};

/*===========================================================================*/
/*===========================================================================*/
// state: constructors and destructors

job_system::state::state(job_system *owner) :
    mOwner(owner) {}

job_system::state::~state() {}

/*===========================================================================*/
/*===========================================================================*/
// state: lifecycle

result<void> job_system::state::start(const job_system_desc &desc) {
    const std::scoped_lock lifecycle(mLifecycleMutex);

    if (mRunning.load(std::memory_order_acquire)) {
        return error(error_code::INVALID_STATE, "job_system::start: the system is already running");
    }
    if (desc.maxQueuedJobs == 0) {
        return error(error_code::INVALID_ARGUMENT, "job_system::start: maxQueuedJobs must be at least 1");
    }
    if (desc.maxQueuedJobs > MAX_QUEUED_JOBS) {
        return error(
          error_code::INVALID_ARGUMENT,
          std::format("job_system::start: maxQueuedJobs {} exceeds the limit {}", desc.maxQueuedJobs, MAX_QUEUED_JOBS)
        );
    }
    if (desc.workerCount > MAX_WORKER_COUNT) {
        return error(
          error_code::INVALID_ARGUMENT,
          std::format("job_system::start: workerCount {} exceeds the maximum of {}", desc.workerCount, MAX_WORKER_COUNT)
        );
    }
    if (desc.threadNamePrefix == nullptr || *desc.threadNamePrefix == '\0') {
        return error(error_code::INVALID_ARGUMENT, "job_system::start: threadNamePrefix must not be empty");
    }

    mWorkerCount.store(desc.workerCount, std::memory_order_relaxed);

    // Named here, not on the workers: a worker must not allocate once start() has returned.
    mWorkerNames.clear();
    mWorkerNames.reserve(desc.workerCount);
    for (u32 index = 1; index <= desc.workerCount; ++index) {
        mWorkerNames.push_back(detail::prepare_thread_name(std::format("{}-{}", desc.threadNamePrefix, index)));
    }

    {
        const std::scoped_lock lock(mQueueLock);
        mStartedWorkers = 0;
        mMaxQueuedJobs = desc.maxQueuedJobs;
        for (lane &priorityLane : mLanes) {
            priorityLane.slots.assign(mMaxQueuedJobs, nullptr);
            priorityLane.head = 0;
            priorityLane.count = 0;
        }
        if (mPoolCapacity.load(std::memory_order_relaxed) == 0) {
            grow_pool_locked();
        }
        mAcceptingJobs = desc.workerCount > 0;
        mStopRequested = false;

        for (thread_stats &stats : mThreadStats) {
            stats.scheduled.store(0, std::memory_order_relaxed);
            stats.executed.store(0, std::memory_order_relaxed);
            stats.stolen.store(0, std::memory_order_relaxed);
            stats.helped.store(0, std::memory_order_relaxed);
            stats.inlineExecutions.store(0, std::memory_order_relaxed);
            stats.queueFullFallbacks.store(0, std::memory_order_relaxed);
        }
        mMaxQueueDepth.store(0, std::memory_order_relaxed);
        mPoolGrowths.store(0, std::memory_order_relaxed);
    }

    mRunning.store(true, std::memory_order_release);

    mWorkers.reserve(desc.workerCount);
    for (u32 index = 1; index <= desc.workerCount; ++index) {
        mWorkers.emplace_back([this, index] {
            worker_main(index);
        });
    }

    // Return only once every worker has run its prologue and is pulling jobs: from here on the
    // pool allocates nothing of its own, which callers gating allocations right after start() rely
    // on, and worker_count() describes threads that exist.
    {
        std::unique_lock lock(mQueueLock);
        mStartCondition.wait(lock, [this, &desc] {
            return mStartedWorkers == desc.workerCount;
        });
    }

    CONJURE_LOG_DEBUG(LOG_CORE, "job_system started with {} worker(s)", desc.workerCount);
    return {};
}

void job_system::state::stop() {
    // Checked before the lifecycle lock: a worker blocking on that lock while the owner's stop() holds
    // it and joins the workers would deadlock that stop().
    CONJURE_ASSERT(sCurrentSystem != mOwner, "job_system::stop called from one of its own workers");
    if (sCurrentSystem == mOwner) {
        return;
    }

    const std::scoped_lock lifecycle(mLifecycleMutex);

    if (!mRunning.load(std::memory_order_acquire)) {
        return;
    }

    {
        const std::scoped_lock lock(mQueueLock);
        mStopRequested = true;
        mAcceptingJobs = false;
    }
    mWorkerCondition.notify_all();

    for (std::thread &worker : mWorkers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    mWorkers.clear();

    // Workers drain the queue before they exit and a worker that queues during the drain picks
    // its job up itself, so nothing should be left; this is the safety net.
    job_block job = {};
    while (try_pop(job)) {
        execute(job, true);
    }

    {
        const std::scoped_lock lock(mQueueLock);
        mStopRequested = false;
    }
    mRunning.store(false, std::memory_order_release);

    CONJURE_LOG_DEBUG(
      LOG_CORE, "job_system stopped after executing {} job(s)", sum_stats(&thread_stats::executed)
    );
}

bool job_system::state::is_running() const noexcept {
    return mRunning.load(std::memory_order_acquire);
}

u32 job_system::state::worker_count() const noexcept {
    return is_running() ? mWorkerCount.load(std::memory_order_relaxed) : 0;
}

bool job_system::state::inline_only() const noexcept {
    return !mRunning.load(std::memory_order_acquire) || mWorkerCount.load(std::memory_order_relaxed) == 0;
}

/*===========================================================================*/
/*===========================================================================*/
// state: scheduling

job_handle job_system::state::schedule(std::function<void()> job, job_priority priority) {
    CONJURE_ASSERT(job != nullptr, "job_system::schedule: empty job");
    if (!job) {
        return {};
    }

    std::shared_ptr<detail::job_counter> counter = std::make_shared<detail::job_counter>();
    counter->remaining.store(1, std::memory_order_relaxed);
    job_handle handle(counter);
    my_stats().scheduled.fetch_add(1, std::memory_order_relaxed);

    if (!inline_only()) {
        bool queued = false;
        {
            const std::scoped_lock lock(mQueueLock);
            job_block *block = begin_enqueue_locked(sCurrentSystem == mOwner);
            if (block != nullptr) {
                block->kind = job_kind::FUNCTION;
                block->function = std::move(job);
                block->counter = counter.get();
                block->sharedCounter = counter;
                block->scheduler = std::this_thread::get_id();
                commit_enqueue_locked(block, priority);
                queued = true;
            }
        }
        if (queued) {
            notify_pushed(1);
            return handle;
        }
    }

    run_inline(job, counter.get());
    return handle;
}

job_handle job_system::state::schedule(std::span<const std::function<void()>> jobs, job_priority priority) {
    const u64 validCount = count_valid(jobs);
    if (validCount == 0) {
        return {};
    }

    std::shared_ptr<detail::job_counter> counter = std::make_shared<detail::job_counter>();
    counter->remaining.store(validCount, std::memory_order_relaxed);
    job_handle handle(counter);
    enqueue_batch(jobs, validCount, counter.get(), counter, priority);
    return handle;
}

void job_system::state::schedule(
  std::span<const std::function<void()>> jobs,
  detail::job_counter &counter,
  job_priority priority
) {
    const u64 validCount = count_valid(jobs);
    if (validCount == 0) {
        return;
    }
    // The count is raised before any job is queued so a finishing job can never drive it below zero,
    // and added rather than stored so a fence may gather several batches before one wait().
    counter.remaining.fetch_add(validCount, std::memory_order_seq_cst);
    enqueue_batch(jobs, validCount, &counter, nullptr, priority);
}

u64 job_system::state::count_valid(std::span<const std::function<void()>> jobs) noexcept {
    u64 validCount = 0;
    for (const std::function<void()> &job : jobs) {
        CONJURE_ASSERT(job != nullptr, "job_system::schedule: empty job in batch");
        if (job) {
            ++validCount;
        }
    }
    return validCount;
}

// Queues the 'validCount' non-empty entries of a batch under 'counter' (already holding their count).
// 'shared' is the handle's owning reference, null for a caller-owned fence. As many jobs as fit are
// queued in one critical section; whatever is left runs on the caller.
void job_system::state::enqueue_batch(
  std::span<const std::function<void()>> jobs,
  u64 validCount,
  detail::job_counter *counter,
  const std::shared_ptr<detail::job_counter> &shared,
  job_priority priority
) {
    my_stats().scheduled.fetch_add(validCount, std::memory_order_relaxed);

    usize firstInline = 0;
    if (!inline_only()) {
        usize pushed = 0;
        const std::thread::id self = std::this_thread::get_id();
        const bool fromOwnWorker = sCurrentSystem == mOwner;
        {
            const std::scoped_lock lock(mQueueLock);
            firstInline = jobs.size();
            for (usize index = 0; index < jobs.size(); ++index) {
                if (!jobs[index]) {
                    continue;
                }
                job_block *block = begin_enqueue_locked(fromOwnWorker);
                if (block == nullptr) {
                    firstInline = index;
                    break;
                }
                block->kind = job_kind::FUNCTION;
                block->function = jobs[index];
                block->counter = counter;
                block->sharedCounter = shared;
                block->scheduler = self;
                commit_enqueue_locked(block, priority);
                ++pushed;
            }
        }
        if (pushed > 0) {
            notify_pushed(pushed);
        }
    }

    for (usize index = firstInline; index < jobs.size(); ++index) {
        if (jobs[index]) {
            run_inline(jobs[index], counter);
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// state: waiting

void job_system::state::wait(const detail::job_counter &counter) {
    job_block job = {};
    spin_window spin = {};
    while (counter.remaining.load(std::memory_order_seq_cst) != 0) {
        if (try_pop(job)) {
            execute(job, true);
            spin.reset();
            continue;
        }
        if (spin.keep_spinning()) {
            continue;
        }
        block_until([this, &counter] {
            return counter.remaining.load(std::memory_order_seq_cst) == 0
                || mQueuedCount.load(std::memory_order_seq_cst) > 0;
        });
    }
}

void job_system::state::wait_idle() {
    // The jobs this thread is nested inside stay active until it returns from them, so they are
    // published to mIdleWaiterDepth and excluded from "idle" for every waiter of this system: two
    // in-flight jobs that wait for each other then both return instead of deadlocking. Frames an
    // enclosing wait_idle() on this thread has already published are not counted twice.
    usize ownJobs = 0;
    usize published = 0;
    for (execution_frame *frame = sInnermostFrame; frame != nullptr; frame = frame->parent) {
        if (frame->system != this) {
            continue;
        }
        ++ownJobs;
        if (!frame->excludedFromIdle) {
            frame->excludedFromIdle = true;
            ++published;
        }
    }
    if (published > 0) {
        mIdleWaiterDepth.fetch_add(published, std::memory_order_seq_cst);
        // A waiter that is already blocked has to re-evaluate with the new exclusion.
        notify_blocked_waiters();
    }

    job_block job = {};
    spin_window spin = {};
    while (true) {
        if (try_pop(job)) {
            execute(job, true);
            spin.reset();
            continue;
        }
        if (is_idle_for(ownJobs)) {
            break;
        }
        if (spin.keep_spinning()) {
            continue;
        }
        block_until([this, ownJobs] {
            return is_idle_for(ownJobs) || mQueuedCount.load(std::memory_order_seq_cst) > 0;
        });
    }

    if (published == 0) {
        return;
    }
    mIdleWaiterDepth.fetch_sub(published, std::memory_order_seq_cst);

    // Every frame pushed while helping has been popped again, so the chain is the one seen on
    // entry and the frames this call marked are the innermost marked ones of this system.
    for (execution_frame *frame = sInnermostFrame; frame != nullptr && published > 0; frame = frame->parent) {
        if (frame->system == this && frame->excludedFromIdle) {
            frame->excludedFromIdle = false;
            --published;
        }
    }
}

bool job_system::state::is_idle_for(usize ownJobs) const noexcept {
    const usize active = mActiveJobs.load(std::memory_order_seq_cst);
    // Outside every job the system must be completely idle; inside a job only the jobs that are
    // neither the caller's own nor themselves blocked in wait_idle() count.
    if (ownJobs == 0) {
        return active == 0;
    }
    return active <= mIdleWaiterDepth.load(std::memory_order_seq_cst);
}

/*===========================================================================*/
/*===========================================================================*/
// state: parallel_for

void job_system::state::parallel_for(usize count, usize grain, detail::range_invoke_fn invoke, const void *context) {
    if (count == 0) {
        return;
    }

    const usize effectiveGrain = std::max<usize>(1, grain);
    if (inline_only() || count <= effectiveGrain) {
        my_stats().scheduled.fetch_add(1, std::memory_order_relaxed);
        run_range_inline(invoke, context, 0, count);
        return;
    }

    const usize rangeCount = 1 + (count - 1) / effectiveGrain;
    my_stats().scheduled.fetch_add(rangeCount, std::memory_order_relaxed);

    // The counter and the body live on this stack frame; every queued range has finished (and
    // dropped its pointers) by the time wait() returns.
    detail::job_counter counter = {};
    counter.remaining.store(static_cast<u64>(rangeCount - 1), std::memory_order_relaxed);

    // Range 0 is the caller's. Queue the rest first so workers start while the caller works; a
    // full queue leaves a contiguous tail of ranges for the caller to run after its own.
    usize pushed = 0;
    usize firstInline = rangeCount;
    {
        const std::thread::id self = std::this_thread::get_id();
        const bool fromOwnWorker = sCurrentSystem == mOwner;
        const std::scoped_lock lock(mQueueLock);
        for (usize range = 1; range < rangeCount; ++range) {
            job_block *block = begin_enqueue_locked(fromOwnWorker);
            if (block == nullptr) {
                firstInline = range;
                break;
            }
            block->kind = job_kind::RANGE;
            block->invoke = invoke;
            block->context = context;
            block->begin = range * effectiveGrain;
            block->end = std::min(block->begin + effectiveGrain, count);
            block->counter = &counter;
            block->scheduler = self;
            commit_enqueue_locked(block, job_priority::NORMAL);
            ++pushed;
        }
    }
    if (pushed > 0) {
        notify_pushed(pushed);
    }

    // The caller's own range runs synchronously on the scheduling thread, so it counts as an inline
    // execution like the fallback ranges below; that keeps stolen + helped + inlineExecutions == executed.
    run_range_inline(invoke, context, 0, std::min(effectiveGrain, count));

    for (usize range = firstInline; range < rangeCount; ++range) {
        const usize begin = range * effectiveGrain;
        run_range_inline(invoke, context, begin, std::min(begin + effectiveGrain, count));
        counter.remaining.fetch_sub(1, std::memory_order_acq_rel);
    }

    wait(counter);
}

/*===========================================================================*/
/*===========================================================================*/
// state: statistics

job_system_stats job_system::state::snapshot() const noexcept {
    job_system_stats stats = {};
    stats.scheduled = sum_stats(&thread_stats::scheduled);
    stats.executed = sum_stats(&thread_stats::executed);
    stats.stolen = sum_stats(&thread_stats::stolen);
    stats.helped = sum_stats(&thread_stats::helped);
    stats.inlineExecutions = sum_stats(&thread_stats::inlineExecutions);
    stats.queueFullFallbacks = sum_stats(&thread_stats::queueFullFallbacks);
    stats.maxQueueDepth = mMaxQueueDepth.load(std::memory_order_relaxed);
    stats.queuedNow = mQueuedCount.load(std::memory_order_relaxed);
    stats.poolGrowths = mPoolGrowths.load(std::memory_order_relaxed);
    stats.poolCapacity = mPoolCapacity.load(std::memory_order_relaxed);
    return stats;
}

job_system::state::thread_stats &job_system::state::my_stats() noexcept {
    // sThreadIndex is the worker's index in its own system; a worker of another system counts
    // with the external threads.
    const usize slot = sCurrentSystem == mOwner ? static_cast<usize>(sThreadIndex) : 0;
    return mThreadStats[slot];
}

u64 job_system::state::sum_stats(std::atomic<u64> thread_stats::*field) const noexcept {
    // Slots above the worker count are zero (start() resets every slot and a worker index never
    // exceeds the count), so the walk stops at the slots in use: the frame loop reads stats() twice a
    // frame and a walk over all MAX_WORKER_COUNT slots would load 32 KB each time.
    const usize slotsInUse = static_cast<usize>(mWorkerCount.load(std::memory_order_relaxed)) + 1;
    u64 total = 0;
    for (usize slot = 0; slot < slotsInUse; ++slot) {
        total += (mThreadStats[slot].*field).load(std::memory_order_relaxed);
    }
    return total;
}

/*===========================================================================*/
/*===========================================================================*/
// state: block pool (mQueueLock held)

void job_system::state::grow_pool_locked() {
    const usize currentCapacity = mPoolCapacity.load(std::memory_order_relaxed);
    const usize ceiling = std::max(mMaxQueuedJobs, INITIAL_POOL_BLOCKS);
    const usize target = std::min(std::max(currentCapacity * 2, INITIAL_POOL_BLOCKS), ceiling);
    const usize added = target > currentCapacity ? target - currentCapacity : 1;

    mChunks.push_back(std::make_unique<job_block[]>(added));
    job_block *chunk = mChunks.back().get();

    // Reserve for the whole pool so returning blocks to the free list never reallocates.
    mFreeBlocks.reserve(currentCapacity + added);
    for (usize index = 0; index < added; ++index) {
        mFreeBlocks.push_back(&chunk[index]);
    }

    mPoolCapacity.store(currentCapacity + added, std::memory_order_relaxed);
    mPoolGrowths.fetch_add(1, std::memory_order_relaxed);
}

job_system::state::job_block *job_system::state::acquire_block_locked() {
    if (mFreeBlocks.empty()) {
        grow_pool_locked();
    }
    job_block *block = mFreeBlocks.back();
    mFreeBlocks.pop_back();
    return block;
}

void job_system::state::release_block_locked(job_block *block) {
    block->kind = job_kind::FUNCTION;
    block->function = nullptr;
    block->invoke = nullptr;
    block->context = nullptr;
    block->begin = 0;
    block->end = 0;
    block->counter = nullptr;
    block->sharedCounter.reset();
    block->scheduler = std::thread::id();
    mFreeBlocks.push_back(block);
}

/*===========================================================================*/
/*===========================================================================*/
// state: queue (mQueueLock held unless stated otherwise)

// Returns a block to fill, or nullptr when the caller has to run the job itself: the system is
// stopping (workers keep queueing so the drain stays local to them) or the queue is full.
job_system::state::job_block *job_system::state::begin_enqueue_locked(bool fromOwnWorker) {
    if (!mAcceptingJobs && !fromOwnWorker) {
        return nullptr;
    }
    if (mQueuedCount.load(std::memory_order_relaxed) >= mMaxQueuedJobs) {
        my_stats().queueFullFallbacks.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
    return acquire_block_locked();
}

void job_system::state::commit_enqueue_locked(job_block *block, job_priority priority) {
    lane &target = mLanes[lane_index(priority)];
    target.slots[(target.head + target.count) % target.slots.size()] = block;
    ++target.count;

    mActiveJobs.fetch_add(1, std::memory_order_seq_cst);
    const usize queued = mQueuedCount.fetch_add(1, std::memory_order_seq_cst) + 1;
    if (queued > mMaxQueueDepth.load(std::memory_order_relaxed)) {
        mMaxQueueDepth.store(queued, std::memory_order_relaxed);
    }
}

bool job_system::state::pop_locked(job_block &out) {
    for (lane &source : mLanes) {
        if (source.count == 0) {
            continue;
        }
        job_block *block = source.slots[source.head];
        source.head = (source.head + 1) % source.slots.size();
        --source.count;
        mQueuedCount.fetch_sub(1, std::memory_order_seq_cst);

        out = std::move(*block);
        release_block_locked(block);
        return true;
    }
    return false;
}

// Lock-free when the queue is empty.
bool job_system::state::try_pop(job_block &out) {
    if (mQueuedCount.load(std::memory_order_acquire) == 0) {
        return false;
    }
    const std::scoped_lock lock(mQueueLock);
    return pop_locked(out);
}

// Called after the lock is released.
void job_system::state::notify_pushed(usize pushed) {
    if (pushed >= mWorkerCount.load(std::memory_order_relaxed)) {
        mWorkerCondition.notify_all();
    } else {
        for (usize index = 0; index < pushed; ++index) {
            mWorkerCondition.notify_one();
        }
    }
    // Threads blocked in wait() can help with the new work instead of sleeping.
    notify_blocked_waiters();
}

/*===========================================================================*/
/*===========================================================================*/
// state: execution

// Decrements the handle counter (and the active count for queued jobs) and wakes blocked waiters
// when either reaches zero, or when the active count drops to the published idle-waiter depth: a job
// blocked in wait_idle() waits for active <= depth, and its last sibling finishing drops neither
// their shared counter nor the active count to zero (a two-job batch, the queued ranges of one
// parallel_for). mIdleWaiterDepth shares mActiveJobs' cache line, so the extra load is served from
// the line the decrement just fetched and costs the hot path nothing; its seq_cst pairing with
// wait_idle()'s publish is what makes the wake-up impossible to lose.
void job_system::state::finish_job(detail::job_counter *counter, bool wasQueued) {
    bool wake = false;
    if (counter != nullptr) {
        wake = counter->remaining.fetch_sub(1, std::memory_order_seq_cst) == 1;
    }
    if (wasQueued) {
        const usize stillActive = mActiveJobs.fetch_sub(1, std::memory_order_seq_cst) - 1;
        wake = wake || stillActive <= mIdleWaiterDepth.load(std::memory_order_seq_cst);
    }
    if (wake) {
        notify_blocked_waiters();
    }
}

void job_system::state::execute(job_block &job, bool helping) {
    execution_frame frame = {};
    frame.system = this;
    frame.parent = sInnermostFrame;
    sInnermostFrame = &frame;

    if (job.kind == job_kind::FUNCTION) {
        job.function();
        // Destroy the callable before signalling so captured state is released by the time a
        // waiter continues.
        job.function = nullptr;
    } else {
        job.invoke(job.context, job.begin, job.end);
    }
    sInnermostFrame = frame.parent;

    thread_stats &stats = my_stats();
    stats.executed.fetch_add(1, std::memory_order_relaxed);
    if (helping) {
        stats.helped.fetch_add(1, std::memory_order_relaxed);
    } else if (job.scheduler != std::this_thread::get_id()) {
        stats.stolen.fetch_add(1, std::memory_order_relaxed);
    }

    detail::job_counter *counter = job.counter;
    job.counter = nullptr;
    job.invoke = nullptr;
    job.context = nullptr;
    finish_job(counter, true);
    // The block's reference kept the counter alive through the decrement; release it last.
    job.sharedCounter.reset();
}

void job_system::state::run_inline(const std::function<void()> &job, detail::job_counter *counter) {
    job();
    thread_stats &stats = my_stats();
    stats.executed.fetch_add(1, std::memory_order_relaxed);
    stats.inlineExecutions.fetch_add(1, std::memory_order_relaxed);
    finish_job(counter, false);
}

void job_system::state::run_range_inline(detail::range_invoke_fn invoke, const void *context, usize begin, usize end) {
    invoke(context, begin, end);
    thread_stats &stats = my_stats();
    stats.executed.fetch_add(1, std::memory_order_relaxed);
    stats.inlineExecutions.fetch_add(1, std::memory_order_relaxed);
}

/*===========================================================================*/
/*===========================================================================*/
// state: workers

// Returns false when the system is stopping and nothing is left to run.
bool job_system::state::next_job(job_block &out) {
    spin_window spin = {};
    while (spin.keep_spinning()) {
        if (try_pop(out)) {
            return true;
        }
    }

    std::unique_lock lock(mQueueLock);
    while (true) {
        if (pop_locked(out)) {
            return true;
        }
        if (mStopRequested) {
            return false;
        }
        mWorkerCondition.wait(lock);
    }
}

void job_system::state::worker_main(u32 index) {
    sCurrentSystem = mOwner;
    sThreadIndex = index;
    detail::set_current_thread_name(mWorkerNames[index - 1]);

    // The prologue is over and nothing below allocates; let start() return.
    {
        const std::scoped_lock lock(mQueueLock);
        ++mStartedWorkers;
    }
    mStartCondition.notify_one();

    job_block job = {};
    while (next_job(job)) {
        execute(job, false);
    }

    sCurrentSystem = nullptr;
    sThreadIndex = 0;
}

/*===========================================================================*/
/*===========================================================================*/
// job_system: constructors and destructors

job_system::job_system() :
    mState(std::make_unique<state>(this)) {
    // The thread that creates the first job system is the main thread unless one was designated.
    static_cast<void>(is_main_thread());
}

job_system::~job_system() {
    stop();
}

/*===========================================================================*/
/*===========================================================================*/
// job_system: public methods

result<void> job_system::start(const job_system_desc &desc) {
    return mState->start(desc);
}

void job_system::stop() {
    mState->stop();
}

bool job_system::is_running() const noexcept {
    return mState->is_running();
}

u32 job_system::worker_count() const noexcept {
    return mState->worker_count();
}

job_handle job_system::schedule(std::function<void()> job, job_priority priority) {
    return mState->schedule(std::move(job), priority);
}

job_handle job_system::schedule(std::span<const std::function<void()>> jobs, job_priority priority) {
    return mState->schedule(jobs, priority);
}

void job_system::schedule(std::span<const std::function<void()>> jobs, job_fence &fence, job_priority priority) {
    mState->schedule(jobs, fence.mCounter, priority);
}

void job_system::wait(const job_handle &handle) {
    if (!handle.valid() || handle.is_complete()) {
        return;
    }
    mState->wait(*handle.mCounter);
}

void job_system::wait(const job_fence &fence) {
    if (fence.is_complete()) {
        return;
    }
    mState->wait(fence.mCounter);
}

void job_system::wait_idle() {
    mState->wait_idle();
}

void job_system::parallel_for(usize count, usize grain, const std::function<void(usize, usize)> &body) {
    CONJURE_ASSERT(body != nullptr, "job_system::parallel_for: empty body");
    if (!body) {
        return;
    }
    const detail::range_invoke_fn invoke = [](const void *context, usize begin, usize end) {
        (*static_cast<const std::function<void(usize, usize)> *>(context))(begin, end);
    };
    parallel_for_impl(count, grain, invoke, &body);
}

void job_system::parallel_for(usize count, const std::function<void(usize)> &body) {
    CONJURE_ASSERT(body != nullptr, "job_system::parallel_for: empty body");
    if (!body) {
        return;
    }
    const detail::range_invoke_fn invoke = [](const void *context, usize begin, usize end) {
        const std::function<void(usize)> &perIndex = *static_cast<const std::function<void(usize)> *>(context);
        for (usize index = begin; index < end; ++index) {
            perIndex(index);
        }
    };
    const usize workers = std::max<usize>(1, worker_count());
    const usize grain = std::max<usize>(1, count / (workers * 4));
    parallel_for_impl(count, grain, invoke, &body);
}

u32 job_system::thread_index() noexcept {
    return sThreadIndex;
}

job_system *job_system::current() noexcept {
    return sCurrentSystem;
}

job_system_stats job_system::stats() const noexcept {
    return mState->snapshot();
}

/*===========================================================================*/
/*===========================================================================*/
// job_system: private methods

void job_system::parallel_for_impl(usize count, usize grain, detail::range_invoke_fn invoke, const void *context) {
    mState->parallel_for(count, grain, invoke, context);
}
