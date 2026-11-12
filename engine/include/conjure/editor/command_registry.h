#pragma once

#include "conjure/editor/editor_command.h"
#include "conjure/editor/schema_validator.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

// The command catalogue (docs/design/04_editor.md D2, D15, 4.3): descriptors by name with their compiled
// argument schemas, the self-description editor.schema returns and the hash session.hello reports. Main
// thread only; module.load adds commands at runtime and module.unload removes them by owner.

namespace conjure::editor {

    class CONJURE_API command_registry {
        private:
            struct entry {
                command_descriptor descriptor = {};
                compiled_schema argsSchema = {};
                compiled_schema resultSchema = {};
            };

            std::vector<entry> mEntries = {};               // sorted by name
            mutable u64 mSchemaHash = 0;                    // valid while !mSchemaHashStale
            mutable bool mSchemaHashStale = true;           // set by every change, cleared by schema_hash()

        public:
            command_registry();
            ~command_registry();

            command_registry(const command_registry &) = delete;
            command_registry &operator=(const command_registry &) = delete;

        public:
            /// <summary>
            /// Registers a command. INVALID_ARGS when the name is empty or taken, the handler is empty, or a
            /// schema does not compile.
            /// </summary>
            [[nodiscard]] core::result<void> add(command_descriptor descriptor);

            /// <summary>
            /// Removes one command. False when unknown.
            /// </summary>
            bool remove(std::string_view name);

            /// <summary>
            /// Removes every command whose owner matches and returns how many.
            /// </summary>
            usize remove_owned_by(std::string_view owner);

            /// <summary>
            /// The descriptor, or nullptr.
            /// </summary>
            [[nodiscard]] const command_descriptor *find(std::string_view name) const noexcept;

            /// <summary>
            /// The compiled argument schema of a command, or nullptr.
            /// </summary>
            [[nodiscard]] const compiled_schema *args_schema(std::string_view name) const noexcept;

            /// <summary>
            /// Every descriptor in name order (the span is invalidated by add/remove).
            /// </summary>
            [[nodiscard]] std::vector<const command_descriptor *> descriptors() const;

            /// <summary>
            /// Every name in order.
            /// </summary>
            [[nodiscard]] std::vector<std::string> names() const;

            /// <summary>
            /// The names closest to 'name' by Levenshtein distance (ties by name), at most 'count'. Empty for a
            /// name longer than 128 bytes: no command is spelt like that, and the distance would cost its length
            /// times every command's.
            /// </summary>
            [[nodiscard]] std::vector<std::string> closest_names(std::string_view name, usize count) const;

            /// <summary>
            /// One command as editor.schema describes it: {name, summary, flags:[...], owner, args, result}.
            /// </summary>
            [[nodiscard]] core::json::document describe(const command_descriptor &descriptor) const;

            /// <summary>
            /// The whole catalogue: {protocolVersion, schemaHash, commands:[...], events:[...], errorCodes:[...]}.
            /// UI_ONLY commands are included only when asked.
            /// </summary>
            [[nodiscard]] core::json::document describe_all(bool includeUiOnly) const;

            /// <summary>
            /// FNV-1a 64 of the canonical catalogue text (UI_ONLY excluded): identical for identical catalogues,
            /// different when any name, schema or flag changes. Computed on the first call after a change, so
            /// registering the whole catalogue command by command stays linear.
            /// </summary>
            [[nodiscard]] u64 schema_hash() const;

            [[nodiscard]] usize size() const noexcept {
                return mEntries.size();
            }

        private:
            [[nodiscard]] const entry *find_entry(std::string_view name) const noexcept;
            void recompute_hash() const;
    };

    /// <summary>
    /// The event names the broadcaster emits, in a fixed order (part of editor.schema).
    /// </summary>
    [[nodiscard]] CONJURE_API std::span<const std::string_view> protocol_event_names() noexcept;
}
