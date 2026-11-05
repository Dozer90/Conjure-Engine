#include "jobs_test_support.h"

using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// scoped_job_system

scoped_job_system::scoped_job_system(u32 workerCount, usize maxQueuedJobs) {
    conjure::core::jobs::job_system_desc desc = {};
    desc.workerCount = workerCount;
    desc.maxQueuedJobs = maxQueuedJobs;
    REQUIRE(mSystem.start(desc).has_value());
}

scoped_job_system::~scoped_job_system() {
    mSystem.stop();
}
