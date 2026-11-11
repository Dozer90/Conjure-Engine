#pragma once

#include "conjure/core/api.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"
#include "conjure/scripting/native_api.h"

#include <array>
#include <span>
#include <string_view>

// Call counters of the native table (docs/design/15_bindings.md 2.7, 3.5, B6). Every entry of
// native_api.inl, core and bindings alike, has an index (native_function_id, its .inl line order, the same
// value as the C# NativeFunction enum and the manifest's "index") and a descriptor. When
// !CONJURE_DISTRIBUTION the subsystem replaces every filled slot with counting_thunk<Id, Fn>::invoke, a
// forwarding function that increments sCounters[Id] and calls the original; in Distribution the raw
// pointers stay and call_counters() is empty. Counters are plain u32 because every entry is main-thread.

namespace conjure::bindings {

    /// <summary>
    /// Index of every entry of the table (one enumerator per CONJURE_NATIVE_FN line, in table order).
    /// </summary>
    enum class native_function_id : u16 {
#define CONJURE_BINDINGS_FUNCTION_ID(group, name, signature) group##_##name,
        CONJURE_NATIVE_API_FOR_EACH_ENTRY(CONJURE_BINDINGS_FUNCTION_ID)
#undef CONJURE_BINDINGS_FUNCTION_ID
        COUNT
    };

    /// <summary>
    /// Entries in the table (core and bindings together).
    /// </summary>
    inline constexpr u32 NATIVE_FUNCTION_COUNT = static_cast<u32>(native_function_id::COUNT);

    static_assert(
      NATIVE_FUNCTION_COUNT == CONJURE_NATIVE_API_ENTRY_COUNT,
      "native_api_hash.h is stale: run tools/generate_native_api.py"
    );

    /// <summary>
    /// One row of the table: its id, group and name, the normalised "<return> <arguments>" signature of the
    /// generator and whether the bindings module owns the entry (R26/R64).
    /// </summary>
    struct native_entry_descriptor {
        native_function_id id = native_function_id::COUNT;
        std::string_view group = {};
        std::string_view name = {};
        std::string_view signature = {};
        bool ownedByBindings = false;
    };

    /// <summary>
    /// Every entry in table order.
    /// </summary>
    [[nodiscard]] CONJURE_API std::span<const native_entry_descriptor> native_entry_descriptors() noexcept;

    /// <summary>
    /// The descriptor of an id; the COUNT descriptor (empty names) for an out-of-range id.
    /// </summary>
    [[nodiscard]] CONJURE_API const native_entry_descriptor &native_entry_descriptor_of(native_function_id id) noexcept;

    /// <summary>
    /// "transform_set_local": the C name of an entry; "" for an out-of-range id.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view function_name(native_function_id id) noexcept;

    /// <summary>
    /// The id of the entry named "<group>_<name>", or COUNT.
    /// </summary>
    [[nodiscard]] CONJURE_API native_function_id find_function(std::string_view fullName) noexcept;

    /// <summary>
    /// True when the entry belongs to one of the ten bindings groups (everything after the R26 marker).
    /// </summary>
    [[nodiscard]] constexpr bool is_bindings_owned(native_function_id id) noexcept {
        const u32 index = static_cast<u32>(id);
        return index >= static_cast<u32>(CONJURE_NATIVE_API_CORE_ENTRY_COUNT) && index < NATIVE_FUNCTION_COUNT;
    }

    /// <summary>
    /// The counters, NATIVE_FUNCTION_COUNT entries indexed by native_function_id; empty in Distribution.
    /// </summary>
    [[nodiscard]] CONJURE_API std::span<const u32> call_counters() noexcept;

    /// <summary>
    /// Calls recorded for one entry since the last reset; 0 in Distribution and for out-of-range ids.
    /// </summary>
    [[nodiscard]] CONJURE_API u32 call_count(native_function_id id) noexcept;

    /// <summary>
    /// Zeroes every counter (headless fixtures call it between cases).
    /// </summary>
    CONJURE_API void reset_call_counters() noexcept;

    namespace detail {
        /// <summary>
        /// The counter storage the thunks increment (one slot per entry plus one spare for COUNT so the
        /// index never needs a check).
        /// </summary>
        [[nodiscard]] CONJURE_API std::array<u32, NATIVE_FUNCTION_COUNT + 1> &counter_storage() noexcept;
    }

    /// <summary>
    /// The forwarding function installed over an entry's slot (B6): one unique instantiation per entry,
    /// increments the entry's counter and calls the original pointer kept in sTarget. Specialised on the
    /// function type Ret(Args...) that the .inl's return and argument lists form.
    /// </summary>
    template <native_function_id Id, typename Fn>
    struct counting_thunk;

    template <native_function_id Id, typename Ret, typename... Args>
    struct counting_thunk<Id, Ret(Args...)> {
        using function_pointer = Ret (*)(Args...);

        /// <summary>
        /// The slot's original pointer (process-wide: one native table per process, like script_runtime).
        /// </summary>
        static inline function_pointer sTarget = nullptr;

        /// <summary>
        /// Increments the counter and forwards every argument by value.
        /// </summary>
        static Ret invoke(Args... args) {
            ++detail::counter_storage()[static_cast<usize>(Id)];
            return sTarget(args...);
        }
    };
}
