#pragma once

#include "conjure/modules/module_abi.h"
#include "conjure/modules/module_types.h"

#include <cstdint>
#include <type_traits>

// Declaring a module (docs/design/07_modules.md 4.1).
//
// Shared game module (CONJURE_MODULE_NAME is defined by conjure_add_game_module()):
//
//   conjure::modules::module_entry_status my_entry(conjure::modules::module_context &context) { ... }
//   CONJURE_DECLARE_MODULE(my_entry)
//
// Built-in or test module compiled into the host, added with module_manager::add_static_module():
//
//   CONJURE_DECLARE_STATIC_MODULE(conjure_ecs, R"({"schema":1,"name":"conjure.ecs", ...})", nullptr)
//   manager.add_static_module(conjure_static_module_conjure_ecs());
//
// A static module's entry may be nullptr (manifest-only module, R18).

namespace conjure::modules {

    class module_context;

    /// <summary>
    /// The entry signature module authors write; the C thunk adapts it to conjure_module_entry_fn.
    /// </summary>
    using module_entry_function = module_entry_status (*)(module_context &context);

    /// <summary>
    /// A module compiled into the host: its manifest as JSON text and its entry (or nullptr).
    /// The manager keeps a pointer to the descriptor, so it must outlive the manager; the macro
    /// below returns a function-local static.
    /// </summary>
    struct static_module_desc {
        const char *identifier = "";
        const char *manifestJson = "";
        conjure_module_entry_fn entry = nullptr;
    };

    namespace detail {
        /// <summary>
        /// Adapts a C++ entry function to the C ABI.
        /// </summary>
        template <auto EntryFunction>
        std::uint32_t entry_thunk(module_context *context) {
            static_assert(
              std::is_same_v<decltype(EntryFunction), module_entry_function>,
              "a module entry has the signature module_entry_status fn(module_context &)"
            );
            return static_cast<std::uint32_t>(EntryFunction(*context));
        }

        /// <summary>
        /// The C entry for a C++ entry function, or nullptr for a manifest-only module.
        /// </summary>
        template <auto EntryFunction>
        [[nodiscard]] constexpr conjure_module_entry_fn static_entry() noexcept {
            if constexpr (std::is_same_v<decltype(EntryFunction), std::nullptr_t>) {
                return nullptr;
            } else {
                return &entry_thunk<EntryFunction>;
            }
        }
    }
}

// Shared game module: exports conjure_module_describe and conjure_module_entry. EntryFunction is
// `conjure::modules::module_entry_status fn(conjure::modules::module_context &)`.
#define CONJURE_DECLARE_MODULE(EntryFunction)                                                               \
    extern "C" CONJURE_MODULE_EXPORT const conjure_module_info *conjure_module_describe(void) {             \
        static constexpr conjure_module_info INFO = ::conjure::modules::make_module_info(CONJURE_MODULE_NAME); \
        return &INFO;                                                                                       \
    }                                                                                                       \
    extern "C" CONJURE_MODULE_EXPORT std::uint32_t conjure_module_entry(                                    \
      ::conjure::modules::module_context *context                                                           \
    ) {                                                                                                     \
        return static_cast<std::uint32_t>(EntryFunction(*context));                                         \
    }

// Built-in library: defines `const static_module_desc &conjure_static_module_<Identifier>()`.
// EntryFunction may be nullptr for a manifest-only module.
#define CONJURE_DECLARE_STATIC_MODULE(Identifier, ManifestJsonText, EntryFunction)                          \
    const ::conjure::modules::static_module_desc &conjure_static_module_##Identifier() {                    \
        static constexpr ::conjure::modules::static_module_desc DESC = {                                    \
            #Identifier,                                                                                    \
            ManifestJsonText,                                                                               \
            ::conjure::modules::detail::static_entry<EntryFunction>(),                                      \
        };                                                                                                  \
        return DESC;                                                                                        \
    }
