#include "conjure/ecs/command_buffer.h"

#include "ecs/entity_allocator.h"

#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/log_category.h"
#include "conjure/ecs/world.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <cstring>
#include <utility>

using namespace conjure;
using namespace conjure::ecs;

namespace {
    /// <summary>
    /// Arena block size: records are small, payloads rarely exceed a few hundred bytes.
    /// </summary>
    constexpr usize COMMAND_ARENA_BLOCK_BYTES = 16 * KIB;
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

command_buffer::command_buffer(world &target) :
    mWorld(&target),
    mRegistry(&target.registry()),
    mArena(COMMAND_ARENA_BLOCK_BYTES) {}

command_buffer::~command_buffer() {
    if (mCount > 0) {
        CONJURE_LOG_WARN(LOG_ECS, "command_buffer destroyed with {} pending command(s); they are discarded", mCount);
    }
    clear();
    return_reserved();
}

command_buffer::command_buffer(command_buffer &&other) noexcept :
    mWorld(other.mWorld),
    mRegistry(other.mRegistry),
    mArena(std::move(other.mArena)),
    mFirst(other.mFirst),
    mLast(other.mLast),
    mCount(other.mCount),
    mReserved(other.mReserved),
    mReservedCount(other.mReservedCount),
    mInstantiates(std::move(other.mInstantiates)) {
    other.mFirst = nullptr;
    other.mLast = nullptr;
    other.mCount = 0;
    other.mReservedCount = 0;
    other.mInstantiates.clear();
}

command_buffer &command_buffer::operator=(command_buffer &&other) noexcept {
    if (this != &other) {
        clear();
        return_reserved();
        mWorld = other.mWorld;
        mRegistry = other.mRegistry;
        mArena = std::move(other.mArena);
        mFirst = other.mFirst;
        mLast = other.mLast;
        mCount = other.mCount;
        mReserved = other.mReserved;
        mReservedCount = other.mReservedCount;
        mInstantiates = std::move(other.mInstantiates);
        other.mFirst = nullptr;
        other.mLast = nullptr;
        other.mCount = 0;
        other.mReservedCount = 0;
        other.mInstantiates.clear();
    }
    return *this;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

entity command_buffer::create_entity() {
    if (mReservedCount == 0 && !refill_reserved()) {
        return NULL_ENTITY;
    }
    --mReservedCount;
    const entity created = mReserved[mReservedCount];
    push_plain_record(opcode::CREATE, created, component_id{});
    return created;
}

void command_buffer::destroy_entity(entity target) {
    push_plain_record(opcode::DESTROY, target, component_id{});
}

void command_buffer::add_raw(entity target, component_id id, std::span<const std::byte> payload) {
    if (!mRegistry->is_registered(id)) {
        CONJURE_ASSERT(false, "command_buffer::add_raw with an unregistered component id");
        return;
    }
    const component_info &info = mRegistry->info(id);
    if (info.is_tag() || payload.empty()) {
        push_plain_record(opcode::ADD, target, id);
        return;
    }
    if (payload.size() != info.size) {
        CONJURE_ASSERT(
          false,
          "command_buffer::add_raw payload of {} bytes for a {} byte component",
          payload.size(),
          info.size
        );
        return;
    }
    void *slot = push_record(opcode::ADD, target, id, info.size, info.alignment);
    if (slot == nullptr) {
        return;
    }
    if (info.ops.copy != nullptr) {
        info.ops.copy(slot, payload.data(), 1);
    } else {
        std::memcpy(slot, payload.data(), info.size);
    }
}

void command_buffer::set_raw(entity target, component_id id, std::span<const std::byte> payload) {
    if (!mRegistry->is_registered(id)) {
        CONJURE_ASSERT(false, "command_buffer::set_raw with an unregistered component id");
        return;
    }
    const component_info &info = mRegistry->info(id);
    if (info.is_tag()) {
        return;
    }
    if (payload.size() != info.size) {
        CONJURE_ASSERT(
          false,
          "command_buffer::set_raw payload of {} bytes for a {} byte component",
          payload.size(),
          info.size
        );
        return;
    }
    void *slot = push_record(opcode::SET, target, id, info.size, info.alignment);
    if (slot == nullptr) {
        return;
    }
    if (info.ops.copy != nullptr) {
        info.ops.copy(slot, payload.data(), 1);
    } else {
        std::memcpy(slot, payload.data(), info.size);
    }
}

void command_buffer::remove_raw(entity target, component_id id) {
    if (!mRegistry->is_registered(id)) {
        CONJURE_ASSERT(false, "command_buffer::remove_raw with an unregistered component id");
        return;
    }
    push_plain_record(opcode::REMOVE, target, id);
}

core::result<entity> command_buffer::instantiate(std::shared_ptr<const entity_archive> archive) {
    if (archive == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "instantiate needs an archive");
    }
    if (&archive->registry() != mRegistry) {
        return make_error(ecs_error::ARCHIVE_CORRUPT, "the archive belongs to another component registry");
    }
    CONJURE_TRY(archive->validate());
    if (archive->entity_count() == 0) {
        return NULL_ENTITY;
    }
    CONJURE_TRY_ASSIGN(const std::span<const entity> ids, push_instantiate(archive, 1));
    return ids[0];
}

void command_buffer::append(command_buffer &&other) {
    if (&other == this || other.mCount == 0) {
        return;
    }
    CONJURE_ASSERT(other.mWorld == mWorld, "command_buffer::append across worlds");
    if (other.mWorld != mWorld) {
        return;
    }

    for (record *source = other.mFirst; source != nullptr; source = source->next) {
        if (source->op == opcode::INSTANTIATE) {
            record *copied = push_plain_record(opcode::INSTANTIATE, source->target, source->id);
            if (copied == nullptr) {
                continue;
            }
            copied->instantiateIndex = static_cast<u32>(mInstantiates.size());
            mInstantiates.push_back(std::move(other.mInstantiates[source->instantiateIndex]));
            continue;
        }
        if (!source->payloadLive) {
            push_plain_record(source->op, source->target, source->id);
            continue;
        }
        const component_info &info = mRegistry->info(source->id);
        void *slot = push_record(source->op, source->target, source->id, info.size, info.alignment);
        if (slot == nullptr) {
            continue;
        }
        if (info.ops.relocate != nullptr) {
            info.ops.relocate(slot, source->payload, 1);
        } else {
            std::memcpy(slot, source->payload, info.size);
        }
        source->payloadLive = false;
    }

    other.mInstantiates.clear();
    other.mFirst = nullptr;
    other.mLast = nullptr;
    other.mCount = 0;
    other.mArena.reset();
}

void command_buffer::clear() noexcept {
    destroy_payloads();
    for (instantiate_entry &entry : mInstantiates) {
        if (!entry.ids.empty()) {
            mWorld->mAllocator->return_unused(entry.ids);
        }
    }
    mInstantiates.clear();
    mFirst = nullptr;
    mLast = nullptr;
    mCount = 0;
    mArena.reset();
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void *command_buffer::push_record(opcode op, entity target, component_id id, u32 payloadSize, u32 alignment) {
    record *entry = push_plain_record(op, target, id);
    if (entry == nullptr || payloadSize == 0) {
        return nullptr;
    }
    void *payload = mArena.allocate(payloadSize, alignment);
    if (payload == nullptr) {
        CONJURE_ASSERT(false, "command_buffer could not allocate a {} byte payload", payloadSize);
        // Drop the record again (it is the last one): the buffer stays consistent without it.
        if (mFirst == entry) {
            mFirst = nullptr;
            mLast = nullptr;
        } else {
            record *previous = mFirst;
            while (previous->next != entry) {
                previous = previous->next;
            }
            previous->next = nullptr;
            mLast = previous;
        }
        --mCount;
        return nullptr;
    }
    entry->payload = payload;
    entry->payloadSize = payloadSize;
    entry->payloadLive = true;
    return payload;
}

command_buffer::record *command_buffer::push_plain_record(opcode op, entity target, component_id id) {
    record *entry = mArena.construct<record>();
    if (entry == nullptr) {
        CONJURE_ASSERT(false, "command_buffer could not allocate a record");
        return nullptr;
    }
    entry->op = op;
    entry->target = target;
    entry->id = id;
    if (mLast == nullptr) {
        mFirst = entry;
    } else {
        mLast->next = entry;
    }
    mLast = entry;
    ++mCount;
    return entry;
}

core::result<std::span<const entity>> command_buffer::push_instantiate(
  std::shared_ptr<const entity_archive> archive,
  u32 copies
) {
    if (&archive->registry() != mRegistry) {
        return make_error(ecs_error::ARCHIVE_CORRUPT, "the archive belongs to another component registry");
    }
    CONJURE_TRY(archive->validate());

    instantiate_entry entry = {};
    entry.copies = copies;
    entry.ids.resize(static_cast<usize>(archive->entity_count()) * copies);
    CONJURE_TRY(mWorld->mAllocator->reserve(entry.ids));
    entry.archive = std::move(archive);

    record *command = push_plain_record(opcode::INSTANTIATE, NULL_ENTITY, component_id{});
    if (command == nullptr) {
        mWorld->mAllocator->return_unused(entry.ids);
        return make_error(ecs_error::OUT_OF_MEMORY, "command_buffer could not record the instantiation");
    }
    command->instantiateIndex = static_cast<u32>(mInstantiates.size());
    mInstantiates.push_back(std::move(entry));
    return std::span<const entity>(mInstantiates.back().ids);
}

void command_buffer::destroy_payloads() noexcept {
    for (record *entry = mFirst; entry != nullptr; entry = entry->next) {
        if (!entry->payloadLive) {
            continue;
        }
        // A payload only ever carries a registered type; the check keeps a corrupt record from running
        // another type's destructor over its bytes.
        if (mRegistry->is_registered(entry->id)) {
            const component_info &info = mRegistry->info(entry->id);
            if (info.ops.destruct != nullptr) {
                info.ops.destruct(entry->payload, 1);
            }
        }
        entry->payloadLive = false;
    }
}

void command_buffer::return_reserved() noexcept {
    if (mReservedCount == 0) {
        return;
    }
    mWorld->mAllocator->return_unused(std::span<const entity>(mReserved.data(), mReservedCount));
    mReservedCount = 0;
}

bool command_buffer::refill_reserved() {
    const core::result<void> reserved = mWorld->mAllocator->reserve(std::span<entity>(mReserved.data(), RESERVE_BLOCK));
    if (!reserved.has_value()) {
        CONJURE_LOG_WARN(LOG_ECS, "command_buffer could not reserve entity ids: {}", reserved.get_error().message());
        return false;
    }
    mReservedCount = RESERVE_BLOCK;
    return true;
}
