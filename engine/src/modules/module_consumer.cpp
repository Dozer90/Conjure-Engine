#include "conjure/modules/module_consumer.h"

#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

/*===========================================================================*/
/*===========================================================================*/
// reload_context

reload_context::reload_context() {}

reload_context::~reload_context() {}

void reload_context::store(string_id key, std::span<const std::byte> bytes) {
    mBlobs.insert_or_assign(key, std::vector<std::byte>(bytes.begin(), bytes.end()));
}

std::span<const std::byte> reload_context::fetch(string_id key) const noexcept {
    const auto found = mBlobs.find(key);
    if (found == mBlobs.end()) {
        return {};
    }
    return std::span<const std::byte>(found->second.data(), found->second.size());
}

bool reload_context::contains(string_id key) const noexcept {
    return mBlobs.contains(key);
}

usize reload_context::size() const noexcept {
    return mBlobs.size();
}


/*===========================================================================*/
/*===========================================================================*/
// module_consumer

module_consumer::module_consumer() {}

module_consumer::~module_consumer() {}

bool module_consumer::participates_in_managed_reload() const noexcept {
    return false;
}
