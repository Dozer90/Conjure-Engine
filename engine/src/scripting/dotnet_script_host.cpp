#include "conjure/scripting/dotnet_script_host.h"

#include "scripting/hostfxr_runtime.h"

#include "conjure/scripting/log_category.h"
#include "conjure/scripting/scripting_error.h"

#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"

using namespace conjure;
using namespace conjure::scripting;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

dotnet_script_host::dotnet_script_host() {}

dotnet_script_host::~dotnet_script_host() {
    shutdown();
}


/*===========================================================================*/
/*===========================================================================*/
// script_host

core::result<void> dotnet_script_host::initialise(const script_host_config &config, conjure_native_api &nativeApi) {
    mStatus = script_host_status::DISABLED;
    if (!core::jobs::is_main_thread()) {
        return core::make_error(script_error::WRONG_THREAD, "dotnet_script_host::initialise off the main thread");
    }

    hostfxr_runtime &runtime = hostfxr_runtime::instance();
    CONJURE_TRY(runtime.ensure_started(config));

    // 10. The handshake: managed fills its header first, so a failure still reports both hashes.
    mManagedApi = {};
    const int32_t handshake = runtime.initialise_entry()(&nativeApi, &mManagedApi);
    if (handshake != 0) {
        return core::make_error(
          script_error::ABI_MISMATCH,
          "managed Initialize rejected the native table (code {}): native hash {:#018x}, size {}, version {}; "
          "managed hash {:#018x}",
          handshake,
          nativeApi.layout_hash,
          nativeApi.size,
          nativeApi.version,
          mManagedApi.layout_hash
        );
    }
    CONJURE_TRY(validate_managed_api(mManagedApi));

    mNativeApi = &nativeApi;
    mMode = runtime.mode();
    mManagedDirectory = runtime.managed_directory();
    mStatus = script_host_status::READY;
    CONJURE_LOG_DEBUG(LOG_SCRIPTING, "dotnet_script_host ready ({})", to_string(mMode));
    return core::result<void>::success();
}

void dotnet_script_host::shutdown() {
    if (mStatus == script_host_status::READY && mManagedApi.shutdown != nullptr) {
        const int32_t result = mManagedApi.shutdown();
        if (result != 0) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "managed shutdown returned {}", result);
        }
    }
    mManagedApi = {};
    mNativeApi = nullptr;
    mStatus = script_host_status::UNINITIALISED;
}

const conjure_managed_api &dotnet_script_host::managed_api() const noexcept {
    return mManagedApi;
}

script_host_status dotnet_script_host::status() const noexcept {
    return mStatus;
}

script_backend dotnet_script_host::backend() const noexcept {
    return script_backend::DOTNET;
}

host_mode dotnet_script_host::mode() const noexcept {
    return mMode;
}

const std::filesystem::path &dotnet_script_host::managed_directory() const noexcept {
    return mManagedDirectory;
}

bool dotnet_script_host::is_runtime_started() noexcept {
    return hostfxr_runtime::instance().is_started();
}
