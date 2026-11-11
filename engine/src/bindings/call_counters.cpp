#include "conjure/bindings/call_counters.h"

#include <algorithm>
#include <array>

using namespace conjure;
using namespace conjure::bindings;

namespace {

    constexpr std::array<native_entry_descriptor, NATIVE_FUNCTION_COUNT + 1> make_descriptors() noexcept {
        std::array<native_entry_descriptor, NATIVE_FUNCTION_COUNT + 1> table = {};
        u32 index = 0;
#define CONJURE_BINDINGS_DESCRIBE(group, name, signature)                                                   \
        table[index] = native_entry_descriptor{                                                             \
          static_cast<native_function_id>(index),                                                           \
          #group,                                                                                           \
          #name,                                                                                            \
          signature,                                                                                        \
          index >= static_cast<u32>(CONJURE_NATIVE_API_CORE_ENTRY_COUNT),                                   \
        };                                                                                                  \
        ++index;
        CONJURE_NATIVE_API_FOR_EACH_ENTRY(CONJURE_BINDINGS_DESCRIBE)
#undef CONJURE_BINDINGS_DESCRIBE
        // The COUNT descriptor: empty names, not owned.
        table[index] = native_entry_descriptor{};
        return table;
    }

    constexpr std::array<native_entry_descriptor, NATIVE_FUNCTION_COUNT + 1> DESCRIPTORS = make_descriptors();

    /// <summary>
    /// The longest "<group>_<name>" of the table, so the name buffers are sized by the .inl and a long line
    /// added there can never write past one.
    /// </summary>
    constexpr usize longest_function_name() noexcept {
        usize longest = 0;
        for (u32 index = 0; index < NATIVE_FUNCTION_COUNT; ++index) {
            const native_entry_descriptor &descriptor = DESCRIPTORS[index];
            longest = std::max(longest, descriptor.group.size() + 1 + descriptor.name.size());
        }
        return longest;
    }

    constexpr usize MAX_FUNCTION_NAME_LENGTH = longest_function_name();
    constexpr usize FUNCTION_NAME_CAPACITY = MAX_FUNCTION_NAME_LENGTH + 1;

    static_assert(MAX_FUNCTION_NAME_LENGTH < 64, "a native entry name is unexpectedly long; check native_api.inl");

    /// <summary>
    /// "<group>_<name>" of every entry, built once (the descriptors hold the two halves).
    /// </summary>
    struct function_names {
        std::array<std::array<char, FUNCTION_NAME_CAPACITY>, NATIVE_FUNCTION_COUNT> storage = {};
        std::array<std::string_view, NATIVE_FUNCTION_COUNT> views = {};

        function_names() noexcept {
            for (u32 index = 0; index < NATIVE_FUNCTION_COUNT; ++index) {
                const native_entry_descriptor &descriptor = DESCRIPTORS[index];
                std::array<char, FUNCTION_NAME_CAPACITY> &buffer = storage[index];
                usize length = 0;
                for (const char character : descriptor.group) {
                    buffer[length] = character;
                    ++length;
                }
                buffer[length] = '_';
                ++length;
                for (const char character : descriptor.name) {
                    buffer[length] = character;
                    ++length;
                }
                views[index] = std::string_view(buffer.data(), length);
            }
        }
    };

    const function_names &names() noexcept {
        static const function_names NAMES = {};
        return NAMES;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Descriptors

std::span<const native_entry_descriptor> conjure::bindings::native_entry_descriptors() noexcept {
    return std::span<const native_entry_descriptor>(DESCRIPTORS.data(), NATIVE_FUNCTION_COUNT);
}

const native_entry_descriptor &conjure::bindings::native_entry_descriptor_of(native_function_id id) noexcept {
    const u32 index = static_cast<u32>(id);
    if (index >= NATIVE_FUNCTION_COUNT) {
        return DESCRIPTORS[NATIVE_FUNCTION_COUNT];
    }
    return DESCRIPTORS[index];
}

std::string_view conjure::bindings::function_name(native_function_id id) noexcept {
    const u32 index = static_cast<u32>(id);
    if (index >= NATIVE_FUNCTION_COUNT) {
        return "";
    }
    return names().views[index];
}

native_function_id conjure::bindings::find_function(std::string_view fullName) noexcept {
    const function_names &table = names();
    for (u32 index = 0; index < NATIVE_FUNCTION_COUNT; ++index) {
        if (table.views[index] == fullName) {
            return static_cast<native_function_id>(index);
        }
    }
    return native_function_id::COUNT;
}


/*===========================================================================*/
/*===========================================================================*/
// Counters

std::array<u32, NATIVE_FUNCTION_COUNT + 1> &conjure::bindings::detail::counter_storage() noexcept {
    static std::array<u32, NATIVE_FUNCTION_COUNT + 1> sCounters = {};
    return sCounters;
}

std::span<const u32> conjure::bindings::call_counters() noexcept {
#if CONJURE_DISTRIBUTION
    return {};
#else
    return std::span<const u32>(detail::counter_storage().data(), NATIVE_FUNCTION_COUNT);
#endif
}

u32 conjure::bindings::call_count(native_function_id id) noexcept {
    const u32 index = static_cast<u32>(id);
    if (index >= NATIVE_FUNCTION_COUNT) {
        return 0;
    }
#if CONJURE_DISTRIBUTION
    return 0;
#else
    return detail::counter_storage()[index];
#endif
}

void conjure::bindings::reset_call_counters() noexcept {
    detail::counter_storage().fill(0);
}
