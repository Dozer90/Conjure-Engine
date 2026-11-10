#pragma once

#include "conjure/scripting/script_runtime.h"

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

    class hot_reload_controller {
        private:
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
            /// Polls the assembly's write time every 'seconds' inside tick() and requests a reload when it
            /// changed; 0 (default) disables the watch.
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
            /// Reloads performed by tick().
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
    [[nodiscard]] std::string_view to_string(reload_state state) noexcept;
}
