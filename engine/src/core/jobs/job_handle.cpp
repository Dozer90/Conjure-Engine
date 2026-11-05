#include "conjure/core/jobs/job_handle.h"

#include <utility>

using namespace conjure::core::jobs;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

job_handle::job_handle(std::shared_ptr<detail::job_counter> counter) noexcept :
    mCounter(std::move(counter)) {}
