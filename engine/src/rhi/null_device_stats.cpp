#include "conjure/rhi/null_device_stats.h"

using namespace conjure;
using namespace conjure::rhi;

void null_device_stats::reset() noexcept {
    // Keep the vector's capacity so a reset inside a frame loop does not allocate on the next marker.
    std::vector<std::string> names = std::move(markerNames);
    names.clear();
    *this = null_device_stats();
    markerNames = std::move(names);
}
