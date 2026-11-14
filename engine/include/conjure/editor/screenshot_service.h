#pragma once

#include "conjure/editor/editor_command.h"

#include "conjure/core/api.h"
#include "conjure/core/image.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Screenshots (docs/design/04_editor.md 3.10, R12): view.screenshot parks its request, the service waits the
// asked number of frames, captures an RGBA8 image through its backend on the main thread, then hands the image
// to a core::jobs job that encodes the PNG and writes the file (a 16384 x 16384 capture must not stall the
// frame); the job posts the completion back to the main thread, which answers {path, width, height, frame}.
// Without a job system the encode runs inline. The renderer backend draws the scene through the requested
// camera ("editor", "game" or a camera entity) into a render target of the requested size and captures it
// through renderer::read_back once a frame has been rendered into it; on the null RHI (every headless run)
// there is no backend and the request fails with NOT_AVAILABLE. Tests install a memory backend that paints a
// solid colour.

namespace conjure::editor {

    class editor_context;

    struct screenshot_request {
        std::filesystem::path path = {};
        // "editor", "game" or an entity guid (view.screenshot resolves a path or name to it).
        std::string camera = "editor";
        std::string format = "png";
        u32 width = 1280;
        u32 height = 720;
        u32 waitFrames = 0;
    };

    /// <summary>
    /// Produces the pixels. begin() runs when the request is accepted and arranges for the picture to be drawn
    /// in the frame being built; prepare_frame() runs on every later frame the request still waits, to have it
    /// drawn again; frame_rendered() says whether a frame has been drawn since it was last asked; capture()
    /// runs once enough frames were.
    /// </summary>
    class CONJURE_API screenshot_backend {
        public:
            screenshot_backend();
            virtual ~screenshot_backend();

            screenshot_backend(const screenshot_backend &) = delete;
            screenshot_backend &operator=(const screenshot_backend &) = delete;

        public:
            [[nodiscard]] virtual core::result<void> begin(const screenshot_request &request) = 0;

            /// <summary>
            /// Asks for the picture to be drawn in the frame being built. The default does nothing (a backend
            /// that needs no frame, such as memory_screenshot_backend).
            /// </summary>
            virtual void prepare_frame(const screenshot_request &request);

            /// <summary>
            /// True when a frame was drawn for the request since the last call (or since begin()). The service
            /// counts a waited frame only then, so a request is never captured before the engine has rendered
            /// what begin() asked for. The default is true.
            /// </summary>
            [[nodiscard]] virtual bool frame_rendered(const screenshot_request &request);

            [[nodiscard]] virtual core::result<core::image> capture(const screenshot_request &request) = 0;

            /// <summary>
            /// Forgets what begin() set up without touching the engine. The service calls it when it cancels
            /// its waiting requests, which happens when the engine restarts or shuts down: the renderer the
            /// backend drew with may already be gone, and takes its own resources with it. The default does
            /// nothing.
            /// </summary>
            virtual void abandon() noexcept;
    };

    /// <summary>
    /// A backend that paints every pixel one colour (tests, documentation runs).
    /// </summary>
    class CONJURE_API memory_screenshot_backend final : public screenshot_backend {
        private:
            core::rgba8 mColour = core::rgba8{32, 64, 96, 255};
            u32 mBegun = 0;
            u32 mCaptured = 0;

        public:
            explicit memory_screenshot_backend(core::rgba8 colour = core::rgba8{32, 64, 96, 255});
            ~memory_screenshot_backend() override;

        public:
            [[nodiscard]] core::result<void> begin(const screenshot_request &request) override;
            [[nodiscard]] core::result<core::image> capture(const screenshot_request &request) override;

            [[nodiscard]] u32 begun() const noexcept {
                return mBegun;
            }

            [[nodiscard]] u32 captured() const noexcept {
                return mCaptured;
            }
    };

    class CONJURE_API screenshot_service {
        private:
            struct pending {
                screenshot_request request = {};
                async_reply reply = {};
                u32 framesLeft = 0;
                bool begun = false;     // the backend's begin() ran for it; only the first request's has
            };

            editor_context *mContext = nullptr;
            std::unique_ptr<screenshot_backend> mBackend = nullptr;
            std::vector<pending> mPending = {};
            std::shared_ptr<bool> mAlive = nullptr;         // completions posted from jobs check it before touching us
            u32 mCompleted = 0;
            u32 mEncoding = 0;                              // captures handed to a job and not yet completed

        public:
            explicit screenshot_service(editor_context &context);
            ~screenshot_service();

            screenshot_service(const screenshot_service &) = delete;
            screenshot_service &operator=(const screenshot_service &) = delete;

        public:
            /// <summary>
            /// Installs the backend (nullptr = none: requests fail with NOT_AVAILABLE).
            /// </summary>
            void set_backend(std::unique_ptr<screenshot_backend> backend);

            [[nodiscard]] bool available() const noexcept {
                return mBackend != nullptr;
            }

            /// <summary>
            /// Validates and parks a request: INVALID_ARGS for an empty path, a zero size or a format other than
            /// "png"; NOT_AVAILABLE without a backend. On failure the reply is completed with that error.
            /// Pictures are taken one at a time, in request order: a request made while another waits is
            /// begun with the backend when its turn comes, and a refusal by the backend (a camera that does
            /// not exist) is answered then.
            /// </summary>
            void request(const screenshot_request &value, async_reply reply);

            /// <summary>
            /// Once per frame, for the first request: counts down its wait (a frame counts once the backend
            /// drew one for it), has the backend draw it again while it waits, captures it when due and hands
            /// the image to a job that encodes, writes and completes the reply on the main thread (IO_ERROR
            /// when the file cannot be written); the request behind it is begun in the same call.
            /// </summary>
            void tick();

            /// <summary>
            /// Fails every waiting request with CANCELLED (engine restart, shutdown) and forgets it; captures
            /// already encoding finish writing their file but their reply was cancelled by the dispatcher.
            /// </summary>
            void cancel_pending();

            /// <summary>
            /// Requests still waiting for their frame (not the captures being encoded).
            /// </summary>
            [[nodiscard]] usize pending_count() const noexcept {
                return mPending.size();
            }

            /// <summary>
            /// Captures handed to a job whose completion has not run yet.
            /// </summary>
            [[nodiscard]] u32 encoding_count() const noexcept {
                return mEncoding;
            }

            [[nodiscard]] u32 completed_count() const noexcept {
                return mCompleted;
            }

            /// <summary>
            /// Parses the view.screenshot arguments: INVALID_ARGS for an empty path, a zero or oversized size
            /// (above 16384), a format other than "png", an empty camera or a waitFrames outside 0..100000; any
            /// camera other than "editor" / "game" names an entity, which view.screenshot resolves to its guid.
            /// </summary>
            [[nodiscard]] static core::result<screenshot_request> parse_request(const core::json::value &args);

            /// <summary>
            /// The renderer-backed backend when the context has a renderer on a real device; nullptr on the
            /// null device, without rendering or without the renderer compiled in.
            /// </summary>
            [[nodiscard]] static std::unique_ptr<screenshot_backend> create_renderer_backend(editor_context &context);

        private:
            void encode_and_complete(screenshot_request request, async_reply reply, core::image image);
            void finish(const screenshot_request &request, const async_reply &reply, const core::result<void> &written);
    };
}
