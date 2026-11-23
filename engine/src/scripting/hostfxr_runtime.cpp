#include "scripting/hostfxr_runtime.h"

#include "scripting/scripting_internal.h"

#include "conjure/scripting/log_category.h"
#include "conjure/scripting/scripting_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/build_info.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::scripting::hostfxr;

namespace {

    constexpr std::string_view DOWNLOAD_URL = "https://dotnet.microsoft.com/download/dotnet/10.0";
    constexpr std::string_view BOOTSTRAP_DLL = "Conjure.Bootstrap.dll";
    constexpr std::string_view BOOTSTRAP_RUNTIME_CONFIG = "Conjure.Bootstrap.runtimeconfig.json";
    constexpr std::string_view MANAGED_DLL = "Conjure.Managed.dll";
    constexpr std::string_view BRIDGE_TYPE = "Conjure.Interop.NativeBridge, Conjure.Managed";
    constexpr std::string_view BRIDGE_METHOD = "Initialize";

    [[nodiscard]] std::string describe_status(int32_t code) {
        return std::format("{:#010x}", static_cast<u32>(code));
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

hostfxr_runtime::hostfxr_runtime() {}

hostfxr_runtime::~hostfxr_runtime() {
    // The CLR never unloads and its threads may still run during static destruction, so the hostfxr
    // library is deliberately left mapped: releasing the owner leaks the handle until process exit.
    static_cast<void>(mLibrary.release());
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

hostfxr_runtime &hostfxr_runtime::instance() noexcept {
    static hostfxr_runtime sInstance = {};
    return sInstance;
}

core::result<void> hostfxr_runtime::ensure_started(const script_host_config &config) {
    if (!core::jobs::is_main_thread()) {
        return core::make_error(script_error::WRONG_THREAD, "the .NET runtime is hosted from the main thread only");
    }
    const std::scoped_lock lock(mMutex);

    CONJURE_TRY(validate_managed_directory(config.managedDirectory));
    CONJURE_TRY_ASSIGN(const host_mode mode, detect_host_mode(config.managedDirectory));

    if (mStarted) {
        std::error_code probe = {};
        const bool sameDirectory = std::filesystem::equivalent(mManagedDirectory, config.managedDirectory, probe);
        if (!sameDirectory || mode != mMode) {
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "the .NET runtime already runs from '{}' ({}); attaching the host for '{}' ({}) to it (D1)",
              detail::path_to_utf8(mManagedDirectory),
              to_string(mMode),
              detail::path_to_utf8(config.managedDirectory),
              to_string(mode)
            );
        }
        return core::result<void>::success();
    }
    return start(config, mode);
}

core::result<std::filesystem::path> hostfxr_runtime::locate_hostfxr(
  const std::filesystem::path &managedDirectory,
  const std::filesystem::path &dotnetRootOverride
) {
    const host_string assemblyPath = managedDirectory.empty()
      ? host_string()
      : to_host_string(managedDirectory / std::filesystem::path(BOOTSTRAP_DLL));
    const host_string dotnetRoot = dotnetRootOverride.empty() ? host_string() : to_host_string(dotnetRootOverride);

    get_hostfxr_parameters parameters = {};
    parameters.size = sizeof(parameters);
    parameters.assembly_path = assemblyPath.empty() ? nullptr : assemblyPath.c_str();
    parameters.dotnet_root = dotnetRoot.empty() ? nullptr : dotnetRoot.c_str();
    const get_hostfxr_parameters *parametersOrNull =
      (parameters.assembly_path == nullptr && parameters.dotnet_root == nullptr) ? nullptr : &parameters;

    size_t size = 0;
    const int32_t probe = get_hostfxr_path(nullptr, &size, parametersOrNull);
    if (probe != static_cast<int32_t>(host_status::HOST_API_BUFFER_TOO_SMALL) || size == 0) {
        return core::make_error(
          script_error::RUNTIME_NOT_FOUND,
          "no .NET runtime found (get_hostfxr_path {}); install the .NET 10 runtime from {}",
          describe_status(probe),
          DOWNLOAD_URL
        );
    }

    std::vector<char_t> buffer(size);
    const int32_t located = get_hostfxr_path(buffer.data(), &size, parametersOrNull);
    if (located != 0) {
        return core::make_error(
          script_error::RUNTIME_NOT_FOUND,
          "get_hostfxr_path failed with {}; install the .NET 10 runtime from {}",
          describe_status(located),
          DOWNLOAD_URL
        );
    }
    return std::filesystem::path(host_string(buffer.data()));
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

core::result<void> hostfxr_runtime::start(const script_host_config &config, host_mode mode) {
    // 1. Locate hostfxr (app-local for a self-contained layout, global otherwise).
    CONJURE_TRY_ASSIGN(
      const std::filesystem::path hostfxrPath,
      locate_hostfxr(config.managedDirectory, config.dotnetRootOverride)
    );

    // 2. Load it and resolve the exports the two hosting paths need.
    CONJURE_TRY(load_library(hostfxrPath));

    // 3. Errors hostfxr reports on this (main) thread go to the log.
    mSetErrorWriter(&forward_error_to_log);

    // 5. Initialise a host context for the detected mode.
    const host_string bootstrapDll = to_host_string(config.managedDirectory / std::filesystem::path(BOOTSTRAP_DLL));
    const host_string runtimeConfig =
      to_host_string(config.managedDirectory / std::filesystem::path(BOOTSTRAP_RUNTIME_CONFIG));
    const host_string managedDirectory = to_host_string(config.managedDirectory);
    const host_string dotnetRoot =
      config.dotnetRootOverride.empty() ? host_string() : to_host_string(config.dotnetRootOverride);

    hostfxr_initialize_parameters parameters = {};
    parameters.size = sizeof(parameters);
    parameters.host_path = nullptr;
    parameters.dotnet_root = nullptr;
    if (mode == host_mode::SELF_CONTAINED) {
        parameters.dotnet_root = managedDirectory.c_str();
    } else if (!dotnetRoot.empty()) {
        parameters.dotnet_root = dotnetRoot.c_str();
    }

    hostfxr_handle handle = nullptr;
    int32_t initialised = 0;
    if (mode == host_mode::FRAMEWORK_DEPENDENT) {
        initialised = mInitialiseForRuntimeConfig(runtimeConfig.c_str(), &parameters, &handle);
    } else {
        std::array<const char_t *, 1> argv = {bootstrapDll.c_str()};
        initialised = mInitialiseForCommandLine(1, argv.data(), &parameters, &handle);
    }
    if (initialised == static_cast<int32_t>(host_status::HOST_INVALID_STATE)) {
        return core::make_error(
          script_error::HOST_ALREADY_RUNNING,
          "a .NET runtime of another configuration already runs in this process ({})",
          describe_status(initialised)
        );
    }
    if (initialised == static_cast<int32_t>(host_status::INVALID_CONFIG_FILE)) {
        return core::make_error(
          script_error::RUNTIME_CONFIG_INVALID,
          "hostfxr rejected '{}' ({})",
          detail::path_to_utf8(config.managedDirectory / std::filesystem::path(BOOTSTRAP_RUNTIME_CONFIG)),
          describe_status(initialised)
        );
    }
    if (!is_success(initialised) || handle == nullptr) {
        return core::make_error(
          script_error::RUNTIME_NOT_FOUND,
          "hostfxr could not initialise the runtime ({}); install the .NET 10 runtime from {}",
          describe_status(initialised),
          DOWNLOAD_URL
        );
    }
    const bool firstContext = initialised == static_cast<int32_t>(host_status::SUCCESS);

    // 6. Properties the managed side reads through AppContext (first context only, before the CLR starts).
    if (firstContext) {
        const host_string baseDirectoryName = host_text("APP_CONTEXT_BASE_DIRECTORY");
        const host_string versionName = host_text("CONJURE_ENGINE_VERSION");
        const host_string versionValue = host_text(core::build_info::VERSION_STRING);
        const host_string debuggerName = host_text("CONJURE_WAIT_FOR_DEBUGGER");
        const host_string debuggerValue = host_text(config.waitForDebugger ? "1" : "0");
        const int32_t baseSet = mSetRuntimeProperty(handle, baseDirectoryName.c_str(), managedDirectory.c_str());
        const int32_t versionSet = mSetRuntimeProperty(handle, versionName.c_str(), versionValue.c_str());
        const int32_t debuggerSet = mSetRuntimeProperty(handle, debuggerName.c_str(), debuggerValue.c_str());
        if (!is_success(baseSet) || !is_success(versionSet) || !is_success(debuggerSet)) {
            CONJURE_LOG_DEBUG(
              LOG_SCRIPTING,
              "hostfxr refused a runtime property ({} {} {}); continuing with the defaults",
              describe_status(baseSet),
              describe_status(versionSet),
              describe_status(debuggerSet)
            );
        }
    }

    // 7. Delegates; this starts CoreCLR.
    void *getFunctionPointerRaw = nullptr;
    const int32_t gotGetFn = mGetRuntimeDelegate(handle, hdt_get_function_pointer, &getFunctionPointerRaw);
    if (!is_success(gotGetFn) || getFunctionPointerRaw == nullptr) {
        mClose(handle);
        return core::make_error(
          script_error::HOSTFXR_INCOMPATIBLE,
          "hostfxr has no hdt_get_function_pointer delegate ({}); .NET 10 or later is required",
          describe_status(gotGetFn)
        );
    }
    get_function_pointer_fn getFunctionPointer = nullptr;
    std::memcpy(&getFunctionPointer, &getFunctionPointerRaw, sizeof(getFunctionPointer));
    // From here on CoreCLR runs in the process: a failure below must not re-initialise it (the retry
    // attaches through Success_HostAlreadyInitialized) nor replace the hostfxr library it was started from.
    mRuntimeStarted = true;

    if (mode == host_mode::FRAMEWORK_DEPENDENT) {
        void *loadAssemblyRaw = nullptr;
        const int32_t gotLoad = mGetRuntimeDelegate(handle, hdt_load_assembly, &loadAssemblyRaw);
        if (!is_success(gotLoad) || loadAssemblyRaw == nullptr) {
            mClose(handle);
            return core::make_error(
              script_error::HOSTFXR_INCOMPATIBLE,
              "hostfxr has no hdt_load_assembly delegate ({}); .NET 10 or later is required",
              describe_status(gotLoad)
            );
        }
        load_assembly_fn loadAssembly = nullptr;
        std::memcpy(&loadAssembly, &loadAssemblyRaw, sizeof(loadAssembly));
        const host_string managedDll = to_host_string(config.managedDirectory / std::filesystem::path(MANAGED_DLL));
        const int32_t loaded = loadAssembly(managedDll.c_str(), nullptr, nullptr);
        if (!is_success(loaded)) {
            mClose(handle);
            return core::make_error(
              script_error::MANAGED_SDK_MISSING,
              "Conjure.Managed.dll did not load into the default load context ({})",
              describe_status(loaded)
            );
        }
    }

    // 8. The handshake entry point.
    const host_string typeName = host_text(BRIDGE_TYPE);
    const host_string methodName = host_text(BRIDGE_METHOD);
    void *initialiseRaw = nullptr;
    const int32_t gotInitialise = getFunctionPointer(
      typeName.c_str(),
      methodName.c_str(),
      UNMANAGEDCALLERSONLY_METHOD,
      nullptr,
      nullptr,
      &initialiseRaw
    );
    if (!is_success(gotInitialise) || initialiseRaw == nullptr) {
        mClose(handle);
        return core::make_error(
          script_error::MANAGED_SDK_MISSING,
          "Conjure.Interop.NativeBridge.Initialize was not found in Conjure.Managed ({})",
          describe_status(gotInitialise)
        );
    }
    std::memcpy(&mInitialise, &initialiseRaw, sizeof(mInitialise));

    // 9. The context is not needed again: delegates have process lifetime.
    mClose(handle);

    mHostfxrPath = hostfxrPath;
    mManagedDirectory = config.managedDirectory;
    mMode = mode;
    mStarted = true;
    CONJURE_LOG_INFO(
      LOG_SCRIPTING,
      ".NET runtime started ({}) from '{}' with hostfxr '{}'",
      to_string(mode),
      detail::path_to_utf8(config.managedDirectory),
      detail::path_to_utf8(hostfxrPath)
    );
    return core::result<void>::success();
}

core::result<void> hostfxr_runtime::load_library(const std::filesystem::path &path) {
    if (mLibrary != nullptr) {
        // A retry after a failed start keeps the first mapping: unloading hostfxr under a running CoreCLR
        // (mRuntimeStarted) would pull the floor from under it, and the exports are already resolved.
        if (path != mHostfxrPath) {
            CONJURE_LOG_DEBUG(
              LOG_SCRIPTING,
              "hostfxr '{}' is already loaded; '{}' is not loaded again",
              detail::path_to_utf8(mHostfxrPath),
              detail::path_to_utf8(path)
            );
        }
        return core::result<void>::success();
    }
    core::result<core::dynamic_library> loaded = core::dynamic_library::load(path);
    if (!loaded.has_value()) {
        return core::make_error(
          script_error::RUNTIME_NOT_FOUND,
          "hostfxr at '{}' did not load: {}",
          detail::path_to_utf8(path),
          loaded.get_error().message()
        );
    }
    mLibrary = std::make_unique<core::dynamic_library>(std::move(loaded).value());
    mHostfxrPath = path;

    mInitialiseForRuntimeConfig =
      mLibrary->symbol_as<hostfxr_initialize_for_runtime_config_fn>("hostfxr_initialize_for_runtime_config");
    mInitialiseForCommandLine =
      mLibrary->symbol_as<hostfxr_initialize_for_dotnet_command_line_fn>("hostfxr_initialize_for_dotnet_command_line");
    mGetRuntimeDelegate = mLibrary->symbol_as<hostfxr_get_runtime_delegate_fn>("hostfxr_get_runtime_delegate");
    mSetRuntimeProperty =
      mLibrary->symbol_as<hostfxr_set_runtime_property_value_fn>("hostfxr_set_runtime_property_value");
    mSetErrorWriter = mLibrary->symbol_as<hostfxr_set_error_writer_fn>("hostfxr_set_error_writer");
    mClose = mLibrary->symbol_as<hostfxr_close_fn>("hostfxr_close");

    const bool complete = mInitialiseForRuntimeConfig != nullptr && mInitialiseForCommandLine != nullptr &&
      mGetRuntimeDelegate != nullptr && mSetRuntimeProperty != nullptr && mSetErrorWriter != nullptr &&
      mClose != nullptr;
    if (!complete) {
        mLibrary.reset();
        mHostfxrPath.clear();
        return core::make_error(
          script_error::HOSTFXR_INCOMPATIBLE,
          "hostfxr at '{}' lacks a required export; .NET 10 or later is required",
          detail::path_to_utf8(path)
        );
    }
    return core::result<void>::success();
}

void HOSTFXR_CALLTYPE hostfxr_runtime::forward_error_to_log(const char_t *message) {
    CONJURE_LOG_ERROR(LOG_SCRIPTING, "hostfxr: {}", narrow(message));
}
