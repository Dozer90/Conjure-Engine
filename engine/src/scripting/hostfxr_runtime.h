#pragma once

#include "scripting/hostfxr_api.h"

#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_host.h"

#include "conjure/core/dynamic_library.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <memory>
#include <mutex>

// Private: the process-wide CLR (docs/design/03_scripting.md D1, 3.1 steps 1-9). CoreCLR cannot be
// unloaded or restarted, so the first dotnet_script_host that initialises starts it and every later one
// attaches; the delegates and the hostfxr library live until process exit.

namespace conjure::scripting {

    class hostfxr_runtime {
        public:
            using initialize_fn = int32_t (*)(conjure_native_api *nativeApi, conjure_managed_api *managedApi);

        private:
            std::mutex mMutex = {};
            std::unique_ptr<core::dynamic_library> mLibrary = nullptr;    // released, never unloaded (see dtor)
            std::filesystem::path mHostfxrPath = {};
            std::filesystem::path mManagedDirectory = {};
            host_mode mMode = host_mode::FRAMEWORK_DEPENDENT;
            initialize_fn mInitialize = nullptr;
            bool mStarted = false;              // steps 1-9 completed: later hosts attach
            bool mRuntimeStarted = false;       // CoreCLR runs (step 7 succeeded) even if a later step failed
            hostfxr_initialize_for_runtime_config_fn mInitializeForRuntimeConfig = nullptr;
            hostfxr_initialize_for_dotnet_command_line_fn mInitializeForCommandLine = nullptr;
            hostfxr_get_runtime_delegate_fn mGetRuntimeDelegate = nullptr;
            hostfxr_set_runtime_property_value_fn mSetRuntimeProperty = nullptr;
            hostfxr_set_error_writer_fn mSetErrorWriter = nullptr;
            hostfxr_close_fn mClose = nullptr;

        public:
            hostfxr_runtime();
            ~hostfxr_runtime();

            hostfxr_runtime(const hostfxr_runtime &) = delete;
            hostfxr_runtime &operator=(const hostfxr_runtime &) = delete;

        public:
            /// <summary>
            /// The one runtime of the process.
            /// </summary>
            [[nodiscard]] static hostfxr_runtime &instance() noexcept;

            /// <summary>
            /// Starts the CLR from config.managedDirectory when none runs yet (steps 1-9), or attaches to the
            /// running one (a different directory or mode logs a WARN). Main thread only.
            /// </summary>
            [[nodiscard]] core::result<void> ensure_started(const script_host_config &config);

            [[nodiscard]] bool is_started() const noexcept {
                return mStarted;
            }

            /// <summary>
            /// True once CoreCLR was started by this process, whether or not the handshake entry was found.
            /// </summary>
            [[nodiscard]] bool is_runtime_started() const noexcept {
                return mRuntimeStarted;
            }

            [[nodiscard]] host_mode mode() const noexcept {
                return mMode;
            }

            [[nodiscard]] const std::filesystem::path &managed_directory() const noexcept {
                return mManagedDirectory;
            }

            [[nodiscard]] const std::filesystem::path &hostfxr_path() const noexcept {
                return mHostfxrPath;
            }

            /// <summary>
            /// Conjure.Interop.NativeBridge.Initialize; nullptr before the runtime started.
            /// </summary>
            [[nodiscard]] initialize_fn initialize_entry() const noexcept {
                return mInitialize;
            }

            /// <summary>
            /// get_hostfxr_path without starting anything: the path of the hostfxr nethost would load for an
            /// application in 'managedDirectory' (empty = the global installation), or RUNTIME_NOT_FOUND.
            /// </summary>
            [[nodiscard]] static core::result<std::filesystem::path> locate_hostfxr(
              const std::filesystem::path &managedDirectory,
              const std::filesystem::path &dotnetRootOverride
            );

        private:
            [[nodiscard]] core::result<void> start(const script_host_config &config, host_mode mode);
            [[nodiscard]] core::result<void> load_library(const std::filesystem::path &path);
            static void HOSTFXR_CALLTYPE forward_error_to_log(const char_t *message);
    };
}
