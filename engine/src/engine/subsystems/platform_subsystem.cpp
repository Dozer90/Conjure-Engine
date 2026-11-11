#include "engine/subsystems/platform_subsystem.h"

#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/log_category.h"

#include "conjure/core/log.h"
#include "conjure/platform/backend.h"
#include "conjure/platform/input.h"

#include <format>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

platform_subsystem::platform_subsystem(engine_services &services) :
    mServices(&services) {}

platform_subsystem::~platform_subsystem() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string_view platform_subsystem::name() const noexcept {
    return "platform";
}

core::result<void> platform_subsystem::startup(engine_context &context) {
    const platform::window_desc desc = make_window_desc(*context.config);
    core::result<std::unique_ptr<platform::window>> created = platform::create_window(desc);
    if (!created.has_value()) {
        return make_error(
          engine_error::WINDOW_CREATE_FAILED,
          std::format("window creation failed: {}", created.get_error().to_string())
        );
    }
    mServices->window = std::move(created).value();
    mServices->input = std::make_unique<platform::input>();
    context.window = mServices->window.get();
    context.input = mServices->input.get();
    CONJURE_LOG_INFO(
      LOG_ENGINE,
      "platform: {} window {} x {} ({})",
      platform::to_string(mServices->window->backend()),
      desc.width,
      desc.height,
      context.config->headless ? "headless" : "interactive"
    );
    return core::result<void>::success();
}

void platform_subsystem::shutdown(engine_context &context) {
    context.input = nullptr;
    context.window = nullptr;
    mServices->input.reset();
    mServices->window.reset();
}

platform::window_desc platform_subsystem::make_window_desc(const engine_config &config) {
    platform::window_desc desc = {};
    desc.backend = config.headless ? platform::backend_kind::NULL_BACKEND : platform::default_backend_kind();
    desc.title = config.window.title;
    desc.width = config.window.width;
    desc.height = config.window.height;
    desc.fullscreen = config.window.fullscreen;
    desc.flags = platform::window_flags::DEFAULT;
    if (!config.window.resizable) {
        desc.flags &= ~platform::window_flags::RESIZABLE;
    }
    return desc;
}
