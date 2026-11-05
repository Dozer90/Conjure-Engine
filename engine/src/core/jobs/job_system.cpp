#include "conjure/core/jobs/job_system.h"

#include "core/jobs/thread_name.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <array>
#include <atomic>
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

    // How many times an idle worker re-checks the queue (yielding in between) before it sleeps on
    // the condition variable. Bounded so idle workers never burn a core.
    constexpr u32 IDLE_SPIN_ITERATIONS = 64;

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
        std::string mThreadNamePrefix = {};        // written by start(), read by the workers it creates
        std::atomic<u32> mWorkerCount = 0;         // a hint for schedulers; relaxed reads are enough
        std::atomic<bool> mRunning = false;
        std::mutex mLifecycleMutex = {};           // serialises start() and stop()

        // Queue and block pool, guarded by mQueueMutex.
        std::mutex mQueueMutex = {};
        std::condition_variable mWorkerCondition = {};
        std::array<lane, JOB_PRIORITY_COUNT> mLanes = {};
        usize mMaxQueuedJobs = 0;
        bool mAcceptingJobs = false;
        bool mStopRequested = false;
        std::vector<std::unique_ptr<job_block[]>> mChunks = {};
        std::vector<job_block *> mFreeBlocks = {};

        // Lock-free mirrors read outside the lock.
        std::atomic<usize> mQueuedCount = 0;
        std::atomic<usize> mActiveJobs = 0;        // queued + executing jobs that went through the queue
        std::atomic<usize> mIdleWaiterDepth = 0;   // active jobs owned by threads inside wait_idle()

        std::vector<std::thread> mWorkers = {};

        // Statistics.
        std::atomic<u64> mScheduled = 0;
        std::atomic<u64> mExecuted = 0;
        std::atomic<u64> mStolen = 0;
        std::atomic<u64> mHelped = 0;
        std::atomic<u64> mInlineExecutions = 0;
        std::atomic<u64> mQueueFullFallbacks = 0;
        std::atomic<usize> mMaxQueueDepth = 0;
        std::atomic<usize> mPoolCapacity = 0;
        std::atomic<u32> mPoolGrowths = 0;

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
        void wait(detail::job_counter &counter);
        void wait_idle();
        void parallel_for(usize count, usize grain, detail::range_invoke_fn invoke, const void *context);

        [[nodiscard]] job_system_stats snapshot() const noexcept;

    private:
        [[nodiscard]] bool inline_only() const noexcept;

        // Block pool (mQueueMutex held).
        void grow_pool_locked();
        [[nodiscard]] job_block *acquire_block_locked();
        void release_block_locked(job_block *block);

        // Queue (mQueueMutex held unless stated otherwise).
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

    mThreadNamePrefix = desc.threadNamePrefix;
    mWorkerCount.store(desc.workerCount, std::memory_order_relaxed);

    {
        const std::scoped_lock lock(mQueueMutex);
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

        mScheduled.store(0, std::memory_order_relaxed);
        mExecuted.store(0, std::memory_order_relaxed);
        mStolen.store(0, std::memory_order_relaxed);
        mHelped.store(0, std::memory_order_relaxed);
        mInlineExecutions.store(0, std::memory_order_relaxed);
        mQueueFullFallbacks.store(0, std::memory_order_relaxed);
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

    CONJURE_LOG_DEBUG(LOG_CORE, "job_system started with {} worker(s)", desc.workerCount);
    return {};
}

void job_system::state::stop() {
    const std::scoped_lock lifecycle(mLifecycleMutex);

    if (!mRunning.load(std::memory_order_acquire)) {
        return;
    }
    CONJURE_ASSERT(sCurrentSystem != mOwner, "job_system::stop called from one of its own workers");
    if (sCurrentSystem == mOwner) {
        return;
    }

    {
        const std::scoped_lock lock(mQueueMutex);
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
        const std::scoped_lock lock(mQueueMutex);
        mStopRequested = false;
    }
    mRunning.store(false, std::memory_order_release);

    CONJURE_LOG_DEBUG(LOG_CORE, "job_system stopped after executing {} job(s)", mExecuted.load());
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
    mScheduled.fetch_add(1, std::memory_order_relaxed);

    if (!inline_only()) {
        bool queued = false;
        {
            const std::scoped_lock lock(mQueueMutex);
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
    u64 validCount = 0;
    for (const std::function<void()> &job : jobs) {
        CONJURE_ASSERT(job != nullptr, "job_system::schedule: empty job in batch");
        if (job) {
            ++validCount;
        }
    }
    if (validCount == 0) {
        return {};
    }

    std::shared_ptr<detail::job_counter> counter = std::make_shared<detail::job_counter>();
    counter->remaining.store(validCount, std::memory_order_relaxed);
    job_handle handle(counter);
    mScheduled.fetch_add(validCount, std::memory_order_relaxed);

    // Queue as many as fit in one critical section; whatever is left runs on the caller.
    usize firstInline = 0;
    if (!inline_only()) {
        usize pushed = 0;
        const std::thread::id self = std::this_thread::get_id();
        const bool fromOwnWorker = sCurrentSystem == mOwner;
        {
            const std::scoped_lock lock(mQueueMutex);
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
                block->counter = counter.get();
                block->sharedCounter = counter;
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
            run_inline(jobs[index], counter.get());
        }
    }
    return handle;
}

/*===========================================================================*/
/*===========================================================================*/
// state: waiting

void job_system::state::wait(detail::job_counter &counter) {
    job_block job = {};
    while (counter.remaining.load(std::memory_order_seq_cst) != 0) {
        if (try_pop(job)) {
            execute(job, true);
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
    while (true) {
        if (try_pop(job)) {
            execute(job, true);
            continue;
        }
        if (is_idle_for(ownJobs)) {
            break;
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
        mScheduled.fetch_add(1, std::memory_order_relaxed);
        run_range_inline(invoke, context, 0, count);
        return;
    }

    const usize rangeCount = 1 + (count - 1) / effectiveGrain;
    mScheduled.fetch_add(rangeCount, std::memory_order_relaxed);

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
        const std::scoped_lock lock(mQueueMutex);
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

    invoke(context, 0, std::min(effectiveGrain, count));
    mExecuted.fetch_add(1, std::memory_order_relaxed);

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
    stats.scheduled = mScheduled.load(std::memory_order_relaxed);
    stats.executed = mExecuted.load(std::memory_order_relaxed);
    stats.stolen = mStolen.load(std::memory_order_relaxed);
    stats.helped = mHelped.load(std::memory_order_relaxed);
    stats.inlineExecutions = mInlineExecutions.load(std::memory_order_relaxed);
    stats.queueFullFallbacks = mQueueFullFallbacks.load(std::memory_order_relaxed);
    stats.maxQueueDepth = mMaxQueueDepth.load(std::memory_order_relaxed);
    stats.queuedNow = mQueuedCount.load(std::memory_order_relaxed);
    stats.poolGrowths = mPoolGrowths.load(std::memory_order_relaxed);
    stats.poolCapacity = mPoolCapacity.load(std::memory_order_relaxed);
    return stats;
}

/*===========================================================================*/
/*===========================================================================*/
// state: block pool (mQueueMutex held)

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
// state: queue (mQueueMutex held unless stated otherwise)

// Returns a block to fill, or nullptr when the caller has to run the job itself: the system is
// stopping (workers keep queueing so the drain stays local to them) or the queue is full.
job_system::state::job_block *job_system::state::begin_enqueue_locked(bool fromOwnWorker) {
    if (!mAcceptingJobs && !fromOwnWorker) {
        return nullptr;
    }
    if (mQueuedCount.load(std::memory_order_relaxed) >= mMaxQueuedJobs) {
        mQueueFullFallbacks.fetch_add(1, std::memory_order_relaxed);
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
    const std::scoped_lock lock(mQueueMutex);
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
// when either reaches zero.
void job_system::state::finish_job(detail::job_counter *counter, bool wasQueued) {
    bool wake = false;
    if (counter != nullptr) {
        wake = counter->remaining.fetch_sub(1, std::memory_order_seq_cst) == 1;
    }
    if (wasQueued) {
        wake = (mActiveJobs.fetch_sub(1, std::memory_order_seq_cst) == 1) || wake;
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

    mExecuted.fetch_add(1, std::memory_order_relaxed);
    if (helping) {
        mHelped.fetch_add(1, std::memory_order_relaxed);
    } else if (job.scheduler != std::this_thread::get_id()) {
        mStolen.fetch_add(1, std::memory_order_relaxed);
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
    mExecuted.fetch_add(1, std::memory_order_relaxed);
    mInlineExecutions.fetch_add(1, std::memory_order_relaxed);
    finish_job(counter, false);
}

void job_system::state::run_range_inline(detail::range_invoke_fn invoke, const void *context, usize begin, usize end) {
    invoke(context, begin, end);
    mExecuted.fetch_add(1, std::memory_order_relaxed);
    mInlineExecutions.fetch_add(1, std::memory_order_relaxed);
}

/*===========================================================================*/
/*===========================================================================*/
// state: workers

// Returns false when the system is stopping and nothing is left to run.
bool job_system::state::next_job(job_block &out) {
    for (u32 spin = 0; spin < IDLE_SPIN_ITERATIONS; ++spin) {
        if (try_pop(out)) {
            return true;
        }
        std::this_thread::yield();
    }

    std::unique_lock lock(mQueueMutex);
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
    detail::set_current_thread_name(mThreadNamePrefix + "-" + std::to_string(index));

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

void job_system::wait(const job_handle &handle) {
    if (!handle.valid() || handle.is_complete()) {
        return;
    }
    mState->wait(*handle.mCounter);
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
