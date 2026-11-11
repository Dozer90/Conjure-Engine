#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/engine_subsystem.h"

#include "conjure/core/result.h"

// Startup step 9 (docs/design/06_engine_loop.md 3.4): the component registry, the world and the scheduler
// over the job system. Never fails. The scheduler runs stages in parallel only when the job system has
// workers (an inline job system makes every system run on the main thread, which keeps headless tests
// deterministic by construction).

namespace conjure::engine {

    class ecs_subsystem final : public engine_subsystem {
        private:
            engine_services *mServices = nullptr;

        public:
            explicit ecs_subsystem(engine_services &services);
            ~ecs_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;
            [[nodiscard]] core::result<void> startup(engine_context &context) override;
            void shutdown(engine_context &context) override;
    };
}
