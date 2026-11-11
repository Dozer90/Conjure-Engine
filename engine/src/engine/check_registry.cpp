#include "conjure/engine/diagnostics/check_registry.h"

using namespace conjure;
using namespace conjure::engine::diagnostics;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

check_registry::check_registry() {}

check_registry::~check_registry() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void check_registry::report(std::string_view name, bool passed, std::string_view detail, u64 frame) {
    check_record record = {};
    record.name = std::string(name);
    record.passed = passed;
    record.detail = std::string(detail);
    record.frame = frame;

    const std::lock_guard<std::mutex> lock(mMutex);
    mRecords.push_back(std::move(record));
    if (!passed) {
        ++mFailedCount;
    }
}

std::vector<check_record> check_registry::records() const {
    const std::lock_guard<std::mutex> lock(mMutex);
    return mRecords;
}

bool check_registry::all_passed() const noexcept {
    const std::lock_guard<std::mutex> lock(mMutex);
    return mFailedCount == 0;
}

u64 check_registry::failed_count() const noexcept {
    const std::lock_guard<std::mutex> lock(mMutex);
    return mFailedCount;
}

usize check_registry::record_count() const noexcept {
    const std::lock_guard<std::mutex> lock(mMutex);
    return mRecords.size();
}

void check_registry::clear() {
    const std::lock_guard<std::mutex> lock(mMutex);
    mRecords.clear();
    mFailedCount = 0;
}
