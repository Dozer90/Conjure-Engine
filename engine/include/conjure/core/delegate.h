#pragma once

#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

// Single-target callbacks without allocation.
//
//   delegate<void(int)> onHit = delegate<void(int)>::bind<&free_function>();
//   onHit = delegate<void(int)>::bind<&player::take_damage>(&thePlayer);
//   onHit = delegate<void(int)>::bind([&total](int amount) { total += amount; });
//   onHit(10);
//
// A delegate is two pointers of inline storage plus the invoker. It never owns anything: member
// bindings store the instance pointer and lambdas must be trivially copyable and fit the storage
// (one or two captured pointers). Larger state belongs in std::function or an event.

namespace conjure::core {

    template <typename Signature>
    class delegate;

    template <typename Return, typename... Args>
    class delegate<Return(Args...)> {
        public:
            /// <summary>
            /// Bytes of inline storage for the bound target.
            /// </summary>
            static constexpr usize STORAGE_BYTES = 2 * sizeof(void *);

            /// <summary>
            /// True when a callable type can be bound: trivially copyable, pointer-aligned, fits the
            /// inline storage and is invocable as const with the delegate's signature.
            /// </summary>
            template <typename Callable>
            static constexpr bool CAN_BIND = sizeof(Callable) <= STORAGE_BYTES
                && alignof(Callable) <= alignof(void *)
                && std::is_trivially_copyable_v<Callable>
                && std::is_trivially_destructible_v<Callable>
                && std::is_invocable_r_v<Return, const Callable &, Args...>;

        private:
            using invoker_type = Return (*)(const void *storage, Args...);
            using function_pointer = Return (*)(Args...);

            alignas(void *) std::array<std::byte, STORAGE_BYTES> mStorage = {};
            invoker_type mInvoke = nullptr;

        public:
            constexpr delegate() noexcept = default;

        public:
            /// <summary>
            /// Binds a free function known at compile time.
            /// </summary>
            template <auto Function>
                requires std::is_pointer_v<decltype(Function)>
                    && std::is_function_v<std::remove_pointer_t<decltype(Function)>>
                    && std::is_invocable_r_v<Return, decltype(Function), Args...>
            [[nodiscard]] static delegate bind() noexcept {
                delegate result = {};
                result.mInvoke = &invoke_static<Function>;
                return result;
            }

            /// <summary>
            /// Binds a member function to an instance: delegate&lt;void(int)&gt;::bind&lt;&foo::bar&gt;(&foo).
            /// Const member functions accept a pointer to const. The instance is not owned.
            /// </summary>
            template <auto Method, typename Class>
                requires std::is_member_function_pointer_v<decltype(Method)>
                    && std::is_invocable_r_v<Return, decltype(Method), Class *, Args...>
            [[nodiscard]] static delegate bind(Class *instance) noexcept {
                CONJURE_ASSERT(instance != nullptr, "delegate::bind requires an instance");
                if (instance == nullptr) {
                    return delegate();
                }

                delegate result = {};
                std::memcpy(result.mStorage.data(), &instance, sizeof(instance));
                result.mInvoke = &invoke_member<Method, Class>;
                return result;
            }

            /// <summary>
            /// Binds a function pointer chosen at runtime. A null pointer yields an unbound delegate.
            /// </summary>
            [[nodiscard]] static delegate bind(function_pointer function) noexcept {
                if (function == nullptr) {
                    return delegate();
                }

                delegate result = {};
                std::memcpy(result.mStorage.data(), &function, sizeof(function));
                result.mInvoke = &invoke_function_pointer;
                return result;
            }

            /// <summary>
            /// Binds a stateless or small trivially copyable callable (see CAN_BIND) by copying it
            /// into the inline storage.
            /// </summary>
            template <typename Callable>
                requires (!std::is_same_v<std::remove_cvref_t<Callable>, delegate>)
                    && (!std::is_same_v<std::remove_cvref_t<Callable>, function_pointer>)
            [[nodiscard]] static delegate bind(Callable callable) noexcept {
                static_assert(
                  CAN_BIND<Callable>,
                  "delegate binds only trivially copyable callables of at most two pointers; use std::function"
                );

                delegate result = {};
                if constexpr (!std::is_empty_v<Callable>) {
                    std::memcpy(result.mStorage.data(), &callable, sizeof(callable));
                } else {
                    // An empty callable has no value representation: its one byte is
                    // indeterminate and copying it would make equal bindings compare unequal.
                    static_cast<void>(callable);
                }
                result.mInvoke = &invoke_callable<Callable>;
                return result;
            }

            /// <summary>
            /// Invokes the bound target. Calling an unbound delegate asserts and returns a
            /// default-constructed value (or nothing for void).
            /// </summary>
            Return operator()(Args... args) const {
                if (mInvoke == nullptr) [[unlikely]] {
                    CONJURE_ASSERT(false, "delegate invoked while unbound");
                    if constexpr (std::is_void_v<Return>) {
                        return;
                    } else if constexpr (std::is_default_constructible_v<Return>) {
                        return Return{};
                    } else {
                        std::abort();
                    }
                }
                return mInvoke(mStorage.data(), std::forward<Args>(args)...);
            }

            /// <summary>
            /// True when a target is bound.
            /// </summary>
            [[nodiscard]] constexpr bool is_bound() const noexcept {
                return mInvoke != nullptr;
            }

            /// <summary>
            /// Same as is_bound().
            /// </summary>
            [[nodiscard]] constexpr explicit operator bool() const noexcept {
                return is_bound();
            }

            /// <summary>
            /// Unbinds the delegate.
            /// </summary>
            constexpr void reset() noexcept {
                mStorage = {};
                mInvoke = nullptr;
            }

            /// <summary>
            /// Two delegates are equal when they invoke the same target on the same instance (or the
            /// same captured state). Unbound delegates are equal to each other.
            /// </summary>
            [[nodiscard]] bool operator==(const delegate &other) const noexcept {
                return mInvoke == other.mInvoke && mStorage == other.mStorage;
            }

        private:
            template <auto Function>
            static Return invoke_static([[maybe_unused]] const void *storage, Args... args) {
                if constexpr (std::is_void_v<Return>) {
                    static_cast<void>(std::invoke(Function, std::forward<Args>(args)...));
                } else {
                    return std::invoke(Function, std::forward<Args>(args)...);
                }
            }

            template <auto Method, typename Class>
            static Return invoke_member(const void *storage, Args... args) {
                Class *instance = nullptr;
                std::memcpy(&instance, storage, sizeof(instance));
                if constexpr (std::is_void_v<Return>) {
                    static_cast<void>(std::invoke(Method, instance, std::forward<Args>(args)...));
                } else {
                    return std::invoke(Method, instance, std::forward<Args>(args)...);
                }
            }

            static Return invoke_function_pointer(const void *storage, Args... args) {
                function_pointer function = nullptr;
                std::memcpy(&function, storage, sizeof(function));
                return function(std::forward<Args>(args)...);
            }

            template <typename Callable>
            static Return invoke_callable(const void *storage, Args... args) {
                const Callable *callable = static_cast<const Callable *>(storage);
                if constexpr (std::is_void_v<Return>) {
                    static_cast<void>(std::invoke(*callable, std::forward<Args>(args)...));
                } else {
                    return std::invoke(*callable, std::forward<Args>(args)...);
                }
            }
    };
}
