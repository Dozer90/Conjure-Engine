#pragma once

#include <memory>
#include <type_traits>
#include <utility>

// Type erasure for published payloads (docs/design/05_assets.md 2.5). This is the only erasure in
// the module: asset types need no base class, no shared_ptr and no virtual call on the hot path.

namespace conjure::assets {

    /// <summary>
    /// An owned, type-erased payload: the object pointer plus the deleter that knows its type.
    /// </summary>
    using asset_payload = std::unique_ptr<void, void (*)(void *)>;

    namespace detail {
        template <typename ValueType>
        void delete_payload(void *object) noexcept {
            std::default_delete<ValueType>{}(static_cast<ValueType *>(object));
        }

        inline void delete_nothing(void *) noexcept {}
    }

    /// <summary>
    /// A payload that owns nothing. Destroying it does nothing.
    /// </summary>
    [[nodiscard]] inline asset_payload make_null_payload() noexcept {
        return asset_payload(nullptr, &detail::delete_nothing);
    }

    /// <summary>
    /// Moves (or copies) a value onto the heap and erases its type. The payload destroys the
    /// value through its real type.
    /// </summary>
    template <typename ValueType>
    [[nodiscard]] asset_payload make_payload(ValueType &&value) {
        using stored_type = std::remove_cvref_t<ValueType>;
        static_assert(!std::is_pointer_v<stored_type>, "make_payload takes the value, not a pointer to it");
        std::unique_ptr<stored_type> object = std::make_unique<stored_type>(std::forward<ValueType>(value));
        return asset_payload(static_cast<void *>(object.release()), &detail::delete_payload<stored_type>);
    }

    /// <summary>
    /// Views the payload as its real type. The caller must know the type (the slot's type id).
    /// </summary>
    template <typename ValueType>
    [[nodiscard]] ValueType *payload_as(const asset_payload &payload) noexcept {
        return static_cast<ValueType *>(payload.get());
    }
}
