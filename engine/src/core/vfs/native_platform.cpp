#include "core/vfs/native_platform.h"

#include <utility>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

internal::read_handle::read_handle(std::intptr_t value) noexcept :
    mValue(value) {}

internal::read_handle::~read_handle() {
    close();
}

internal::read_handle::read_handle(read_handle &&other) noexcept :
    mValue(std::exchange(other.mValue, INVALID_VALUE)) {}

internal::read_handle &internal::read_handle::operator=(read_handle &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    close();
    mValue = std::exchange(other.mValue, INVALID_VALUE);
    return *this;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void internal::read_handle::close() noexcept {
    if (!is_open()) {
        return;
    }
    close_read_handle(mValue);
    mValue = INVALID_VALUE;
}
