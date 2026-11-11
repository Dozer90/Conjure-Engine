#pragma once

#include "conjure/scripting/native_api.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The native copy of the script class schema (docs/design/03_scripting.md 2.2, 3.7). Managed reports
// every ScriptBehaviour subclass and its serialised fields through script_register_class/field into the
// staging half while an assembly loads; commit_staging() swaps it in once the load succeeded, so a failed
// load never disturbs the active schema the inspector and the blob codecs read.

namespace conjure::scripting {

    struct script_field_info {
        u64 hash = 0;                                                   // fnv1a64 of the C# field name
        std::string name = {};
        script_field_kind kind = script_field_kind::INT32;
        script_field_kind elementKind = script_field_kind::INT32;       // LIST only
        core::json::document defaultValue = {};
        core::json::document meta = {};                 // tooltip, min, max, enumNames, assetType, hidden
    };

    struct CONJURE_API script_class_info {
        u64 hash = 0;                                                   // fnv1a64 of the full C# type name
        std::string fullName = {};
        u16 callbackMask = 0;                                           // bits of script_callback
        std::vector<script_field_info> fields = {};                     // sorted by hash

        /// <summary>
        /// The field with this hash, or nullptr (binary search; fields must be sorted).
        /// </summary>
        [[nodiscard]] const script_field_info *find_field(u64 fieldHash) const noexcept;

        /// <summary>
        /// The field with this name, or nullptr.
        /// </summary>
        [[nodiscard]] const script_field_info *find_field(std::string_view fieldName) const noexcept;

        /// <summary>
        /// True when the class overrides the callback.
        /// </summary>
        [[nodiscard]] bool has_callback(script_callback callback) const noexcept {
            return (callbackMask & callback_bit(callback)) != 0;
        }

        /// <summary>
        /// Sorts the fields by hash (what find_field needs).
        /// </summary>
        void sort_fields();
    };

    /// <summary>
    /// Hash of a C# type or field name as the managed side computes it (core::hash::fnv1a64 of UTF-8).
    /// </summary>
    [[nodiscard]] CONJURE_API u64 script_name_hash(std::string_view fullName) noexcept;

    class CONJURE_API script_class_registry {
        private:
            struct table {
                std::vector<script_class_info> classes = {};
                std::unordered_map<u64, u32> indexByHash = {};
            };

            table mActive = {};
            table mStaging = {};
            bool mStagingOpen = false;

        public:
            script_class_registry();
            ~script_class_registry();

            script_class_registry(const script_class_registry &) = delete;
            script_class_registry &operator=(const script_class_registry &) = delete;
            script_class_registry(script_class_registry &&) noexcept = default;
            script_class_registry &operator=(script_class_registry &&) noexcept = default;

        public:
            /// <summary>
            /// Empties the staging half and opens it for stage_class/stage_field.
            /// </summary>
            void begin_staging();

            /// <summary>
            /// Adds a class to the staging half. ALREADY_EXISTS for a hash already staged, INVALID_STATE when
            /// no staging is open.
            /// </summary>
            [[nodiscard]] core::result<void> stage_class(script_class_info info);

            /// <summary>
            /// Adds a field to a staged class. CLASS_NOT_FOUND, ALREADY_EXISTS for a duplicate field hash.
            /// </summary>
            [[nodiscard]] core::result<void> stage_field(u64 classHash, script_field_info field);

            /// <summary>
            /// Sorts every staged class' fields, makes the staging half active and closes staging.
            /// </summary>
            void commit_staging();

            /// <summary>
            /// Drops the staging half; the active schema is untouched.
            /// </summary>
            void discard_staging();

            [[nodiscard]] bool is_staging_open() const noexcept {
                return mStagingOpen;
            }

            /// <summary>
            /// The active class with this hash, or nullptr.
            /// </summary>
            [[nodiscard]] const script_class_info *find(u64 classHash) const noexcept;

            /// <summary>
            /// The active class with this full name, or nullptr.
            /// </summary>
            [[nodiscard]] const script_class_info *find(std::string_view fullName) const noexcept;

            /// <summary>
            /// The staged class with this hash, or nullptr.
            /// </summary>
            [[nodiscard]] const script_class_info *find_staged(u64 classHash) const noexcept;

            /// <summary>
            /// Dense index of an active class (the order of classes()), or UINT32_MAX.
            /// </summary>
            [[nodiscard]] u32 index_of(u64 classHash) const noexcept;

            /// <summary>
            /// Active classes in registration order.
            /// </summary>
            [[nodiscard]] std::span<const script_class_info> classes() const noexcept {
                return mActive.classes;
            }

            [[nodiscard]] std::span<const script_class_info> staged_classes() const noexcept {
                return mStaging.classes;
            }

            [[nodiscard]] usize size() const noexcept {
                return mActive.classes.size();
            }

            /// <summary>
            /// Empties both halves.
            /// </summary>
            void clear();

        private:
            [[nodiscard]] static const script_class_info *find_in(const table &source, u64 classHash) noexcept;
    };
}
