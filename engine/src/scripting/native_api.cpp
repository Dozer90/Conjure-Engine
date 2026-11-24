#include "conjure/scripting/native_api.h"

#include "conjure/scripting/scripting_error.h"

#include "conjure/core/hash.h"

#include <array>
#include <cstring>
#include <format>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::scripting;

namespace {

    /*---------------------------------------------------------------*/
    // Descriptor table: the generated, already normalised rows of native_api.inl (native_api_hash.h).

    struct raw_entry {
        std::string_view group = {};
        std::string_view name = {};
        std::string_view signature = {};
    };

    constexpr std::array<raw_entry, CONJURE_NATIVE_API_ENTRY_COUNT> RAW_ENTRIES = {{
#define CONJURE_NATIVE_ENTRY(group, name, signature) raw_entry{#group, #name, signature},
        CONJURE_NATIVE_API_FOR_EACH_ENTRY(CONJURE_NATIVE_ENTRY)
#undef CONJURE_NATIVE_ENTRY
    }};

    struct descriptor_table {
        std::array<std::string, CONJURE_NATIVE_API_ENTRY_COUNT> fullNames = {};
        std::array<native_entry_descriptor, CONJURE_NATIVE_API_ENTRY_COUNT> descriptors = {};
        u64 hash = 0;

        descriptor_table() {
            u64 running = core::hash::FNV1A64_OFFSET_BASIS;
            for (u32 index = 0; index < CONJURE_NATIVE_API_ENTRY_COUNT; ++index) {
                const raw_entry &raw = RAW_ENTRIES[index];
                fullNames[index] = std::string(raw.group) + "_" + std::string(raw.name);
                descriptors[index].group = raw.group;
                descriptors[index].name = raw.name;
                descriptors[index].signature = raw.signature;
                descriptors[index].index = index;

                const std::string line = fullNames[index] + " " + std::string(raw.signature) + "\n";
                running = core::hash::fnv1a64(line, running);
            }
            hash = running;
        }
    };

    [[nodiscard]] const descriptor_table &table() noexcept {
        static const descriptor_table sTable = {};
        return sTable;
    }

    constexpr usize HEADER_SIZE = offsetof(conjure_native_api, log_write);
}

std::string_view conjure::scripting::to_string(script_field_kind kind) noexcept {
    switch (kind) {
        case script_field_kind::BOOL:
            return "BOOL";
        case script_field_kind::INT32:
            return "INT32";
        case script_field_kind::INT64:
            return "INT64";
        case script_field_kind::FLOAT:
            return "FLOAT";
        case script_field_kind::DOUBLE:
            return "DOUBLE";
        case script_field_kind::STRING:
            return "STRING";
        case script_field_kind::VEC2:
            return "VEC2";
        case script_field_kind::VEC3:
            return "VEC3";
        case script_field_kind::VEC4:
            return "VEC4";
        case script_field_kind::QUAT:
            return "QUAT";
        case script_field_kind::COLOR:
            return "COLOR";
        case script_field_kind::ENTITY:
            return "ENTITY";
        case script_field_kind::ASSET:
            return "ASSET";
        case script_field_kind::ENUM:
            return "ENUM";
        case script_field_kind::LIST:
            return "LIST";
        case script_field_kind::COUNT:
            break;
    }
    return "COUNT";
}

script_field_kind conjure::scripting::parse_script_field_kind(std::string_view name) noexcept {
    for (u8 index = 0; index < static_cast<u8>(script_field_kind::COUNT); ++index) {
        const script_field_kind kind = static_cast<script_field_kind>(index);
        if (to_string(kind) == name) {
            return kind;
        }
    }
    return script_field_kind::COUNT;
}

std::string_view conjure::scripting::to_string(script_callback callback) noexcept {
    switch (callback) {
        case script_callback::CREATE:
            return "CREATE";
        case script_callback::START:
            return "START";
        case script_callback::FIXED_UPDATE:
            return "FIXED_UPDATE";
        case script_callback::UPDATE:
            return "UPDATE";
        case script_callback::LATE_UPDATE:
            return "LATE_UPDATE";
        case script_callback::DESTROY:
            return "DESTROY";
        case script_callback::COLLISION_ENTER:
            return "COLLISION_ENTER";
        case script_callback::COLLISION_EXIT:
            return "COLLISION_EXIT";
        case script_callback::TRIGGER_ENTER:
            return "TRIGGER_ENTER";
        case script_callback::TRIGGER_EXIT:
            return "TRIGGER_EXIT";
        case script_callback::RELOAD:
            return "RELOAD";
        case script_callback::COUNT:
            break;
    }
    return "COUNT";
}

script_callback conjure::scripting::parse_script_callback(std::string_view name) noexcept {
    for (u8 index = 0; index < static_cast<u8>(script_callback::COUNT); ++index) {
        const script_callback callback = static_cast<script_callback>(index);
        if (to_string(callback) == name) {
            return callback;
        }
    }
    return script_callback::COUNT;
}

std::string_view conjure::scripting::to_string(script_phase phase) noexcept {
    switch (phase) {
        case script_phase::FIXED_UPDATE:
            return "FIXED_UPDATE";
        case script_phase::UPDATE:
            return "UPDATE";
        case script_phase::LATE_UPDATE:
            return "LATE_UPDATE";
        case script_phase::COUNT:
            break;
    }
    return "COUNT";
}

std::span<const native_entry_descriptor> conjure::scripting::native_entry_descriptors() noexcept {
    return table().descriptors;
}

u64 conjure::scripting::compute_native_api_hash() noexcept {
    return table().hash;
}

conjure_native_api conjure::scripting::make_native_api(const conjure_frame_block *frame) noexcept {
    conjure_native_api api = {};
    api.size = static_cast<uint32_t>(sizeof(conjure_native_api));
    api.version = CONJURE_NATIVE_API_VERSION;
    api.layout_hash = CONJURE_NATIVE_API_HASH;
    api.frame = frame;
    return api;
}

std::span<void *const> conjure::scripting::native_api_slots(const conjure_native_api &api) noexcept {
    // The entries are a contiguous run of function pointers after the header (static_asserted in the
    // header); the bytes are read through void * because function and object pointers share a size here.
    const auto *slots = reinterpret_cast<void *const *>(reinterpret_cast<const std::byte *>(&api) + HEADER_SIZE);
    return std::span<void *const>(slots, CONJURE_NATIVE_API_ENTRY_COUNT);
}

std::span<void *> conjure::scripting::native_api_slots(conjure_native_api &api) noexcept {
    auto *slots = reinterpret_cast<void **>(reinterpret_cast<std::byte *>(&api) + HEADER_SIZE);
    return std::span<void *>(slots, CONJURE_NATIVE_API_ENTRY_COUNT);
}

i32 conjure::scripting::find_native_entry(std::string_view fullName) noexcept {
    const descriptor_table &entries = table();
    for (u32 index = 0; index < CONJURE_NATIVE_API_ENTRY_COUNT; ++index) {
        if (entries.fullNames[index] == fullName) {
            return static_cast<i32>(index);
        }
    }
    return -1;
}

core::result<void> conjure::scripting::validate_native_api(const conjure_native_api &api) {
    if (api.size != sizeof(conjure_native_api)) {
        return core::make_error(
          script_error::ABI_MISMATCH,
          "native table size {} differs from {}",
          api.size,
          sizeof(conjure_native_api)
        );
    }
    if (api.version != CONJURE_NATIVE_API_VERSION) {
        return core::make_error(
          script_error::ABI_MISMATCH,
          "native table version {} differs from {}",
          api.version,
          static_cast<u32>(CONJURE_NATIVE_API_VERSION)
        );
    }
    if (api.layout_hash != CONJURE_NATIVE_API_HASH) {
        return core::make_error(
          script_error::ABI_MISMATCH,
          "native table layout hash {:#018x} differs from {:#018x}",
          api.layout_hash,
          static_cast<u64>(CONJURE_NATIVE_API_HASH)
        );
    }

    const std::span<void *const> slots = native_api_slots(api);
    for (u32 index = 0; index < CONJURE_NATIVE_API_CORE_ENTRY_COUNT; ++index) {
        if (slots[index] == nullptr) {
            return core::make_error(
              script_error::NOT_INITIALISED,
              "core native entry '{}' is not filled",
              table().fullNames[index]
            );
        }
    }
    return core::result<void>::success();
}

core::result<void> conjure::scripting::validate_managed_api(const conjure_managed_api &api) {
    if (api.size != sizeof(conjure_managed_api)) {
        return core::make_error(
          script_error::ABI_MISMATCH,
          "managed table size {} differs from {}",
          api.size,
          sizeof(conjure_managed_api)
        );
    }
    if (api.version != CONJURE_NATIVE_API_VERSION) {
        return core::make_error(
          script_error::ABI_MISMATCH,
          "managed table version {} differs from {}",
          api.version,
          static_cast<u32>(CONJURE_NATIVE_API_VERSION)
        );
    }
    if (api.layout_hash != CONJURE_NATIVE_API_HASH) {
        return core::make_error(
          script_error::ABI_MISMATCH,
          "managed table layout hash {:#018x} differs from {:#018x}",
          api.layout_hash,
          static_cast<u64>(CONJURE_NATIVE_API_HASH)
        );
    }

    const bool complete = api.load_game_assembly != nullptr && api.unload_game_assembly != nullptr &&
      api.create_instances != nullptr && api.destroy_instances != nullptr && api.run_phase != nullptr &&
      api.dispatch_collisions != nullptr && api.apply_fields != nullptr && api.collect_fields != nullptr &&
      api.collect_garbage != nullptr && api.shutdown != nullptr;
    if (!complete) {
        return core::make_error(script_error::ABI_MISMATCH, "managed table has an unfilled function");
    }
    return core::result<void>::success();
}
