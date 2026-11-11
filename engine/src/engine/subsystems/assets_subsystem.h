#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/engine_subsystem.h"

#include "conjure/core/result.h"

#include <string>

// Startup step 8 (docs/design/06_engine_loop.md 3.4, R33): the asset manager over the cache source (a
// development project) or the pack source (a cooked project), the database with its "assets" mount and the
// polling file watcher + hot reload controller (development, non-Distribution). scan() and import_stale()
// wait for post_startup (step 17) so every module registered its importers first. Every frame:
// begin_frame / update / hot reload at frame step 5, end_frame at step 13.

namespace conjure::engine {

    class assets_subsystem final : public engine_subsystem {
        private:
            engine_services *mServices = nullptr;
            std::string mAssetsMountRoot = {};      // database-relative root of the "assets" mount
            u32 mImportedThisStartup = 0;

        public:
            explicit assets_subsystem(engine_services &services);
            ~assets_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;
            [[nodiscard]] core::result<void> startup(engine_context &context) override;
            [[nodiscard]] core::result<void> post_startup(engine_context &context) override;
            void begin_frame(engine_context &context) override;
            void end_frame(engine_context &context) override;
            void shutdown(engine_context &context) override;

            /// <summary>
            /// Sources imported (or re-imported) by the startup scan.
            /// </summary>
            [[nodiscard]] u32 imported_at_startup() const noexcept {
                return mImportedThisStartup;
            }

            /// <summary>
            /// The cache platform name of this build ("windows", "linux", "macos").
            /// </summary>
            [[nodiscard]] static std::string_view cache_platform() noexcept;

            /// <summary>
            /// The cache directory relative to the project root.
            /// </summary>
            static constexpr std::string_view CACHE_ROOT = ".conjure/cache";

            /// <summary>
            /// The pack file name inside the cooked root.
            /// </summary>
            static constexpr std::string_view PACK_FILE_NAME = "content.cpak";
    };
}
