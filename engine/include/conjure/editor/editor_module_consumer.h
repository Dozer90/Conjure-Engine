#pragma once

#include "conjure/editor/command_registry.h"
#include "conjure/editor/panel_desc.h"

#include "conjure/core/api.h"
#include "conjure/core/event.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/modules/module_consumer.h"
#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_types.h"

#include <span>
#include <string>
#include <vector>

// The editor's module consumer (docs/design/04_editor.md D13, R14): turns editor.command records into
// command_descriptors owned by the module and keeps the module's editor.panel records for the UI's panel
// registry; detach removes both. It vetoes an unload with UNLOAD_BLOCKED while the editor plays, because
// scripts and systems of the module may be live.

namespace conjure::editor {

    class play_controller;

    /// <summary>
    /// A panel a loaded module provides.
    /// </summary>
    struct module_panel_entry {
        std::string owner = "";
        std::string title = "";
        panel_desc desc = {};
        modules::module_id module = {};
    };

    class CONJURE_API editor_module_consumer final : public modules::module_consumer {
        private:
            command_registry *mRegistry = nullptr;
            const play_controller *mPlay = nullptr;
            std::vector<module_panel_entry> mPanels = {};
            std::vector<std::string> mModuleNames = {};     // owner strings, index = position in mAttached
            std::vector<modules::module_id> mAttached = {};
            core::event<> mOnPanelsChanged = {};

        public:
            editor_module_consumer(command_registry &registry, const play_controller *play);
            ~editor_module_consumer() override;

        public:
            [[nodiscard]] core::string_id name() const noexcept override;
            [[nodiscard]] core::result<void> validate(
              modules::module_id id,
              const modules::module_registration_set &set,
              const modules::module_registry &registry
            ) override;
            void attach(
              modules::module_id id,
              modules::module_context &context,
              const modules::module_registration_set &set
            ) override;
            [[nodiscard]] core::result<void> can_detach(modules::module_id id, modules::unload_policy policy) override;
            void detach(modules::module_id id, modules::reload_context *reload) override;
            void restore(modules::module_id id, const modules::reload_context &reload) override;
            [[nodiscard]] u32 live_use_count(modules::module_id id) const override;

            /// <summary>
            /// The panels of every attached module, in attach order.
            /// </summary>
            [[nodiscard]] std::span<const module_panel_entry> panels() const noexcept {
                return mPanels;
            }

            /// <summary>
            /// Emitted after attach or detach changed the panel list.
            /// </summary>
            [[nodiscard]] core::event<> &on_panels_changed() noexcept {
                return mOnPanelsChanged;
            }

            /// <summary>
            /// Modules whose records are currently applied.
            /// </summary>
            [[nodiscard]] std::span<const modules::module_id> attached() const noexcept {
                return mAttached;
            }
    };
}
