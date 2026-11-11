#pragma once

#include "conjure/core/api.h"
#include "conjure/core/hash.h"
#include "conjure/core/types.h"

#include <compare>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>

// Hashed string identifiers.
//
//   constexpr string_id POSITION("ecs.position");            // hashed at compile time, not interned
//   static const string_id ROTATION = string_id::intern("ecs.rotation");   // static: interned
//   const string_id name(nameFromFile);                      // hashed and interned at runtime
//   using namespace conjure::core::literals;
//   if (id == "ecs.position"_sid) { ... }                    // compile-time hash, never locks
//
// A string_id is the 64-bit FNV-1a hash of its name (hash.h). Comparison, hashing and storage
// never touch the name; the name is kept in a process-wide, mutex-guarded table so tools (the
// editor, module loaders, log output) can show it again.
//
// Interning rules. The constructor interns only when it is *not* constant-evaluated, and the
// language decides that, not the spelling at the call site:
//   - Variables with static storage duration whose initialiser is a constant expression are
//     constant-initialised, so `static const string_id ID("x");`, `static const auto ID = "x"_sid;`
//     and namespace-scope ids are NEVER interned. Use `string_id::intern("x")` for those.
//   - A `const` integral or enumeration local whose initialiser is a constant expression
//     (`const bool same = id == string_id("x");`) is trial-constant-evaluated by GCC and Clang, so
//     whether it interns depends on the compiler. Do not rely on interning from such initialisers.
//   - Constructing from a runtime value (a std::string, a view read from a file, a function
//     argument) always interns.
//   - `"x"_sid` is consteval: always hashed at compile time, never interned and never takes the
//     table mutex, so it is safe in hot loops. Only ids that someone has interned (or constructed
//     at runtime) resolve through view()/c_str()/to_string(); others print as `#<hex>`.

namespace conjure::core {

    namespace detail {
        /// <summary>
        /// Records 'name' as the spelling of 'hash'. Interning a different name under a hash that
        /// is already registered is a collision: it asserts, logs an ERROR on LOG_CORE and keeps
        /// the first name. Thread-safe. Exposed so tests and tooling can exercise the table directly.
        /// </summary>
        CONJURE_API void register_string_id_name(u64 hash, std::string_view name);

        /// <summary>
        /// The registered spelling of 'hash', or an empty, null-terminated view when unknown.
        /// The returned view stays valid for the lifetime of the process. Thread-safe.
        /// </summary>
        [[nodiscard]] CONJURE_API std::string_view find_string_id_name(u64 hash) noexcept;

        /// <summary>
        /// Number of distinct hashes with a registered name. Thread-safe.
        /// </summary>
        [[nodiscard]] CONJURE_API usize string_id_name_count() noexcept;
    }

    /// <summary>
    /// A string reduced to its 64-bit FNV-1a hash. Trivially copyable and comparable by hash;
    /// the original spelling is available through view()/c_str() once the id has been interned.
    /// </summary>
    class CONJURE_API string_id {
        private:
            u64 mHash = 0;

        public:
            /// <summary>
            /// The null id: hash 0, no name.
            /// </summary>
            constexpr string_id() noexcept = default;

            /// <summary>
            /// Hashes 'name'. When constant-evaluated (constexpr variables, static or namespace-
            /// scope ids with a literal name) the id is only hashed; otherwise the name is also
            /// interned so view()/c_str() can resolve it later. See the header comment for the
            /// exact rules; use intern() where interning must be guaranteed.
            /// </summary>
            constexpr explicit string_id(std::string_view name) :
                mHash(hash::fnv1a64(name)) {
                if (!std::is_constant_evaluated()) {
                    detail::register_string_id_name(mHash, name);
                }
            }

        public:
            /// <summary>
            /// The null id. Equivalent to string_id().
            /// </summary>
            [[nodiscard]] static constexpr string_id null() noexcept {
                return string_id();
            }

            /// <summary>
            /// Wraps an already computed hash (deserialisation, C# interop). Nothing is interned.
            /// </summary>
            [[nodiscard]] static constexpr string_id from_hash(u64 hash) noexcept {
                string_id id = {};
                id.mHash = hash;
                return id;
            }

            /// <summary>
            /// Hashes and interns 'name' in every context, including for ids that were first
            /// created in constant expressions. Never constexpr, so it is the right initialiser
            /// for static and namespace-scope ids whose names must be visible to tools.
            /// </summary>
            [[nodiscard]] static string_id intern(std::string_view name);

            /// <summary>
            /// Number of names currently interned in the process-wide table.
            /// </summary>
            [[nodiscard]] static usize interned_count() noexcept {
                return detail::string_id_name_count();
            }

            /// <summary>
            /// The 64-bit FNV-1a hash of the name. 0 for the null id.
            /// </summary>
            [[nodiscard]] constexpr u64 hash() const noexcept {
                return mHash;
            }

            /// <summary>
            /// True for the default-constructed id (hash 0).
            /// </summary>
            [[nodiscard]] constexpr bool is_null() const noexcept {
                return mHash == 0;
            }

            /// <summary>
            /// The interned name, or an empty view when this id was never interned.
            /// </summary>
            [[nodiscard]] std::string_view view() const noexcept {
                return detail::find_string_id_name(mHash);
            }

            /// <summary>
            /// The interned name as a null-terminated string, or "" when unknown.
            /// </summary>
            [[nodiscard]] const char *c_str() const noexcept {
                return detail::find_string_id_name(mHash).data();
            }

            /// <summary>
            /// The interned name when known, otherwise "#" followed by the 16-digit hex hash.
            /// </summary>
            [[nodiscard]] std::string to_string() const;

            [[nodiscard]] constexpr auto operator<=>(const string_id &) const noexcept = default;
            [[nodiscard]] constexpr bool operator==(const string_id &) const noexcept = default;
    };

    static_assert(sizeof(string_id) == sizeof(u64), "string_id must be exactly its hash");
    static_assert(std::is_trivially_copyable_v<string_id>, "string_id must be trivially copyable");

    inline namespace literals {
        /// <summary>
        /// "ecs.component"_sid: a string_id from a literal. Always evaluated at compile time, so
        /// it never interns the name and never takes the table mutex; intern() the name once
        /// elsewhere if tools must be able to resolve it.
        /// </summary>
        [[nodiscard]] consteval string_id operator""_sid(const char *text, usize length) {
            return string_id(std::string_view(text, length));
        }
    }
}

template <>
struct std::hash<conjure::core::string_id> {
    [[nodiscard]] std::size_t operator()(const conjure::core::string_id &value) const noexcept {
        // u64 and size_t are the same width on every supported (64-bit only) target.
        return value.hash();
    }
};

template <>
struct std::formatter<conjure::core::string_id> : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const conjure::core::string_id &value, FormatContext &context) const {
        return std::formatter<std::string>::format(value.to_string(), context);
    }
};
