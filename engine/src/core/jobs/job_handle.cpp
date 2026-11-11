#include "conjure/core/jobs/job_handle.h"

#include "conjure/core/assert.h"

#include <utility>

using namespace conjure::core::jobs;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

job_handle::job_handle(std::shared_ptr<detail::job_counter> counter) noexcept :
    mCounter(std::move(counter)) {}

job_fence::~job_fence() {
    CONJURE_ASSERT(is_complete(), "job_fence destroyed with {} job(s) still pending", pending());
}
