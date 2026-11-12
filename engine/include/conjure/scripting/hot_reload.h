#pragma once

#include "conjure/scripting/script_runtime.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>

// Hot reload driver (docs/design/03_scripting.md 3.8). Requests (editor build success, the script.reload
// command, or the controller's own watch on the output assembly) are debounced and executed by tick()
// at the frame boundary, before the first script phase.

namespace conjure::scripting {

    enum class reload_state : u8 {
        IDLE,
        PENDING,        // requested, waiting for the debounce window
        RELOADING       // reload_game_assembly in progress (visible only from inside on_reloaded)
    };

    class CONJURE_API hot_reload_controller {
        private:
            static constexpr u32 WATCH_RETRY_LIMIT = 3;     // retries of a watched load that failed

            script_runtime *mRuntime = nullptr;
            std::filesystem::path mAssemblyPath = {};
            reload_state mState = reload_state::IDLE;
            f64 mRequestTime = 0.0;
            f64 mDebounceSeconds = 0.2;
            f64 mWatchIntervalSeconds = 0.0;
            f64 mLastWatchTime = 0.0;
            bool mHasWatchStamp = false;
            std::filesystem::file_time_type mWatchStamp = {};
            bool mImmediate = false;
            bool mWatchTriggered = false;       // the pending reload was requested by the watch
            u32 mWatchRetriesLeft = 0;
            u32 mReloadCount = 0;
            u32 mRequestCount = 0;
            core::result<void> mLastResult = core::result<void>::success();

        public:
            hot_reload_controller(script_runtime &runtime, std::filesystem::path assemblyPath);
            ~hot_reload_controller();

            hot_reload_controller(const hot_reload_controller &) = delete;
            hot_reload_controller &operator=(const hot_reload_controller &) = delete;

        public:
            /// <summary>
            /// Asks for a reload. Requests inside the debounce window coalesce into one; 'immediate' makes
            /// the next tick() reload regardless of the window.
            /// </summary>
            void request(bool immediate = false);

            /// <summary>
            /// Call once per frame before the first script phase with the frame time; performs a due reload
            /// (and polls the assembly file when watching).
            /// </summary>
            void tick(f64 nowSeconds);

            [[nodiscard]] reload_state state() const noexcept {
                return mState;
            }

            /// <summary>
            /// Seconds a request waits before tick() executes it (default 0.2).
            /// </summary>
            void set_debounce_seconds(f64 seconds) noexcept;

            [[nodiscard]] f64 debounce_seconds() const noexcept {
                return mDebounceSeconds;
            }

            /// <summary>
            /// Polls the write time of the assembly and of the .pdb next to it (the newer of the two) every
            /// 'seconds' inside tick() and requests a reload when it changed; 0 (default) disables the watch. A
            /// reload the watch requested that fails with ASSEMBLY_LOAD_FAILED (a build still writing or locking
            /// the files) is retried up to 3 times, one debounce window apart (a WARN each); a reload asked for
            /// with request() is not retried.
            /// </summary>
            void set_watch_interval_seconds(f64 seconds) noexcept;

            [[nodiscard]] f64 watch_interval_seconds() const noexcept {
                return mWatchIntervalSeconds;
            }

            void set_assembly_path(std::filesystem::path assemblyPath);

            [[nodiscard]] const std::filesystem::path &assembly_path() const noexcept {
                return mAssemblyPath;
            }

            /// <summary>
            /// Reload attempts tick() performed, including the retries of a failed watched reload.
            /// </summary>
            [[nodiscard]] u32 reload_count() const noexcept {
                return mReloadCount;
            }

            /// <summary>
            /// request() calls received (coalesced or not).
            /// </summary>
            [[nodiscard]] u32 request_count() const noexcept {
                return mRequestCount;
            }

            /// <summary>
            /// Outcome of the last reload tick() performed (success before any).
            /// </summary>
            [[nodiscard]] const core::result<void> &last_result() const noexcept {
                return mLastResult;
            }

        private:
            void perform_reload();
            void poll_assembly_file(f64 nowSeconds);
    };

    /// <summary>
    /// The upper-case name of a state ("PENDING").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(reload_state state) noexcept;
}
