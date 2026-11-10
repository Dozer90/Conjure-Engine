#include "physics/jolt_job_system.h"

#include "conjure/core/assert.h"

#include <utility>

namespace conjure::physics {

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

jolt_job_system::jolt_job_system() {}

jolt_job_system::~jolt_job_system() {
    drain_queue();
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void jolt_job_system::init(u32 maxJobs, u32 maxBarriers, core::jobs::job_system &jobs) {
    mJobs = &jobs;
    JobSystemWithBarrier::Init(maxBarriers);
    mPool.Init(maxJobs, maxJobs);
    mCapacity = maxJobs;
    mQueue.assign(maxJobs, nullptr);
    mHead = 0;
    mTail = 0;
}

u32 jolt_job_system::helper_count() const noexcept {
    return mJobs == nullptr ? 0 : mJobs->worker_count();
}

int jolt_job_system::GetMaxConcurrency() const {
    return static_cast<int>(helper_count()) + 1;
}

JPH::JobHandle jolt_job_system::CreateJob(
  const char *name,
  JPH::ColorArg color,
  const JobFunction &function,
  JPH::uint32 dependencyCount
) {
    const JPH::uint32 index = mPool.ConstructObject(name, color, this, function, dependencyCount);
    CONJURE_FATAL_ASSERT(index != job_pool::cInvalidObjectIndex, "jolt_job_system: the job pool is exhausted");
    Job *job = &mPool.Get(index);
    JobHandle handle(job);
    if (dependencyCount == 0) {
        QueueJob(job);
    }
    return handle;
}


/*===========================================================================*/
/*===========================================================================*/
// JPH::JobSystem queue

void jolt_job_system::QueueJob(Job *job) {
    if (!mParallelActive.load(std::memory_order_acquire)) {
        // No helpers: run the job now, exactly like JobSystemSingleThreaded.
        job->Execute();
        return;
    }
    job->AddRef();
    if (!push(job)) {
        // The queue holds maxJobs slots and the pool never hands out more jobs than that, so this is a
        // programming error; executing inline keeps the simulation correct.
        CONJURE_ASSERT(false, "jolt_job_system: queue full");
        job->Execute();
        job->Release();
        return;
    }
    mGeneration.fetch_add(1, std::memory_order_release);
    mGeneration.notify_one();
}

void jolt_job_system::QueueJobs(Job **jobs, JPH::uint count) {
    for (JPH::uint index = 0; index < count; ++index) {
        QueueJob(jobs[index]);
    }
}

void jolt_job_system::FreeJob(Job *job) {
    mPool.DestructObject(job);
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void jolt_job_system::begin_parallel() noexcept {
    mParallelActive.store(true, std::memory_order_release);
}

void jolt_job_system::end_parallel() noexcept {
    mParallelActive.store(false, std::memory_order_release);
    mGeneration.fetch_add(1, std::memory_order_release);
    mGeneration.notify_all();
}

void jolt_job_system::help_until_done() {
    while (true) {
        // Read the generation before looking at the queue: a push that lands after this read changes
        // it, so the wait below returns at once instead of sleeping past the new job.
        const u32 generation = mGeneration.load(std::memory_order_acquire);
        Job *job = pop();
        if (job != nullptr) {
            job->Execute();
            job->Release();
            mExecutedByHelpers.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        if (!mParallelActive.load(std::memory_order_acquire)) {
            return;
        }
        mGeneration.wait(generation, std::memory_order_acquire);
    }
}

void jolt_job_system::drain_queue() {
    // Jolt waited for every job on its barrier, so the queue is normally empty; a job that was queued
    // but already executed by the barrier thread still holds the queue's reference.
    while (true) {
        Job *job = pop();
        if (job == nullptr) {
            return;
        }
        job->Execute();
        job->Release();
    }
}

bool jolt_job_system::push(Job *job) noexcept {
    const std::scoped_lock lock(mQueueMutex);
    if (mTail - mHead >= mCapacity) {
        return false;
    }
    mQueue[mTail % mCapacity] = job;
    ++mTail;
    mQueuedCount.fetch_add(1, std::memory_order_relaxed);
    return true;
}

JPH::JobSystem::Job *jolt_job_system::pop() noexcept {
    const std::scoped_lock lock(mQueueMutex);
    if (mHead == mTail) {
        return nullptr;
    }
    Job *job = mQueue[mHead % mCapacity];
    mQueue[mHead % mCapacity] = nullptr;
    ++mHead;
    mQueuedCount.fetch_sub(1, std::memory_order_relaxed);
    return job;
}
}
