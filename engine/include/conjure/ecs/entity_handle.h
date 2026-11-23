#pragma once

#include "conjure/ecs/component.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <type_traits>
#include <utility>

// The OOP facade (docs/design/01_ecs.md 4.5). Handles and component references are non-owning values
// that re-validate against the world on every call, so a stale handle reads as dead instead of
// touching freed memory.
//
//   entity_handle player = world.handle(id);
//   component_ref<position> position = player.component<position>();
//   if (position.is_valid()) { position.write()->y += 1.0f; }

namespace conjure::ecs {

    /// <summary>
    /// A reference to one component of one entity that resolves the storage on every access.
    /// </summary>
    template <typename ComponentType>
    class component_ref {
        private:
            world *mWorld = nullptr;
            entity mEntity = {};

        public:
            /// <summary>
            /// A reference to nothing.
            /// </summary>
            component_ref() noexcept = default;

            component_ref(world &target, entity owner) noexcept :
                mWorld(&target),
                mEntity(owner) {}

            ~component_ref() = default;

            component_ref(const component_ref &) noexcept = default;
            component_ref &operator=(const component_ref &) noexcept = default;

        public:
            /// <summary>
            /// True while the entity is alive and holds the component.
            /// </summary>
            [[nodiscard]] bool is_valid() const noexcept {
                return mWorld != nullptr && mWorld->has<ComponentType>(mEntity);
            }

            /// <summary>
            /// The former name of is_valid(), kept for one wave (code sweep F624); returns is_valid().
            /// </summary>
            [[deprecated("use is_valid()")]] [[nodiscard]] bool valid() const noexcept {
                return is_valid();
            }

            /// <summary>
            /// The entity this reference points at.
            /// </summary>
            [[nodiscard]] entity owner() const noexcept {
                return mEntity;
            }

            /// <summary>
            /// The component for reading; nullptr when dead or missing. Never stamps.
            /// </summary>
            [[nodiscard]] const ComponentType *read() const noexcept {
                if (mWorld == nullptr) {
                    return nullptr;
                }
                return static_cast<const world &>(*mWorld).get<ComponentType>(mEntity);
            }

            /// <summary>
            /// The component for writing; stamps its chunk column. nullptr when dead or missing.
            /// </summary>
            [[nodiscard]] ComponentType *write() noexcept {
                if (mWorld == nullptr) {
                    return nullptr;
                }
                return mWorld->get<ComponentType>(mEntity);
            }

            /// <summary>
            /// Replaces the value. ENTITY_NOT_ALIVE / COMPONENT_NOT_PRESENT when the reference is invalid.
            /// </summary>
            [[nodiscard]] core::result<void> set(ComponentType value) {
                if (mWorld == nullptr) {
                    return make_error(ecs_error::ENTITY_NOT_ALIVE, "component_ref points at no world");
                }
                return mWorld->set<ComponentType>(mEntity, std::move(value));
            }
    };

    /// <summary>
    /// An entity plus its world. Every call re-validates the entity.
    /// </summary>
    class entity_handle {
        private:
            world *mWorld = nullptr;
            entity mEntity = {};

        public:
            /// <summary>
            /// A handle to nothing.
            /// </summary>
            entity_handle() noexcept = default;

            entity_handle(world &target, entity id) noexcept :
                mWorld(&target),
                mEntity(id) {}

            ~entity_handle() = default;

            entity_handle(const entity_handle &) noexcept = default;
            entity_handle &operator=(const entity_handle &) noexcept = default;

        public:
            /// <summary>
            /// The underlying entity id.
            /// </summary>
            [[nodiscard]] entity id() const noexcept {
                return mEntity;
            }

            /// <summary>
            /// The world the handle belongs to, or nullptr.
            /// </summary>
            [[nodiscard]] world *owner() const noexcept {
                return mWorld;
            }

            /// <summary>
            /// True while the entity is alive.
            /// </summary>
            [[nodiscard]] bool is_alive() const noexcept {
                return mWorld != nullptr && mWorld->is_alive(mEntity);
            }

            /// <summary>
            /// Adds a component (world::add) and returns a reference to it.
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] core::result<component_ref<ComponentType>> add(ComponentType value = {}) {
                if (mWorld == nullptr) {
                    return make_error(ecs_error::ENTITY_NOT_ALIVE, "entity_handle points at no world");
                }
                CONJURE_TRY(mWorld->add<ComponentType>(mEntity, std::move(value)));
                return component_ref<ComponentType>(*mWorld, mEntity);
            }

            /// <summary>
            /// A reference to a component; is_valid() tells whether it exists.
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] component_ref<ComponentType> component() const noexcept {
                if (mWorld == nullptr) {
                    return component_ref<ComponentType>();
                }
                return component_ref<ComponentType>(*mWorld, mEntity);
            }

            /// <summary>
            /// True when the entity is alive and holds the component.
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] bool has() const noexcept {
                return mWorld != nullptr && mWorld->has<ComponentType>(mEntity);
            }

            /// <summary>
            /// Removes a component (world::remove).
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] core::result<void> remove() {
                if (mWorld == nullptr) {
                    return make_error(ecs_error::ENTITY_NOT_ALIVE, "entity_handle points at no world");
                }
                return mWorld->remove<ComponentType>(mEntity);
            }

            /// <summary>
            /// Destroys the entity (world::destroy_entity).
            /// </summary>
            [[nodiscard]] core::result<void> destroy() {
                if (mWorld == nullptr) {
                    return make_error(ecs_error::ENTITY_NOT_ALIVE, "entity_handle points at no world");
                }
                return mWorld->destroy_entity(mEntity);
            }

            [[nodiscard]] bool operator==(const entity_handle &other) const noexcept = default;
    };

    inline entity_handle world::handle(entity target) noexcept {
        return entity_handle(*this, target);
    }
}
