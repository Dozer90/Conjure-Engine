#include "conjure/testing/memory_guard.h"

#include "conjure/testing/doctest.h"

using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

memory_guard::memory_guard() :
    mBaseline(conjure::core::memory::snapshot()) {}

memory_guard::~memory_guard() {
    if (!mChecked) {
        check();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void memory_guard::check() {
    mChecked = true;
    const i64 delta = live_delta();
    CHECK_MESSAGE(
      delta == 0,
      "memory_guard: " << delta << " allocation(s) still live relative to the baseline (positive = leak)"
    );
}

void memory_guard::reset_baseline() noexcept {
    mBaseline = conjure::core::memory::snapshot();
    mChecked = false;
}

conjure::i64 memory_guard::live_delta() const noexcept {
    const conjure::core::memory::memory_snapshot now = conjure::core::memory::snapshot();
    return static_cast<i64>(now.live_count()) - static_cast<i64>(mBaseline.live_count());
}
