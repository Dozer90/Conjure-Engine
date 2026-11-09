#include "conjure/platform/platform.h"
#include "conjure/platform/log_category.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/platform_error.h"
#include "conjure/platform/window.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"

#if CONJURE_PLATFORM_HAS_GLFW
    #include "platform/glfw/glfw_context.h"
    #include "platform/glfw/glfw_window.h"
#endif

#include <memory>
#include <utility>

// Module-level pieces: the log category, the window base class, description validation, the
// create_window factory and the backend queries (docs/design/11_platform.md 4, 6).

using namespace conjure;
using namespace conjure::platform;

namespace conjure::platform {
    CONJURE_DEFINE_LOG_CATEGORY(LOG_PLATFORM, "platform");
}

namespace {
    // One live window per process in v1 (P1, section 5). Main-thread only, like every window call.
    u32 sLiveWindowCount = 0;
}

/*===========================================================================*/
/*===========================================================================*/
// window

window::window() {
    ++sLiveWindowCount;
}

window::~window() {
    CONJURE_ASSERT(sLiveWindowCount > 0, "window destroyed more often than created");
    if (sLiveWindowCount > 0) {
        --sLiveWindowCount;
    }
}

null_backend *window::as_null() noexcept {
    return nullptr;
}

/*===========================================================================*/
/*===========================================================================*/
// Factory

core::result<void> conjure::platform::validate_window_desc(const window_desc &desc) {
    if (desc.backend >= backend_kind::COUNT) {
        return core::make_error(
          platform_error::INVALID_DESC,
          "window_desc::backend {} is not a backend_kind",
          static_cast<u32>(desc.backend)
        );
    }
    if (!desc.fullscreen && (desc.width == 0 || desc.height == 0)) {
        return core::make_error(
          platform_error::INVALID_DESC,
          "window_desc::width / height must be non-zero for a windowed window (got {} x {})",
          desc.width,
          desc.height
        );
    }
    if (desc.title.size() > MAX_WINDOW_TITLE_BYTES) {
        return core::make_error(
          platform_error::INVALID_DESC,
          "window_desc::title is {} bytes, the maximum is {}",
          desc.title.size(),
          MAX_WINDOW_TITLE_BYTES
        );
    }
    return core::result<void>::success();
}

core::result<std::unique_ptr<window>> conjure::platform::create_window(const window_desc &desc) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "create_window must run on the main thread");

    CONJURE_TRY(validate_window_desc(desc));

    if (sLiveWindowCount != 0) {
        CONJURE_ASSERT(false, "create_window: a window is already alive (one live window per process in v1)");
        return core::make_error(platform_error::INVALID_DESC, "a window is already alive; v1 allows exactly one");
    }

    if (desc.backend == backend_kind::NULL_BACKEND) {
        if (desc.fullscreen && desc.monitorIndex != 0) {
            return core::make_error(
              platform_error::MONITOR_NOT_FOUND,
              "monitor index {} out of range (the null backend has 1 monitor)",
              desc.monitorIndex
            );
        }
        std::unique_ptr<window> created = null_backend::create(desc);
        CONJURE_LOG_INFO(LOG_PLATFORM, "Created null window {} x {} '{}'", desc.width, desc.height, desc.title);
        return core::result<std::unique_ptr<window>>(std::move(created));
    }

#if CONJURE_PLATFORM_HAS_GLFW
    return glfw::glfw_window::create(desc);
#else
    return core::make_error(
      platform_error::BACKEND_NOT_COMPILED,
      "backend '{}' was not compiled into this build (CONJURE_PLATFORM_GLFW is OFF)",
      to_string(desc.backend)
    );
#endif
}

u32 conjure::platform::live_window_count() noexcept {
    return sLiveWindowCount;
}

/*===========================================================================*/
/*===========================================================================*/
// Backend queries

bool conjure::platform::is_backend_compiled(backend_kind kind) noexcept {
    switch (kind) {
        case backend_kind::NULL_BACKEND:
            return true;
        case backend_kind::GLFW:
            return CONJURE_PLATFORM_HAS_GLFW != 0;
        case backend_kind::COUNT:
            break;
    }
    return false;
}

backend_kind conjure::platform::default_backend_kind() noexcept {
    if (is_backend_compiled(backend_kind::GLFW)) {
        return backend_kind::GLFW;
    }
    return backend_kind::NULL_BACKEND;
}

std::string_view conjure::platform::backend_version_string(backend_kind kind) noexcept {
    switch (kind) {
        case backend_kind::NULL_BACKEND:
            return "null";
        case backend_kind::GLFW:
#if CONJURE_PLATFORM_HAS_GLFW
            return glfw::version_string();
#else
            return "glfw (not compiled)";
#endif
        case backend_kind::COUNT:
            break;
    }
    return "";
}
