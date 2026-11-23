#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/engine_subsystem.h"

#include "conjure/core/event.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING

#include "conjure/platform/input_state.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_runtime.h"

#include <filesystem>
#include <vector>

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/contact_events.h"
#endif

// Startup steps 14 and 17 (docs/design/06_engine_loop.md 3.4, R27/R29/R36): step 14 constructs the
// script_runtime over the .NET host (or the null host for tests and tools) and registers script_component;
// the bindings subsystem fills the native table at step 16; step 17 runs initialise (handshake) and
// load_game_assembly. A missing runtime is SCRIPT_RUNTIME_NOT_FOUND when scripting.required, otherwise a WARN
// and a null context pointer. With scripting.enabled == false nothing is constructed and one INFO line says so;
// when it is the headless default that holds back a project with a game assembly, the line names --scripting
// (D12).
// Every frame (step 5) the subsystem writes the conjure_frame_block from engine_time and the input snapshot.
// Before a scene unloads it runs destroy_all(true) so OnDestroy sees live entities (R29); physics contact
// events become collision events for scripting.collision_dispatch.

namespace conjure::engine {

    class scripting_subsystem final : public engine_subsystem {
        private:
            engine_services *mServices = nullptr;
            core::event_subscription mUnloadSubscription = {};
            std::vector<scripting::collision_event> mCollisions = {};
            u32 mFrameBlocksWritten = 0;
            bool mInitialised = false;

        public:
            explicit scripting_subsystem(engine_services &services);
            ~scripting_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;
            [[nodiscard]] core::result<void> startup(engine_context &context) override;
            [[nodiscard]] core::result<void> post_startup(engine_context &context) override;
            void register_systems(ecs::scheduler &scheduler, engine_context &context) override;
            void begin_frame(engine_context &context) override;
            void shutdown(engine_context &context) override;

            /// <summary>
            /// True once initialise() succeeded (the runtime is ready for the game assembly).
            /// </summary>
            [[nodiscard]] bool is_initialised() const noexcept {
                return mInitialised;
            }

            /// <summary>
            /// Frame blocks written so far (one per frame while the runtime exists).
            /// </summary>
            [[nodiscard]] u32 frame_blocks_written() const noexcept {
                return mFrameBlocksWritten;
            }

            /// <summary>
            /// True when scripting is off although the loaded project has scripts to run (it enables scripting
            /// and names a game assembly) and the run is headless outside the editor: the case --scripting
            /// exists for (06 D12). False without a project, when the project itself disables scripting, when
            /// it names no game assembly (a project.json without a "scripting" section enables scripting by
            /// default and has no script to hold back), for a windowed run, and for the editor, whose headless
            /// mode chooses its own scripting host. A headless run started with --no-scripting on a scripted
            /// project is true as well: the configuration does not record which flag was given.
            /// </summary>
            [[nodiscard]] static bool headless_holds_scripts_back(
              const engine_config &config,
              const project *loaded
            ) noexcept;

            /// <summary>
            /// Copies engine_time and an input snapshot into the managed frame block (the body of begin_frame).
            /// </summary>
            static void write_frame_block(
              conjure_frame_block &block,
              const engine_time &time,
              const platform::input_state &input
            ) noexcept;

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
            /// <summary>
            /// Physics contacts as scripting collision events: STAY contacts are dropped, the four others map
            /// one to one.
            /// </summary>
            static void convert_contacts(
              std::span<const physics::contact_event> contacts,
              std::vector<scripting::collision_event> &out
            );
#endif

        private:
            void disable(engine_context &context);
    };
}

#endif
