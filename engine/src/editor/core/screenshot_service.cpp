#include "conjure/editor/screenshot_service.h"

#include "editor/core/scene_view.h"

#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/uuid.h"
#include "conjure/engine/engine_config.h"

#include <format>
#include <memory>
#include <system_error>
#include <utility>

namespace conjure::editor {

    namespace {
        constexpr u32 MAX_SCREENSHOT_DIMENSION = 16384;

        /// <summary>
        /// Encodes and writes the PNG (any thread).
        /// </summary>
        core::result<void> write_png(const screenshot_request &request, const core::image &image) {
            std::error_code ignored = {};
            if (request.path.has_parent_path()) {
                std::filesystem::create_directories(request.path.parent_path(), ignored);
            }
            return image.save_png(request.path);
        }
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Backends

    screenshot_backend::screenshot_backend() = default;
    screenshot_backend::~screenshot_backend() = default;

    void screenshot_backend::prepare_frame(const screenshot_request &request) {
        (void) request;
    }

    bool screenshot_backend::frame_rendered(const screenshot_request &request) {
        (void) request;
        return true;
    }

    void screenshot_backend::abandon() noexcept {}

    memory_screenshot_backend::memory_screenshot_backend(core::rgba8 colour) :
        mColour(colour) {}

    memory_screenshot_backend::~memory_screenshot_backend() = default;

    core::result<void> memory_screenshot_backend::begin(const screenshot_request &request) {
        (void) request;
        ++mBegun;
        return core::result<void>::success();
    }

    core::result<core::image> memory_screenshot_backend::capture(const screenshot_request &request) {
        ++mCaptured;
        core::image image(request.width, request.height);
        image.fill(mColour);
        return image;
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // screenshot_service

    screenshot_service::screenshot_service(editor_context &context) :
        mContext(&context),
        mAlive(std::make_shared<bool>(true)) {}

    screenshot_service::~screenshot_service() {
        cancel_pending();
        *mAlive = false;
    }

    void screenshot_service::set_backend(std::unique_ptr<screenshot_backend> backend) {
        mBackend = std::move(backend);
    }

    std::unique_ptr<screenshot_backend> screenshot_service::create_renderer_backend(editor_context &context) {
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        renderer::renderer *target = context.renderer();
        const engine::engine_config *config = context.engine_context().config;
        if (target == nullptr || config == nullptr || !config->rendering.enabled ||
          config->rendering.backend == engine::rhi_backend_choice::NULL_DEVICE) {
            return nullptr;
        }
        return make_renderer_screenshot_backend(context, *target);
#else
        (void) context;
        return nullptr;
#endif
    }

    core::result<screenshot_request> screenshot_service::parse_request(const core::json::value &args) {
        screenshot_request request = {};
        const core::json::value path = args["path"];
        if (!path.is_string() || path.as_string().empty()) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, "args.path: a file path is required");
        }
        request.path = std::filesystem::path(std::string(path.as_string()));
        if (args.contains("width")) {
            const i64 width = args["width"].as_i64(-1);
            if (width <= 0 || width > MAX_SCREENSHOT_DIMENSION) {
                return make_editor_error(protocol_error_code::INVALID_ARGS, "args.width: expected 1..16384");
            }
            request.width = static_cast<u32>(width);
        }
        if (args.contains("height")) {
            const i64 height = args["height"].as_i64(-1);
            if (height <= 0 || height > MAX_SCREENSHOT_DIMENSION) {
                return make_editor_error(protocol_error_code::INVALID_ARGS, "args.height: expected 1..16384");
            }
            request.height = static_cast<u32>(height);
        }
        if (args.contains("camera")) {
            // Any other non-empty value names an entity (guid, path or name); view.screenshot resolves it against
            // the scene and hands the service its guid.
            const std::string_view camera = args["camera"].as_string();
            if (camera.empty()) {
                return make_editor_error(
                  protocol_error_code::INVALID_ARGS,
                  "args.camera: expected \"editor\", \"game\" or an entity (guid, path or name)"
                );
            }
            request.camera = std::string(camera);
        }
        if (args.contains("format")) {
            const std::string_view format = args["format"].as_string();
            if (format != "png") {
                return make_editor_error(
                  protocol_error_code::INVALID_ARGS,
                  std::format("args.format: only \"png\" is supported (got \"{}\")", format)
                );
            }
            request.format = std::string(format);
        }
        if (args.contains("waitFrames")) {
            const i64 waitFrames = args["waitFrames"].as_i64(-1);
            if (waitFrames < 0 || waitFrames > 100000) {
                return make_editor_error(protocol_error_code::INVALID_ARGS, "args.waitFrames: expected 0..100000");
            }
            request.waitFrames = static_cast<u32>(waitFrames);
        }
        return request;
    }

    void screenshot_service::request(const screenshot_request &value, async_reply reply) {
        if (value.path.empty() || value.width == 0 || value.height == 0) {
            reply.fail(protocol_error_code::INVALID_ARGS, "a screenshot needs a path and a non-zero size");
            return;
        }
        if (value.format != "png") {
            reply.fail(protocol_error_code::INVALID_ARGS, "only png screenshots are supported");
            return;
        }
        if (mBackend == nullptr) {
            reply.fail(
              protocol_error_code::NOT_AVAILABLE,
              "screenshots need a real rhi device; the null device (headless) cannot render pixels"
            );
            return;
        }
        pending entry = {};
        entry.request = value;
        entry.reply = reply;
        entry.framesLeft = value.waitFrames + 1;
        // One picture at a time: a backend draws into a single target, so a request made while another waits
        // is begun by tick() when its turn comes. Two begun at once used to share the target, and the first
        // was answered with the second's picture.
        if (mPending.empty()) {
            const core::result<void> begun = mBackend->begin(value);
            if (!begun.has_value()) {
                reply.fail(classify_error(begun.get_error()), begun.get_error().message());
                return;
            }
            entry.begun = true;
        }
        mPending.push_back(std::move(entry));
    }

    void screenshot_service::tick() {
        // Only the first request is with the backend; the ones behind it wait their turn.
        while (!mPending.empty()) {
            pending &entry = mPending.front();
            if (!entry.begun) {
                const core::result<void> begun = mBackend == nullptr
                  ? core::result<void>(make_editor_error(protocol_error_code::NOT_AVAILABLE, "no screenshot backend"))
                  : mBackend->begin(entry.request);
                if (!begun.has_value()) {
                    // Out of the queue before the reply: a completion may dispatch against this service.
                    const async_reply refused = entry.reply;
                    mPending.erase(mPending.begin());
                    if (refused.valid()) {
                        refused.fail(classify_error(begun.get_error()), begun.get_error().message());
                    }
                    continue;
                }
                entry.begun = true;
            }
            // A frame is waited only once the backend has drawn one for the request. A request made by a
            // protocol client is ticked in the very frame it arrived, before anything was rendered for it: it
            // used to be captured right there, an empty target.
            const bool rendered = mBackend == nullptr || mBackend->frame_rendered(entry.request);
            if (rendered && entry.framesLeft > 0) {
                --entry.framesLeft;
            }
            if (!rendered || entry.framesLeft > 0) {
                if (mBackend != nullptr) {
                    mBackend->prepare_frame(entry.request);
                }
                return;
            }
            // Take the entry out first: the inline encode path completes the reply, and a completion may
            // dispatch further commands against this service.
            const pending due = std::move(entry);
            mPending.erase(mPending.begin());
            core::result<core::image> captured = mBackend == nullptr
              ? core::result<core::image>(make_editor_error(
                protocol_error_code::NOT_AVAILABLE,
                "no screenshot backend"
              ))
              : mBackend->capture(due.request);
            if (!captured.has_value()) {
                if (due.reply.valid()) {
                    due.reply.fail(classify_error(captured.get_error()), captured.get_error().message());
                }
                continue;
            }
            encode_and_complete(due.request, due.reply, std::move(captured).value());
        }
    }

    void screenshot_service::cancel_pending() {
        for (pending &entry : mPending) {
            if (entry.reply.valid()) {
                entry.reply.fail(protocol_error_code::CANCELLED, "the screenshot was cancelled");
            }
        }
        mPending.clear();
        // Requests are cancelled when the engine restarts or shuts down. The renderer the backend holds a
        // target in may be gone by now, so the backend lets go of it without calling into it.
        if (mBackend != nullptr) {
            mBackend->abandon();
        }
    }

    void screenshot_service::encode_and_complete(screenshot_request request, async_reply reply, core::image image) {
        core::jobs::job_system *jobs = mContext->engine_context().jobs;
        if (jobs == nullptr || !jobs->is_running() || jobs->worker_count() == 0) {
            finish(request, reply, write_png(request, image));
            return;
        }
        ++mEncoding;
        // The job owns the image; the completion runs on the main thread and checks that the service still
        // exists (the editor may have restarted its engine or shut down meanwhile).
        std::weak_ptr<bool> alive = mAlive;
        screenshot_service *self = this;
        (void) jobs->schedule([self, alive, request = std::move(request), reply, image = std::move(image)]() {
            const core::result<void> written = write_png(request, image);
            core::jobs::post_to_main_thread([self, alive, request, reply, written]() {
                const std::shared_ptr<bool> stillAlive = alive.lock();
                if (stillAlive == nullptr || !*stillAlive) {
                    return;
                }
                --self->mEncoding;
                self->finish(request, reply, written);
            });
        });
    }

    void screenshot_service::finish(
      const screenshot_request &request,
      const async_reply &reply,
      const core::result<void> &written
    ) {
        if (!reply.valid()) {
            return;
        }
        if (!written.has_value()) {
            reply.fail(
              protocol_error_code::IO_ERROR,
              std::format("writing '{}' failed: {}", request.path.generic_string(), written.get_error().message())
            );
            return;
        }
        ++mCompleted;
        reply.succeed(core::json::object({
          {"path", request.path.generic_string()},
          {"width", request.width},
          {"height", request.height},
          {"frame", mContext->frame_index()},
        }));
    }
}
