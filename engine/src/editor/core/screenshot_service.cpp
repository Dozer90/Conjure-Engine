#include "conjure/editor/screenshot_service.h"

#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/uuid.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/render_target.h"
    #include "conjure/renderer/renderer.h"
#endif

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

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        /// <summary>
        /// Captures a render target through renderer::read_back. The engine offers no hook to append a view
        /// for the editor camera yet, so the target holds the renderer's clear colour; the frame still goes
        /// through the real device and the read-back path.
        /// </summary>
        class renderer_screenshot_backend final : public screenshot_backend {
            private:
                renderer::renderer *mRenderer = nullptr;
                renderer::render_target_id mTarget = {};

            public:
                explicit renderer_screenshot_backend(renderer::renderer &target) noexcept :
                    mRenderer(&target) {}

                ~renderer_screenshot_backend() override {
                    if (mTarget.is_valid()) {
                        mRenderer->destroy_render_target(mTarget);
                    }
                }

            public:
                core::result<void> begin(const screenshot_request &request) override {
                    if (mTarget.is_valid()) {
                        mRenderer->destroy_render_target(mTarget);
                        mTarget = {};
                    }
                    renderer::render_target_desc desc = {};
                    desc.size = math::uvec2(request.width, request.height);
                    desc.withDepth = true;
                    CONJURE_TRY_ASSIGN(mTarget, mRenderer->create_render_target(desc));
                    return core::result<void>::success();
                }

                core::result<core::image> capture(const screenshot_request &request) override {
                    (void) request;
                    if (!mTarget.is_valid()) {
                        return make_editor_error(protocol_error_code::INTERNAL, "no render target to read back");
                    }
                    core::result<core::image> image = mRenderer->read_back(mTarget);
                    mRenderer->destroy_render_target(mTarget);
                    mTarget = {};
                    return image;
                }
        };
#endif
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Backends

    screenshot_backend::screenshot_backend() = default;
    screenshot_backend::~screenshot_backend() = default;

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
        return std::make_unique<renderer_screenshot_backend>(*target);
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
            const std::string_view camera = args["camera"].as_string();
            if (camera != "editor" && camera != "game" && !core::uuid::parse(camera).has_value()) {
                return make_editor_error(
                  protocol_error_code::INVALID_ARGS,
                  "args.camera: expected \"editor\", \"game\" or an entity guid"
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
        const core::result<void> begun = mBackend->begin(value);
        if (!begun.has_value()) {
            reply.fail(classify_error(begun.get_error()), begun.get_error().message());
            return;
        }
        pending entry = {};
        entry.request = value;
        entry.reply = reply;
        entry.framesLeft = value.waitFrames + 1;
        mPending.push_back(std::move(entry));
    }

    void screenshot_service::tick() {
        for (usize index = 0; index < mPending.size();) {
            pending &entry = mPending[index];
            if (entry.framesLeft > 0) {
                --entry.framesLeft;
            }
            if (entry.framesLeft > 0) {
                ++index;
                continue;
            }
            // Take the entry out first: the inline encode path completes the reply, and a completion may
            // dispatch further commands against this service.
            const pending due = std::move(entry);
            mPending.erase(mPending.begin() + static_cast<isize>(index));
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
