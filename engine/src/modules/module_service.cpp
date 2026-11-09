#include "conjure/modules/module_service.h"

#include "conjure/core/assert.h"

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

service_table::service_table() {}

service_table::~service_table() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

bool service_table::contains(service_id id) const noexcept {
    return mServices.contains(id);
}

usize service_table::size() const noexcept {
    return mServices.size();
}

void service_table::clear() noexcept {
    mServices.clear();
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void service_table::provide_raw(service_id id, void *service, std::string_view name) {
    CONJURE_ASSERT(service != nullptr, "service_table::provide: null service '{}'", name);
    if (service == nullptr) {
        return;
    }
    const bool inserted = mServices.try_emplace(id, service).second;
    CONJURE_ASSERT(inserted, "service_table::provide: service '{}' was already provided", name);
}

void *service_table::find_raw(service_id id) const noexcept {
    const auto found = mServices.find(id);
    if (found == mServices.end()) {
        return nullptr;
    }
    return found->second;
}
