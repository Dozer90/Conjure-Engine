#pragma once

#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_host.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>

// The real host (docs/design/03_scripting.md 3.1): one CLR per process, started on first use by the
// private hostfxr_runtime singleton; every dotnet_script_host::initialize re-runs the handshake
// (managed Initialize is idempotent), so hosts may be created and destroyed freely.

namespace conjure::scripting {

    class CONJURE_API dotnet_script_host final : public script_host {
        private:
            conjure_managed_api mManagedApi = {};
            conjure_native_api *mNativeApi = nullptr;
            script_host_status mStatus = script_host_status::UNINITIALIZED;
            host_mode mMode = host_mode::FRAMEWORK_DEPENDENT;
            std::filesystem::path mManagedDirectory = {};

        public:
            dotnet_script_host();
            ~dotnet_script_host() override;

        public:
            /// <summary>
            /// Steps 1-10 of the hosting sequence: locate and load hostfxr, start (or attach to) the runtime,
            /// load Conjure.Managed into the default load context, resolve Initialize and hand it both
            /// tables. RUNTIME_NOT_FOUND, HOSTFXR_INCOMPATIBLE, RUNTIME_CONFIG_INVALID, MANAGED_SDK_MISSING,
            /// HOST_ALREADY_RUNNING, ABI_MISMATCH, WRONG_THREAD.
            /// </summary>
            [[nodiscard]] core::result<void> initialize(
              const script_host_config &config,
              conjure_native_api &nativeApi
            ) override;

            /// <summary>
            /// managed.shutdown(): destroys instances, unloads the game load context. READY -> UNINITIALIZED.
            /// </summary>
            void shutdown() override;

            [[nodiscard]] const conjure_managed_api &managed_api() const noexcept override;
            [[nodiscard]] script_host_status status() const noexcept override;
            [[nodiscard]] script_backend backend() const noexcept override;

            /// <summary>
            /// The hosting mode the process runtime was started in (meaningful once READY).
            /// </summary>
            [[nodiscard]] host_mode mode() const noexcept;

            /// <summary>
            /// The managed directory the process runtime was started from (meaningful once READY). A later
            /// host asking for another directory attaches to this one with a WARN (D1).
            /// </summary>
            [[nodiscard]] const std::filesystem::path &managed_directory() const noexcept;

            /// <summary>
            /// True once any host in this process started the CLR.
            /// </summary>
            [[nodiscard]] static bool is_runtime_started() noexcept;
    };
}
