#pragma once

#include "conjure/ecs/entity.h"

#include "conjure/core/assert.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/hash.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <array>
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Components (docs/design/01_ecs.md 2.2).
//
//   struct velocity { f32 x = 0.0f; f32 y = 0.0f; f32 z = 0.0f; };
//   CONJURE_REFLECT_STRUCT(velocity, "game.velocity", CONJURE_FIELD(x), CONJURE_FIELD(y), CONJURE_FIELD(z));
//   CONJURE_COMPONENT(velocity, "game.velocity");
//
//   component_registry registry = {};
//   core::result<component_id> id = registry.register_component<velocity>();
//
// Component ids are dense 16-bit indices assigned per name in registration order and never reused for
// the registry's lifetime. A component_mask is one bit per id (1024 bits, 128 bytes) and is what
// archetype matching compares.

namespace conjure::core::json {
    class value;
    class writer;
}

namespace conjure::core::reflect {
    struct type_info;
}

namespace conjure::ecs {

    /// <summary>
    /// Most component types one registry can hold.
    /// </summary>
    inline constexpr u32 MAX_COMPONENTS = 1024;

    /// <summary>
    /// Largest component in bytes; larger data belongs behind an asset handle.
    /// </summary>
    inline constexpr u32 MAX_COMPONENT_SIZE = 4096;

    /// <summary>
    /// Strictest component alignment accepted.
    /// </summary>
    inline constexpr u32 MAX_COMPONENT_ALIGNMENT = 64;

    /// <summary>
    /// Dense index of a component type within one registry. INVALID means "no component".
    /// </summary>
    struct component_id {
        static constexpr u16 INVALID = 0xFFFF;

        u16 value = INVALID;

        /// <summary>
        /// True for an id below MAX_COMPONENTS (not INVALID).
        /// </summary>
        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return value < MAX_COMPONENTS;
        }

        [[nodiscard]] constexpr bool operator==(const component_id &other) const noexcept = default;
    };

    /// <summary>
    /// Properties of a component type.
    /// </summary>
    enum class component_flags : u32 {
        NONE = 0,
        TAG = 1 << 0,              // size 0, lives only in the mask
        TRIVIAL = 1 << 1,          // memcpy relocate/copy, no destructor
        DYNAMIC = 1 << 2,          // registered at runtime (C#), fields described by data
        TRANSIENT = 1 << 3,        // never serialised, no reflection required
        EDITOR_HIDDEN = 1 << 4     // internal types (unresolved_components); area H calls this INTERNAL (R32)
    };
    CONJURE_ENUM_FLAGS(component_flags);

    /// <summary>
    /// Lifecycle hooks of a non-trivial component. A null pointer selects the trivial path: zero-fill or
    /// the registered default bytes for construction, no-op destruction, memcpy for relocation and copy.
    /// </summary>
    struct component_ops {
        void (*constructDefault)(void *destination, u32 count) = nullptr;
        void (*destruct)(void *object, u32 count) = nullptr;
        void (*relocate)(void *destination, void *source, u32 count) = nullptr;    // move + destruct source
        void (*copy)(void *destination, const void *source, u32 count) = nullptr;  // copy-construct
    };

    /// <summary>
    /// Serialisation hooks of a component type. Null hooks select the reflection-driven default, which
    /// walks component_info::reflection (serialization.h). remapEntities is consulted only when a type
    /// stores entity references that reflection cannot see: entity_archive::capture calls it with a
    /// table whose targets are the captured entities (real -> archive-local) and world::instantiate with
    /// the table of the created copy (archive-local -> real). A hook rewrites each reference through
    /// entity_remap::remap, which serves both directions; references to entities outside the archive
    /// are left as they are (archive_options::keepExternalReferences only governs reflected fields).
    /// </summary>
    struct component_serializer {
        core::result<void> (*writeJson)(const void *component, core::json::writer &out) = nullptr;
        core::result<void> (*readJson)(void *component, const core::json::value &in) = nullptr;
        void (*remapEntities)(void *component, const entity_remap &map) = nullptr;
    };

    /// <summary>
    /// Everything the registry knows about one component type. A registered component_info stays at a
    /// stable address for the registry's lifetime.
    /// </summary>
    struct component_info {
        component_id id = {};
        core::string_id name = {};
        u32 size = 0;
        u32 alignment = 1;
        component_flags flags = component_flags::NONE;
        component_ops ops = {};
        component_serializer serializer = {};
        const core::reflect::type_info *reflection = nullptr;
        std::vector<u32> entityFieldOffsets = {};      // byte offsets of every entity-typed field
        std::vector<u32> assetRefFieldOffsets = {};    // byte offsets of every assets::asset_ref field (R25)
        std::vector<std::byte> defaultValue = {};      // default bytes of trivial types; empty = zero-fill

        /// <summary>
        /// True when the type has the TAG flag (no storage).
        /// </summary>
        [[nodiscard]] bool is_tag() const noexcept {
            return has_flag(flags, component_flags::TAG);
        }

        /// <summary>
        /// True when the type is relocated and copied with memcpy and needs no destructor.
        /// </summary>
        [[nodiscard]] bool is_trivial() const noexcept {
            return has_flag(flags, component_flags::TRIVIAL);
        }

        /// <summary>
        /// True when the type is never serialised.
        /// </summary>
        [[nodiscard]] bool is_transient() const noexcept {
            return has_flag(flags, component_flags::TRANSIENT);
        }
    };

    /// <summary>
    /// A set of component ids: one bit per id. Archetype matching is a handful of 64-bit operations.
    /// </summary>
    class component_mask {
        public:
            /// <summary>
            /// Number of 64-bit words.
            /// </summary>
            static constexpr u32 WORD_COUNT = MAX_COMPONENTS / 64;

        private:
            std::array<u64, WORD_COUNT> mWords = {};

        public:
            constexpr component_mask() noexcept = default;

        public:
            /// <summary>
            /// Sets the bit of 'id'. Invalid ids assert and are ignored.
            /// </summary>
            constexpr void set(component_id id) noexcept {
                if (!id.is_valid()) {
                    CONJURE_ASSERT(false, "component_mask::set with an invalid component id");
                    return;
                }
                mWords[id.value / 64] |= (u64{1} << (id.value % 64));
            }

            /// <summary>
            /// Clears the bit of 'id'. Invalid ids assert and are ignored.
            /// </summary>
            constexpr void clear(component_id id) noexcept {
                if (!id.is_valid()) {
                    CONJURE_ASSERT(false, "component_mask::clear with an invalid component id");
                    return;
                }
                mWords[id.value / 64] &= ~(u64{1} << (id.value % 64));
            }

            /// <summary>
            /// True when the bit of 'id' is set; false for an invalid id.
            /// </summary>
            [[nodiscard]] constexpr bool test(component_id id) const noexcept {
                if (!id.is_valid()) {
                    return false;
                }
                return (mWords[id.value / 64] & (u64{1} << (id.value % 64))) != 0;
            }

            /// <summary>
            /// True when every bit of 'other' is set here.
            /// </summary>
            [[nodiscard]] constexpr bool contains_all(const component_mask &other) const noexcept {
                for (u32 word = 0; word < WORD_COUNT; ++word) {
                    if ((mWords[word] & other.mWords[word]) != other.mWords[word]) {
                        return false;
                    }
                }
                return true;
            }

            /// <summary>
            /// True when at least one bit of 'other' is set here.
            /// </summary>
            [[nodiscard]] constexpr bool contains_any(const component_mask &other) const noexcept {
                for (u32 word = 0; word < WORD_COUNT; ++word) {
                    if ((mWords[word] & other.mWords[word]) != 0) {
                        return true;
                    }
                }
                return false;
            }

            /// <summary>
            /// True when no bit is set.
            /// </summary>
            [[nodiscard]] constexpr bool none() const noexcept {
                for (u32 word = 0; word < WORD_COUNT; ++word) {
                    if (mWords[word] != 0) {
                        return false;
                    }
                }
                return true;
            }

            /// <summary>
            /// Number of set bits.
            /// </summary>
            [[nodiscard]] constexpr u32 count() const noexcept {
                u32 total = 0;
                for (u32 word = 0; word < WORD_COUNT; ++word) {
                    total += static_cast<u32>(std::popcount(mWords[word]));
                }
                return total;
            }

            /// <summary>
            /// FNV-1a 64 over the words; equal masks hash equal on every platform.
            /// </summary>
            [[nodiscard]] u64 hash() const noexcept;

            /// <summary>
            /// Invokes callable(component_id) for every set bit in ascending id order.
            /// </summary>
            template <typename Callable>
            constexpr void for_each_set(Callable &&callable) const {
                for (u32 word = 0; word < WORD_COUNT; ++word) {
                    u64 bits = mWords[word];
                    while (bits != 0) {
                        const u32 bit = static_cast<u32>(std::countr_zero(bits));
                        callable(component_id{static_cast<u16>(word * 64 + bit)});
                        bits &= bits - 1;
                    }
                }
            }

            /// <summary>
            /// The raw words, lowest ids first.
            /// </summary>
            [[nodiscard]] constexpr const std::array<u64, WORD_COUNT> &words() const noexcept {
                return mWords;
            }

            [[nodiscard]] constexpr component_mask operator|(const component_mask &other) const noexcept {
                component_mask combined = {};
                for (u32 word = 0; word < WORD_COUNT; ++word) {
                    combined.mWords[word] = mWords[word] | other.mWords[word];
                }
                return combined;
            }

            [[nodiscard]] constexpr component_mask operator&(const component_mask &other) const noexcept {
                component_mask combined = {};
                for (u32 word = 0; word < WORD_COUNT; ++word) {
                    combined.mWords[word] = mWords[word] & other.mWords[word];
                }
                return combined;
            }

            [[nodiscard]] constexpr component_mask operator~() const noexcept {
                component_mask inverted = {};
                for (u32 word = 0; word < WORD_COUNT; ++word) {
                    inverted.mWords[word] = ~mWords[word];
                }
                return inverted;
            }

            constexpr component_mask &operator|=(const component_mask &other) noexcept {
                for (u32 word = 0; word < WORD_COUNT; ++word) {
                    mWords[word] |= other.mWords[word];
                }
                return *this;
            }

            constexpr component_mask &operator&=(const component_mask &other) noexcept {
                for (u32 word = 0; word < WORD_COUNT; ++word) {
                    mWords[word] &= other.mWords[word];
                }
                return *this;
            }

            [[nodiscard]] constexpr bool operator==(const component_mask &other) const noexcept = default;

            /// <summary>
            /// Hasher for std::unordered_map keys.
            /// </summary>
            struct hasher {
                [[nodiscard]] std::size_t operator()(const component_mask &mask) const noexcept {
                    return static_cast<std::size_t>(mask.hash());
                }
            };
    };

    static_assert(sizeof(component_mask) == 128, "component_mask must be 16 words");

    /// <summary>
    /// The registered name of a C++ component type. Specialised by CONJURE_COMPONENT; the name's
    /// string_id is the registry key, so the same name yields the same id in every registry.
    /// </summary>
    template <typename ComponentType>
    struct component_name;

    /// <summary>
    /// True when CONJURE_COMPONENT named the type.
    /// </summary>
    template <typename ComponentType>
    concept named_component = requires {
        { component_name<std::remove_cvref_t<ComponentType>>::VALUE } -> std::convertible_to<std::string_view>;
    };

    /// <summary>
    /// The per-type lookup cache behind component_registry::id_of: one atomic word packing
    /// (registrySerial &lt;&lt; 16) | id. A serial mismatch means "look the name up again". The static
    /// lives per shared library but only caches a lookup result, so every library agrees (R17).
    /// </summary>
    template <typename ComponentType>
    struct component_type_cache {
        [[nodiscard]] static std::atomic<u64> &slot() noexcept {
            static std::atomic<u64> sPacked = 0;
            return sPacked;
        }
    };

    namespace detail {
        template <typename ComponentType>
        void construct_default_components(void *destination, u32 count) {
            ComponentType *objects = static_cast<ComponentType *>(destination);
            for (u32 index = 0; index < count; ++index) {
                std::construct_at(objects + index);
            }
        }

        template <typename ComponentType>
        void destruct_components(void *object, u32 count) {
            ComponentType *objects = static_cast<ComponentType *>(object);
            for (u32 index = 0; index < count; ++index) {
                std::destroy_at(objects + index);
            }
        }

        template <typename ComponentType>
        void relocate_components(void *destination, void *source, u32 count) {
            ComponentType *target = static_cast<ComponentType *>(destination);
            ComponentType *origin = static_cast<ComponentType *>(source);
            for (u32 index = 0; index < count; ++index) {
                std::construct_at(target + index, std::move(origin[index]));
                std::destroy_at(origin + index);
            }
        }

        template <typename ComponentType>
        void copy_components(void *destination, const void *source, u32 count) {
            ComponentType *target = static_cast<ComponentType *>(destination);
            const ComponentType *origin = static_cast<const ComponentType *>(source);
            for (u32 index = 0; index < count; ++index) {
                std::construct_at(target + index, origin[index]);
            }
        }
    }

    /// <summary>
    /// The lifecycle hooks of a C++ type: all null for trivially copyable types (the memcpy path),
    /// otherwise the type's special members.
    /// </summary>
    template <typename ComponentType>
    [[nodiscard]] component_ops make_component_ops() noexcept {
        component_ops ops = {};
        if constexpr (!std::is_trivially_copyable_v<ComponentType>) {
            ops.constructDefault = &detail::construct_default_components<ComponentType>;
            ops.destruct = &detail::destruct_components<ComponentType>;
            ops.relocate = &detail::relocate_components<ComponentType>;
            ops.copy = &detail::copy_components<ComponentType>;
        }
        return ops;
    }

    /// <summary>
    /// The flags a C++ type implies: TAG for empty types, TRIVIAL for trivially copyable ones, combined
    /// with the caller's extra flags.
    /// </summary>
    template <typename ComponentType>
    [[nodiscard]] constexpr component_flags derive_component_flags(component_flags extra) noexcept {
        component_flags flags = extra;
        if constexpr (std::is_empty_v<ComponentType>) {
            flags |= component_flags::TAG | component_flags::TRIVIAL;
        } else if constexpr (std::is_trivially_copyable_v<ComponentType>) {
            flags |= component_flags::TRIVIAL;
        }
        return flags;
    }

    /// <summary>
    /// The default bytes of a trivially copyable type with a non-trivial default constructor (member
    /// initialisers), constructed into zeroed storage so padding stays zero. Empty when zero-fill is
    /// already the default or the type is not trivially copyable.
    /// </summary>
    template <typename ComponentType>
    [[nodiscard]] std::vector<std::byte> default_component_bytes() {
        if constexpr (std::is_empty_v<ComponentType> || !std::is_trivially_copyable_v<ComponentType> ||
          std::is_trivially_default_constructible_v<ComponentType>) {
            return {};
        } else {
            // The object is built in storage aligned for the type (a std::vector only guarantees the
            // default new alignment, too weak for alignas(32/64) types) and copied out afterwards.
            alignas(ComponentType) std::byte storage[sizeof(ComponentType)] = {};
            ComponentType *object = ::new (static_cast<void *>(storage)) ComponentType();
            bool allZero = true;
            for (const std::byte value : storage) {
                if (value != std::byte{0}) {
                    allZero = false;
                    break;
                }
            }
            std::destroy_at(object);
            if (allZero) {
                return {};
            }
            std::vector<std::byte> bytes(sizeof(ComponentType));
            std::memcpy(bytes.data(), storage, sizeof(ComponentType));
            return bytes;
        }
    }
}

// Names a C++ component type. Used at global namespace scope; takes the caller's semicolon.
#define CONJURE_COMPONENT(Type, name)                                                                       \
    template <>                                                                                             \
    struct conjure::ecs::component_name<Type> {                                                             \
        static constexpr std::string_view VALUE = name;                                                     \
    }
