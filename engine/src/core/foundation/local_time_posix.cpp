// POSIX backend of detail::local_time_of: localtime_r.

#include "core/foundation/local_time.h"

#include <time.h>

/*===========================================================================*/
/*===========================================================================*/
// Public functions

std::tm conjure::core::detail::local_time_of(std::time_t seconds) noexcept {
    std::tm local = {};
    localtime_r(&seconds, &local);
    return local;
}
