// Windows backend of detail::local_time_of: the C runtime's localtime_s.

#include "core/foundation/local_time.h"

/*===========================================================================*/
/*===========================================================================*/
// Public functions

std::tm conjure::core::detail::local_time_of(std::time_t seconds) noexcept {
    std::tm local = {};
    localtime_s(&local, &seconds);
    return local;
}
