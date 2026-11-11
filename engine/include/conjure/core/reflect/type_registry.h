#pragma once

#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/reflect/type_info.h"

#include "conjure/core/api.h"
#include "conjure/core/event.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <atomic>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>

// The type registry.
//
// A registry owns the descriptions of every registered struct and enum and hands them out by stable
// pointer. The process-wide instance is registry(); type_info_of<T>() fills it lazily from the
// CONJURE_REFLECT descriptions, modules add dynamic types with register_type() and remove them on
// unload with unregister_type(). Tests may construct their own registries.
//
// Threading: register/unregister take a mutex and are safe from any thread. find()/find_enum() are
// lock-free: descriptions never move and the id index is an append-only atomic table, so lookups may
// run concurrently with registrations. types()/enums() return views of the live lists that are
// invalidated by the next registration or unregistration; call them when registrations are quiescent
// or copy the span under external synchronisation. on_registered()/on_unregistered() handlers run on
// the registering thread, serialised by the registry; subscribe to them while no registration is in
// flight (typically at startup on the main thread). A handler may itself register types.
//
// Event ordering: struct registrations and unregistrations are emitted in the order their mutations
// took effect, because the event lock is taken before the registry lock and held until the handlers
// have run. A listener therefore always sees on_registered for a description before any
// on_unregistered for it, even when another thread removes a just-inserted type. The lock order is
// event lock -> registry lock everywhere; handlers re-entering register_type() on the same thread
// re-acquire the (recursive) event lock and then the free registry lock, so they never deadlock.

namespace conjure::core::reflect {

    /// <summary>
    /// Registered struct and enum descriptions with stable addresses and lock-free lookup.
    /// </summary>
    class CONJURE_API type_registry {
        private:
            struct state;
            std::unique_ptr<state> mState = nullptr;   // storage, indexes, mutexes and events

        public:
            type_registry();
            ~type_registry();

            type_registry(const type_registry &) = delete;
            type_registry &operator=(const type_registry &) = delete;
            type_registry(type_registry &&) = delete;
            type_registry &operator=(type_registry &&) = delete;

        public:
            /// <summary>
            /// Registers a struct description after validating it (see validate_layout). A null id
            /// is derived from the name; an id that does not match the name is INVALID_ARGUMENT, and
            /// so is a STRUCT or ENUM field whose description pointer is not a live description of
            /// this registry (a stack-local or foreign type_info would dangle). Registering the same
            /// id again returns the existing description when the layout is identical and fails
            /// with ALREADY_EXISTS otherwise. New registrations emit on_registered() on the calling
            /// thread after the registry lock is released (see the event ordering note above).
            /// </summary>
            [[nodiscard]] result<const type_info *> register_type(type_info info);

            /// <summary>
            /// Registers an enumeration description: non-empty name, underlying size 1, 2, 4 or 8,
            /// unique non-empty enumerator names (INVALID_ARGUMENT otherwise). The same id with an
            /// equal description returns the existing one; a different description is ALREADY_EXISTS.
            /// </summary>
            [[nodiscard]] result<const enum_info *> register_enum(enum_info info);

            /// <summary>
            /// The registered struct with this id, or nullptr. Lock-free.
            /// </summary>
            [[nodiscard]] const type_info *find(type_id id) const noexcept;

            /// <summary>
            /// The registered struct with this exact name, or nullptr. Lock-free.
            /// </summary>
            [[nodiscard]] const type_info *find(std::string_view name) const noexcept;

            /// <summary>
            /// The registered enumeration with this id, or nullptr. Lock-free.
            /// </summary>
            [[nodiscard]] const enum_info *find_enum(type_id id) const noexcept;

            /// <summary>
            /// The registered enumeration with this exact name, or nullptr. Lock-free.
            /// </summary>
            [[nodiscard]] const enum_info *find_enum(std::string_view name) const noexcept;

            /// <summary>
            /// Every live struct in registration order. The view is invalidated by the next
            /// registration or unregistration.
            /// </summary>
            [[nodiscard]] std::span<const type_info *const> types() const noexcept;

            /// <summary>
            /// Every live enumeration in registration order. Same validity as types().
            /// </summary>
            [[nodiscard]] std::span<const enum_info *const> enums() const noexcept;

            /// <summary>
            /// Number of live struct descriptions.
            /// </summary>
            [[nodiscard]] usize type_count() const noexcept;

            /// <summary>
            /// Number of live enumeration descriptions.
            /// </summary>
            [[nodiscard]] usize enum_count() const noexcept;

            /// <summary>
            /// Emitted once per new struct registration, on the registering thread.
            /// </summary>
            [[nodiscard]] event<const type_info &> &on_registered() noexcept;

            /// <summary>
            /// Emitted once per successful unregister_type(), on the calling thread. The description
            /// stays valid for the duration of the emit.
            /// </summary>
            [[nodiscard]] event<const type_info &> &on_unregistered() noexcept;

            /// <summary>
            /// Removes a struct (module unload). NOT_FOUND when unknown; INVALID_STATE when a live
            /// struct has a STRUCT field (or array of STRUCT) of this type. The description's memory
            /// stays allocated until the registry is destroyed so lock-free lookups racing the
            /// removal never dereference freed memory.
            /// </summary>
            [[nodiscard]] result<void> unregister_type(type_id id);

            /// <summary>
            /// Removes an enumeration. NOT_FOUND when unknown; INVALID_STATE when a live struct has
            /// an ENUM field (or array of ENUM) of this type.
            /// </summary>
            [[nodiscard]] result<void> unregister_enum(type_id id);
    };

    /// <summary>
    /// The process-wide registry that type_info_of() and enum_info_of() fill.
    /// </summary>
    [[nodiscard]] CONJURE_API type_registry &registry() noexcept;

    namespace detail {
        /// <summary>
        /// Registers a macro-built struct description in registry(); logs an ERROR and returns
        /// nullptr when the registry rejects it.
        /// </summary>
        [[nodiscard]] CONJURE_API const type_info *register_described_type(type_info info) noexcept;

        /// <summary>
        /// Registers a macro-built enumeration description in registry(); logs an ERROR and returns
        /// nullptr when the registry rejects it.
        /// </summary>
        [[nodiscard]] CONJURE_API const enum_info *register_described_enum(enum_info info) noexcept;
    }

    /// <summary>
    /// The registered description of a CONJURE_REFLECT_STRUCT type, registering it in registry() on
    /// first use (thread-safe). nullptr for types without a description and when registration
    /// failed (a different type already owns the same name). Neither a failure nor a removal is
    /// cached: the cached pointer is re-validated against registry() on every call (one lock-free
    /// lookup), so the type registers again once the conflicting type is gone, and after
    /// registry().unregister_type() (module unload) the next call returns the fresh registration.
    /// </summary>
    template <typename T>
    [[nodiscard]] const type_info *type_info_of() noexcept {
        using plain_type = std::remove_cvref_t<T>;
        if constexpr (reflected_struct<plain_type>) {
            static std::atomic<const type_info *> sInfo = nullptr;
            const type_info *cached = sInfo.load(std::memory_order_acquire);
            if (cached != nullptr && registry().find(type_id_of<plain_type>()) == cached) {
                return cached;
            }

            // register_type() returns the existing description for an identical layout, so a
            // concurrent first call or a stale cache costs one idempotent registration.
            const type_info *registered = detail::register_described_type(describe_struct<plain_type>());
            sInfo.store(registered, std::memory_order_release);
            return registered;
        } else {
            return nullptr;
        }
    }

    /// <summary>
    /// The registered description of a CONJURE_REFLECT_ENUM type, registering it on first use.
    /// nullptr for enumerations without a description and when registration failed. Like
    /// type_info_of(), failures and removals are not cached.
    /// </summary>
    template <typename T>
    [[nodiscard]] const enum_info *enum_info_of() noexcept {
        using plain_type = std::remove_cvref_t<T>;
        if constexpr (reflected_enum<plain_type>) {
            static std::atomic<const enum_info *> sInfo = nullptr;
            const enum_info *cached = sInfo.load(std::memory_order_acquire);
            if (cached != nullptr && registry().find_enum(type_id_of<plain_type>()) == cached) {
                return cached;
            }

            const enum_info *registered = detail::register_described_enum(describe_enum<plain_type>());
            sInfo.store(registered, std::memory_order_release);
            return registered;
        } else {
            return nullptr;
        }
    }
}
