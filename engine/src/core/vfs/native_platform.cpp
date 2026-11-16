#include "core/vfs/native_platform.h"

#include "conjure/core/log.h"

#include <atomic>
#include <utility>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    // Process-wide and written from any thread. Nothing is published through them, so relaxed order
    // is enough for the counter; the flag only has to let exactly one caller through.
    std::atomic<u64> sFlushedFileCount = 0;
    std::atomic<bool> sUnflushableVolumeReported = false;
}


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


/*===========================================================================*/
/*===========================================================================*/
// Flush statistics

u64 internal::flushed_file_count() noexcept {
    return sFlushedFileCount.load(std::memory_order_relaxed);
}

void internal::note_flushed_file() noexcept {
    sFlushedFileCount.fetch_add(1, std::memory_order_relaxed);
}

void internal::note_unflushable_volume(std::string_view displayPath) {
    if (sUnflushableVolumeReported.exchange(true, std::memory_order_relaxed)) {
        return;
    }
    CONJURE_LOG_WARN(
      LOG_CORE,
      "vfs: the volume holding '{}' cannot flush files to its device; atomic writes there are not protected "
      "against a power cut (reported once)",
      displayPath
    );
}

bool internal::unflushable_volume_reported() noexcept {
    return sUnflushableVolumeReported.load(std::memory_order_relaxed);
}
