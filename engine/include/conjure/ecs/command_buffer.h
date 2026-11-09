#pragma once

#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity.h"

#include "conjure/core/assert.h"
#include "conjure/core/memory/arena.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

// Deferred structural changes (docs/design/01_ecs.md 2.5, 3.4, 4.6).
//
//   command_buffer commands(world);
//   const entity spawned = commands.create_entity();     // a real id, usable right away
//   commands.add<position>(spawned, position{1.0f, 2.0f, 3.0f});
//   commands.destroy_entity(other);
//   flush_report report = world.flush(commands);
//
// Records live in an arena; payloads are moved in and destroyed on apply or clear(). Each buffer owns
// a block of up to 64 reserved ids refilled from the world's allocator (the module's only mutex), so
// create_entity() is safe from job threads as long as one buffer belongs to one thread at a time.

namespace conjure::ecs {

    class entity_archive;
    class world;

    /// <summary>
    /// Outcome of world::flush.
    /// </summary>
    struct flush_report {
        u32 applied = 0;      // commands applied
        u32 skipped = 0;      // commands dropped because their entity was dead or stale at flush
    };

    class command_buffer {
        public:
            /// <summary>
            /// Ids reserved per refill.
            /// </summary>
            static constexpr u32 RESERVE_BLOCK = 64;

        private:
            friend class world;

            enum class opcode : u8 {
                CREATE,
                DESTROY,
                ADD,
                SET,
                REMOVE,
                INSTANTIATE
            };

            struct record {
                opcode op = opcode::CREATE;
                bool payloadLive = false;       // payload constructed and not yet consumed
                entity target = {};
                component_id id = {};
                u32 payloadSize = 0;
                u32 instantiateIndex = 0;       // INSTANTIATE: index into mInstantiates
                void *payload = nullptr;
                record *next = nullptr;
            };

            struct instantiate_entry {
                std::shared_ptr<const entity_archive> archive = {};
                std::vector<entity> ids = {};       // copies * entries, reserved at record time
                u32 copies = 0;
            };

            world *mWorld = nullptr;
            const component_registry *mRegistry = nullptr;
            core::memory::arena mArena;                       // no default: block size chosen by the constructor
            record *mFirst = nullptr;
            record *mLast = nullptr;
            u32 mCount = 0;
            std::array<entity, RESERVE_BLOCK> mReserved = {};
            u32 mReservedCount = 0;
            std::vector<instantiate_entry> mInstantiates = {};

        public:
            explicit command_buffer(world &target);

            /// <summary>
            /// Discards pending commands with a WARN (payloads are destroyed) and returns unused ids.
            /// </summary>
            ~command_buffer();

            command_buffer(const command_buffer &) = delete;
            command_buffer &operator=(const command_buffer &) = delete;

            command_buffer(command_buffer &&other) noexcept;
            command_buffer &operator=(command_buffer &&other) noexcept;

        public:
            /// <summary>
            /// Reserves a real entity id that becomes alive at flush. The id may be used in later
            /// commands of any buffer of the same world and handed to scripts at once. Returns
            /// NULL_ENTITY (and logs) when the id space is exhausted.
            /// </summary>
            [[nodiscard]] entity create_entity();

            /// <summary>
            /// Records the destruction of an entity. Wins over every other command on it in this buffer.
            /// </summary>
            void destroy_entity(entity target);

            /// <summary>
            /// Records adding a component with an initial value (moved in). On an entity that already
            /// has it the value is applied like set(). An unregistered type asserts and records nothing.
            /// </summary>
            template <typename ComponentType>
            void add(entity target, ComponentType value = {}) {
                using plain_type = std::remove_cvref_t<ComponentType>;
                const component_id id = mRegistry->id_of<plain_type>();
                if (!id.is_valid()) {
                    CONJURE_ASSERT(
                      false,
                      "command_buffer::add of unregistered component '{}'",
                      component_name<plain_type>::VALUE
                    );
                    return;
                }
                void *slot = push_record(opcode::ADD, target, id, payload_size<plain_type>(), alignof(plain_type));
                if (slot != nullptr) {
                    std::construct_at(static_cast<plain_type *>(slot), std::move(value));
                }
            }

            /// <summary>
            /// Records replacing the value of a component the entity has at flush (a missing component is
            /// skipped and counted). An unregistered type asserts and records nothing.
            /// </summary>
            template <typename ComponentType>
            void set(entity target, ComponentType value) {
                using plain_type = std::remove_cvref_t<ComponentType>;
                const component_id id = mRegistry->id_of<plain_type>();
                if (!id.is_valid()) {
                    CONJURE_ASSERT(
                      false,
                      "command_buffer::set of unregistered component '{}'",
                      component_name<plain_type>::VALUE
                    );
                    return;
                }
                void *slot = push_record(opcode::SET, target, id, payload_size<plain_type>(), alignof(plain_type));
                if (slot != nullptr) {
                    std::construct_at(static_cast<plain_type *>(slot), std::move(value));
                }
            }

            /// <summary>
            /// Records removing a component. Cancels a pending add of the same component in this buffer.
            /// An unregistered type asserts and records nothing.
            /// </summary>
            template <typename ComponentType>
            void remove(entity target) {
                using plain_type = std::remove_cvref_t<ComponentType>;
                remove_raw(target, mRegistry->id_of<plain_type>());
            }

            /// <summary>
            /// Untyped add: the payload bytes are copy-constructed into the record (ops.copy or memcpy). An
            /// empty payload means the default value.
            /// </summary>
            void add_raw(entity target, component_id id, std::span<const std::byte> payload = {});

            /// <summary>
            /// Untyped set.
            /// </summary>
            void set_raw(entity target, component_id id, std::span<const std::byte> payload);

            /// <summary>
            /// Untyped remove.
            /// </summary>
            void remove_raw(entity target, component_id id);

            /// <summary>
            /// Records instantiating one copy of an archive at flush; ids are reserved now and the root
            /// is returned now. ARCHIVE_CORRUPT when the archive does not validate, ENTITY_LIMIT_REACHED
            /// when no ids are left.
            /// </summary>
            [[nodiscard]] core::result<entity> instantiate(std::shared_ptr<const entity_archive> archive);

            /// <summary>
            /// Moves every command of 'other' behind this buffer's commands, in order. 'other' keeps its
            /// reserved ids and is empty afterwards.
            /// </summary>
            void append(command_buffer &&other);

            /// <summary>
            /// True when no command is recorded.
            /// </summary>
            [[nodiscard]] bool empty() const noexcept {
                return mCount == 0;
            }

            /// <summary>
            /// Number of recorded commands.
            /// </summary>
            [[nodiscard]] u32 command_count() const noexcept {
                return mCount;
            }

            /// <summary>
            /// Ids reserved and not yet handed out by create_entity().
            /// </summary>
            [[nodiscard]] u32 reserved_count() const noexcept {
                return mReservedCount;
            }

            /// <summary>
            /// Drops every command, destroying payloads that were not applied. Unused reserved ids are kept.
            /// </summary>
            void clear() noexcept;

            /// <summary>
            /// The world this buffer targets.
            /// </summary>
            [[nodiscard]] world &target() const noexcept {
                return *mWorld;
            }

        private:
            template <typename ComponentType>
            [[nodiscard]] static constexpr u32 payload_size() noexcept {
                if constexpr (std::is_empty_v<ComponentType>) {
                    return 0;
                } else {
                    return static_cast<u32>(sizeof(ComponentType));
                }
            }

            [[nodiscard]] void *push_record(opcode op, entity target, component_id id, u32 payloadSize, u32 alignment);
            record *push_plain_record(opcode op, entity target, component_id id);
            [[nodiscard]] core::result<std::span<const entity>> push_instantiate(
              std::shared_ptr<const entity_archive> archive,
              u32 copies
            );
            void destroy_payloads() noexcept;
            void return_reserved() noexcept;
            [[nodiscard]] bool refill_reserved();
    };
}
