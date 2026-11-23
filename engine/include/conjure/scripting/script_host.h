#pragma once

#include "conjure/scripting/native_api.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <memory>

// The host interface (docs/design/03_scripting.md 3.1, 4, 7): the object that owns the managed side of
// the two ABI tables. dotnet_script_host runs the real CLR, null_script_host is the C++ fake every other
// module's tests use. Both are created through create_script_host().

namespace conjure::scripting {

    /// <summary>
    /// Which managed side a host talks to.
    /// </summary>
    enum class script_backend : u8 {
        DOTNET,
        NULL_HOST
    };

    enum class script_host_status : u8 {
        UNINITIALISED,
        READY,
        DISABLED        // initialise failed; the host stays usable as an object but runs nothing
    };

    /// <summary>
    /// How the CLR is hosted (D2/D14): framework-dependent through the runtime config, self-contained
    /// through the bootstrap command line.
    /// </summary>
    enum class host_mode : u8 {
        FRAMEWORK_DEPENDENT,
        SELF_CONTAINED
    };

    /// <summary>
    /// What hosting paths this build supports on this platform.
    /// </summary>
    struct host_capabilities {
        bool frameworkDependent = true;
        bool selfContained = true;
    };

    struct script_host_config {
        script_backend backend = script_backend::DOTNET;
        std::filesystem::path managedDirectory = {};        // Conjure.Bootstrap.* and Conjure.Managed.dll
        std::filesystem::path dotnetRootOverride = {};      // empty = nethost default lookup
        bool waitForDebugger = false;
    };

    class CONJURE_API script_host {
        public:
            script_host();
            virtual ~script_host();

            script_host(const script_host &) = delete;
            script_host &operator=(const script_host &) = delete;

        public:
            /// <summary>
            /// Starts (or attaches to) the runtime and performs the ABI handshake over 'nativeApi', which the
            /// caller must have filled and which must outlive the host. Main thread only. Failure leaves the
            /// host DISABLED.
            /// </summary>
            [[nodiscard]] virtual core::result<void> initialise(
              const script_host_config &config,
              conjure_native_api &nativeApi
            ) = 0;

            /// <summary>
            /// Stops managed execution for this host (instances destroyed, game assembly unloaded). The CLR
            /// stays resident until process exit.
            /// </summary>
            virtual void shutdown() = 0;

            /// <summary>
            /// The table the managed side filled during the handshake; all null before READY.
            /// </summary>
            [[nodiscard]] virtual const conjure_managed_api &managed_api() const noexcept = 0;

            [[nodiscard]] virtual script_host_status status() const noexcept = 0;

            [[nodiscard]] virtual script_backend backend() const noexcept = 0;
    };

    /// <summary>
    /// Creates the host for the backend. NULL_HOST never fails to initialise.
    /// </summary>
    [[nodiscard]] CONJURE_API std::unique_ptr<script_host> create_script_host(script_backend backend);

    /// <summary>
    /// Static hostfxr probe (R28): true when nethost finds a .NET runtime without starting one. Used as the
    /// DOTNET_RUNTIME test precondition.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_runtime_available() noexcept;

    /// <summary>
    /// The hosting paths available on this platform and build.
    /// </summary>
    [[nodiscard]] CONJURE_API host_capabilities query_host_capabilities() noexcept;

    /// <summary>
    /// Reads Conjure.Bootstrap.runtimeconfig.json in 'managedDirectory': SELF_CONTAINED when
    /// runtimeOptions.includedFrameworks is present, FRAMEWORK_DEPENDENT otherwise. RUNTIME_CONFIG_INVALID
    /// when the file is missing or malformed.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<host_mode> detect_host_mode(const std::filesystem::path &managedDirectory);

    /// <summary>
    /// The anchor files a managed directory must hold (D2): Conjure.Bootstrap.dll, its runtime config and
    /// Conjure.Managed.dll. MANAGED_SDK_MISSING naming the first absent file.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> validate_managed_directory(
      const std::filesystem::path &managedDirectory
    );

    /// <summary>
    /// The upper-case name of a status ("READY").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(script_host_status status) noexcept;

    /// <summary>
    /// The upper-case name of a backend ("NULL_HOST").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(script_backend backend) noexcept;

    /// <summary>
    /// The upper-case name of a mode ("SELF_CONTAINED").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(host_mode mode) noexcept;
}
