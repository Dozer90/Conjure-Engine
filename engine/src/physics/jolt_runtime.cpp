#include "physics/jolt_runtime.h"

#include "physics/jolt_include.h"
#include "physics/jolt_memory.h"

#include "conjure/physics/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/RegisterTypes.h>

#include <cstdarg>
#include <cstdio>
#include <memory>
#include <mutex>

namespace conjure::physics::jolt_runtime {

namespace {

    std::mutex sMutex = {};
    u32 sReferences = 0;
    std::unique_ptr<JPH::Factory> sFactory = nullptr;

    void trace_to_log(const char *format, ...) {
        char buffer[1024] = {};
        va_list arguments = {};
        va_start(arguments, format);
        std::vsnprintf(buffer, sizeof(buffer), format, arguments);
        va_end(arguments);
        CONJURE_LOG_DEBUG(LOG_PHYSICS, "jolt: {}", buffer);
    }

#ifdef JPH_ENABLE_ASSERTS
    bool assert_to_log(const char *expression, const char *message, const char *file, JPH::uint line) {
        CONJURE_LOG_ERROR(
          LOG_PHYSICS,
          "jolt assertion '{}' failed at {}:{}: {}",
          expression == nullptr ? "" : expression,
          file == nullptr ? "" : file,
          line,
          message == nullptr ? "" : message
        );
        CONJURE_ASSERT(false, "jolt assertion failed: {}", expression == nullptr ? "" : expression);
        // The engine's assert handler decided about breaking already.
        return false;
    }
#endif
}

void acquire() {
    const std::scoped_lock lock(sMutex);
    ++sReferences;
    if (sReferences != 1) {
        return;
    }

    jolt_memory::install_hooks();
    JPH::Trace = &trace_to_log;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = &assert_to_log;)

    // Factory carries JPH_OVERRIDE_NEW_DELETE, so this one `new` routes through the hooks above (P4).
    sFactory = std::make_unique<JPH::Factory>();
    JPH::Factory::sInstance = sFactory.get();
    JPH::RegisterTypes();
    CONJURE_LOG_DEBUG(
      LOG_PHYSICS,
      "jolt {}.{}.{} initialised",
      JPH_VERSION_MAJOR,
      JPH_VERSION_MINOR,
      JPH_VERSION_PATCH
    );
}

void release() {
    const std::scoped_lock lock(sMutex);
    if (sReferences == 0) {
        CONJURE_ASSERT(false, "jolt_runtime::release without a matching acquire");
        return;
    }
    --sReferences;
    if (sReferences != 0) {
        return;
    }

    JPH::UnregisterTypes();
    JPH::Factory::sInstance = nullptr;
    sFactory.reset();
}

u32 reference_count() noexcept {
    const std::scoped_lock lock(sMutex);
    return sReferences;
}

guard::guard() {
    acquire();
}

guard::~guard() {
    release();
}
}
