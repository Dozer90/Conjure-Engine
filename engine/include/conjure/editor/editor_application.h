#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_options.h"
#include "conjure/editor/protocol/protocol_server.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/layer.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

// The editor process object (docs/design/04_editor.md 3.1, 3.2, 5): owns the engine::application, the
// editor_context and the protocol_server, drives the frame integration headless (through a core layer) or
// with the UI layer the executable injects, restarts the engine when project.open names another project and
// runs --export / --exec / the interactive loop.

namespace conjure::editor {

    class CONJURE_API editor_application final : public editor_host {
        public:
            using layer_factory = std::function<std::unique_ptr<engine::layer>(editor_context &context)>;

        private:
            editor_options mOptions = {};
            layer_factory mUiFactory = {};
            std::unique_ptr<engine::application> mEngine = nullptr;
            std::unique_ptr<editor_context> mContext = nullptr;
            std::unique_ptr<protocol_server> mServer = nullptr;
            std::optional<std::filesystem::path> mPendingProject = std::nullopt;
            int mExitCode = 0;
            u32 mTicks = 0;
            bool mQuitRequested = false;
            bool mInitialised = false;

        public:
            editor_application();
            ~editor_application() override;

        public:
            /// <summary>
            /// Builds the engine over the options (headless: null platform, rhi and audio; the scripting
            /// runtime on the null host so script.* commands work without .NET), initialises it, creates the
            /// context, pushes the frame layer (and the UI layer from the factory when not headless) and
            /// starts the protocol server. Fails with the engine's error (exit 2 in main) or IO_ERROR when an
            /// explicit port is busy.
            /// </summary>
            [[nodiscard]] core::result<void> initialise(const editor_options &options, layer_factory uiFactory);

            /// <summary>
            /// --export: runs project.export and returns 0 or 1. --exec: the headless runner's exit code.
            /// Otherwise frames until quit; returns the exit code.
            /// </summary>
            [[nodiscard]] int run();

            /// <summary>
            /// Runs one engine frame (the frame layer ticks the context) and applies a pending project
            /// restart afterwards. Tests and the headless runner pump with it.
            /// </summary>
            [[nodiscard]] core::result<void> pump_frame();

            /// <summary>
            /// The frame body outside a frame (tests): what the frame layer calls once per frame.
            /// </summary>
            void headless_tick();

            /// <summary>
            /// Stops the server and shuts the engine down (idempotent).
            /// </summary>
            void shutdown();

            void request_quit(int exitCode) override;
            [[nodiscard]] core::result<void> request_open_project(const std::filesystem::path &directory) override;
            [[nodiscard]] bool has_pending_project_open() const noexcept override;

            [[nodiscard]] bool quit_requested() const noexcept {
                return mQuitRequested;
            }

            [[nodiscard]] int exit_code() const noexcept {
                return mExitCode;
            }

            [[nodiscard]] editor_context &context() noexcept {
                return *mContext;
            }

            [[nodiscard]] engine::application &engine() noexcept {
                return *mEngine;
            }

            [[nodiscard]] protocol_server &server() noexcept {
                return *mServer;
            }

            [[nodiscard]] const editor_options &options() const noexcept {
                return mOptions;
            }

            [[nodiscard]] bool is_initialised() const noexcept {
                return mInitialised;
            }

            /// <summary>
            /// Frames pumped since initialise().
            /// </summary>
            [[nodiscard]] u32 ticks() const noexcept {
                return mTicks;
            }

            /// <summary>
            /// The engine configuration the options produce for a project directory (empty = none): the
            /// merge of engine_config::defaults() (editor = true), --headless, project.json and the engine
            /// arguments, plus the editor's scripting policy. Exposed for tests.
            /// </summary>
            [[nodiscard]] static core::result<engine::application_desc> make_engine_desc(
              const editor_options &options,
              const std::filesystem::path &projectPath
            );

        private:
            [[nodiscard]] core::result<void> start_engine(const std::filesystem::path &projectPath);
            [[nodiscard]] core::result<void> apply_pending_project_open();
            [[nodiscard]] int run_export();
            [[nodiscard]] int run_exec();
    };
}
