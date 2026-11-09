#include "conjure/modules/module_context.h"

#include "conjure/core/assert.h"

#include <utility>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    // Where registrar() points when called outside the entry: a registrar whose set is thrown
    // away, so a continuing assert handler cannot corrupt a module's real registrations.
    module_registration_set &scratch_set() noexcept {
        static module_registration_set sScratch = {};
        return sScratch;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

module_context::module_context(
  module_id id,
  module_host_kind host,
  semantic_version engineVersion,
  const module_manifest &manifest,
  std::filesystem::path rootDirectory,
  const service_table &services
) :
    mId(id),
    mHost(host),
    mEngineVersion(engineVersion),
    mManifest(&manifest),
    mRootDirectory(std::move(rootDirectory)),
    mServices(&services) {}

module_context::~module_context() {
    // Explicit so the order is visible: every retained callback dies before the owner's binary.
    mSubscriptions.clear();
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

module_id module_context::id() const noexcept {
    return mId;
}

module_host_kind module_context::host() const noexcept {
    return mHost;
}

semantic_version module_context::engine_version() const noexcept {
    return mEngineVersion;
}

const module_manifest &module_context::manifest() const noexcept {
    return *mManifest;
}

const std::filesystem::path &module_context::root_directory() const noexcept {
    return mRootDirectory;
}

module_registrar &module_context::registrar() noexcept {
    CONJURE_ASSERT(mRegistrar != nullptr, "module_context::registrar() called outside the module entry");
    if (mRegistrar != nullptr) {
        return *mRegistrar;
    }
    static module_registrar sScratchRegistrar(scratch_set());
    scratch_set() = {};
    return sScratchRegistrar;
}

bool module_context::is_entry_running() const noexcept {
    return mRegistrar != nullptr;
}

void module_context::retain(event_subscription &&subscription) {
    mSubscriptions.push_back(std::move(subscription));
}

usize module_context::retained_subscription_count() const noexcept {
    return mSubscriptions.size();
}

void module_context::begin_entry(module_registrar &registrar) noexcept {
    mRegistrar = &registrar;
}

void module_context::end_entry() noexcept {
    mRegistrar = nullptr;
}
