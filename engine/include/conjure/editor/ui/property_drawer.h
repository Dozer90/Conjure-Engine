#pragma once

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/types.h"

#include <functional>
#include <string>
#include <string_view>

// Property drawers (docs/design/04_editor.md 4.2): one ImGui widget per reflection field kind. The inspector
// reads a component as JSON, hands each field's value to the drawer for its kind and, when the widget
// reports a change, writes the new value back through component.set. Modules may replace the drawer of a
// kind for their own tooling.

namespace conjure::editor::ui {

    /// <summary>
    /// What a drawer receives: the field description (attributes drive ranges and tooltips) and the value.
    /// </summary>
    struct property_draw_context {
        const core::reflect::field_info *field = nullptr;   // nullptr for unreflected values
        std::string label = "";
        bool readOnly = false;
    };

    /// <summary>
    /// Draws one value. Returns true when the user changed it; 'value' then holds the new JSON.
    /// </summary>
    using draw_fn = std::function<bool(const property_draw_context &context, core::json::document &value)>;

    class CONJURE_API property_drawer_registry {
        private:
            draw_fn mDrawers[static_cast<usize>(core::reflect::field_kind::COUNT)] = {};
            draw_fn mFallback = {};

        public:
            /// <summary>
            /// Installs the built-in drawers for every kind.
            /// </summary>
            property_drawer_registry();
            ~property_drawer_registry();

            property_drawer_registry(const property_drawer_registry &) = delete;
            property_drawer_registry &operator=(const property_drawer_registry &) = delete;

        public:
            /// <summary>
            /// Replaces the drawer of a kind (an empty function restores the built-in one).
            /// </summary>
            void set(core::reflect::field_kind kind, draw_fn drawer);

            /// <summary>
            /// The drawer for a kind (the fallback for COUNT: a read-only JSON text).
            /// </summary>
            [[nodiscard]] const draw_fn &drawer(core::reflect::field_kind kind) const noexcept;

            /// <summary>
            /// Draws a value with the drawer of its kind. Returns true when it changed.
            /// </summary>
            bool draw(
              core::reflect::field_kind kind,
              const property_draw_context &context,
              core::json::document &value
            ) const;

            /// <summary>
            /// Draws a whole reflected object: every field of the description, nested structs as tree nodes.
            /// Returns true when any field changed; 'object' is updated in place.
            /// </summary>
            bool draw_object(const core::reflect::type_info &type, core::json::document &object, bool readOnly) const;

            /// <summary>
            /// Draws an unreflected JSON value member by member (script fields, unknown components).
            /// </summary>
            bool draw_json(std::string_view label, core::json::document &value, bool readOnly) const;

        private:
            void install_defaults();
    };
}
