#include "conjure/core/reflect/type_registry.h"

#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/reflect/type_info.h"
#include "pointer_index.h"

#include "conjure/core/error.h"
#include "conjure/core/event.h"
#include "conjure/core/log.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <format>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect;

/*===========================================================================*/
/*===========================================================================*/
// State

struct type_registry::state {
    // Guards every mutation. Lookups never take it: descriptions never move (unique_ptr storage that
    // is only ever appended to and kept until destruction) and the indexes are lock-free.
    std::mutex mutex = {};

    // Serialises event emission so handlers never run concurrently and so struct mutations are
    // emitted in the order they took effect: register_type/unregister_type take it before 'mutex'
    // and keep it until their handlers have run (lock order: eventMutex -> mutex, never the reverse).
    // Recursive because a handler may register further types, which emits again on the same thread.
    std::recursive_mutex eventMutex = {};

    std::vector<std::unique_ptr<reflect::type_info>> typeStorage = {};   // every description ever registered
    std::vector<std::unique_ptr<enum_info>> enumStorage = {};
    std::vector<const reflect::type_info *> liveTypes = {};              // registration order, live only
    std::vector<const enum_info *> liveEnums = {};
    detail::pointer_index typeIndex = {};
    detail::pointer_index enumIndex = {};
    event<const reflect::type_info &> onRegistered = {};
    event<const reflect::type_info &> onUnregistered = {};
};

namespace {
    result<void> validate_enum(const enum_info &info) {
        if (info.name.empty()) {
            return error(error_code::INVALID_ARGUMENT, "enum has no name");
        }
        const u32 size = info.underlyingSize;
        if (!is_valid_enum_size(size)) {
            return error(
              error_code::INVALID_ARGUMENT,
              std::format("enum '{}': underlying size {} is not 1, 2, 4 or 8", info.name, size)
            );
        }
        for (usize index = 0; index < info.values.size(); ++index) {
            const std::pair<std::string, i64> &entry = info.values[index];
            if (entry.first.empty()) {
                return error(
                  error_code::INVALID_ARGUMENT,
                  std::format("enum '{}': enumerator {} has no name", info.name, index)
                );
            }
            if (!info.is_representable(entry.second)) {
                return error(
                  error_code::INVALID_ARGUMENT,
                  std::format(
                    "enum '{}': enumerator '{}' = {} does not fit {} bytes",
                    info.name,
                    entry.first,
                    entry.second,
                    size
                  )
                );
            }
            for (usize other = index + 1; other < info.values.size(); ++other) {
                if (info.values[other].first == entry.first) {
                    return error(
                      error_code::INVALID_ARGUMENT,
                      std::format("enum '{}': enumerator '{}' is declared twice", info.name, entry.first)
                    );
                }
            }
        }
        return result<void>::success();
    }

    /// <summary>
    /// The first live struct whose fields hold a value of the given kind and type id, or nullptr.
    /// Reports the field through 'referencingField'.
    /// </summary>
    const reflect::type_info *find_reference(
      std::span<const reflect::type_info *const> types,
      field_kind valueKind,
      type_id id,
      const field_info *&referencingField
    ) noexcept {
        for (const reflect::type_info *type : types) {
            for (const field_info &field : type->fields) {
                if (field.value_kind() == valueKind && field.type == id) {
                    referencingField = &field;
                    return type;
                }
            }
        }
        return nullptr;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

type_registry::type_registry() :
    mState(std::make_unique<state>()) {}

type_registry::~type_registry() {}

/*===========================================================================*/
/*===========================================================================*/
// Registration

result<const reflect::type_info *> type_registry::register_type(reflect::type_info info) {
    CONJURE_TRY(validate_layout(info));

    const type_id expected = type_id::from_name(info.name);
    if (info.type.is_null()) {
        info.type = expected;
    } else if (info.type != expected) {
        return error(
          error_code::INVALID_ARGUMENT,
          std::format("type '{}': id {} does not match its name (expected {})", info.name, info.type, expected)
        );
    }

    // The event lock is taken first and held through the emit so this registration and any
    // concurrent unregistration are reported in the order they took effect.
    const std::scoped_lock eventLock(mState->eventMutex);
    const reflect::type_info *registered = nullptr;
    {
        const std::scoped_lock lock(mState->mutex);

        // Nested descriptions must be live members of this registry: a pointer at a stack-local or
        // foreign type_info would dangle, and find_reference() relies on field.type naming it.
        for (const field_info &field : info.fields) {
            const field_kind valueKind = field.value_kind();
            if (valueKind == field_kind::STRUCT && mState->typeIndex.find(field.type.value) != field.structType) {
                return error(
                  error_code::INVALID_ARGUMENT,
                  std::format(
                    "type '{}': field '{}' points at a struct description that is not registered here",
                    info.name,
                    field.name
                  )
                );
            }
            if (valueKind == field_kind::ENUM && mState->enumIndex.find(field.type.value) != field.enumType) {
                return error(
                  error_code::INVALID_ARGUMENT,
                  std::format(
                    "type '{}': field '{}' points at an enum description that is not registered here",
                    info.name,
                    field.name
                  )
                );
            }
        }

        const void *found = mState->typeIndex.find(info.type.value);
        const reflect::type_info *existing = static_cast<const reflect::type_info *>(found);
        if (existing != nullptr) {
            if (existing->name != info.name) {
                return error(
                  error_code::ALREADY_EXISTS,
                  std::format("type '{}' hashes to the same id as registered type '{}'", info.name, existing->name)
                );
            }
            if (!existing->has_same_layout(info)) {
                return error(
                  error_code::ALREADY_EXISTS,
                  std::format("type '{}' is already registered with a different layout", info.name)
                );
            }
            return existing;
        }

        mState->typeStorage.push_back(std::make_unique<reflect::type_info>(std::move(info)));
        registered = mState->typeStorage.back().get();
        mState->liveTypes.push_back(registered);
        mState->typeIndex.insert(registered->type.value, registered);
    }

    mState->onRegistered.emit(*registered);
    return registered;
}

result<const enum_info *> type_registry::register_enum(enum_info info) {
    CONJURE_TRY(validate_enum(info));

    const type_id expected = type_id::from_name(info.name);
    if (info.type.is_null()) {
        info.type = expected;
    } else if (info.type != expected) {
        return error(
          error_code::INVALID_ARGUMENT,
          std::format("enum '{}': id {} does not match its name (expected {})", info.name, info.type, expected)
        );
    }

    const std::scoped_lock lock(mState->mutex);

    const enum_info *existing = static_cast<const enum_info *>(mState->enumIndex.find(info.type.value));
    if (existing != nullptr) {
        if (*existing == info) {
            return existing;
        }
        return error(
          error_code::ALREADY_EXISTS,
          std::format("enum '{}' is already registered with a different description", info.name)
        );
    }

    mState->enumStorage.push_back(std::make_unique<enum_info>(std::move(info)));
    const enum_info *registered = mState->enumStorage.back().get();
    mState->liveEnums.push_back(registered);
    mState->enumIndex.insert(registered->type.value, registered);
    return registered;
}

result<void> type_registry::unregister_type(type_id id) {
    const std::scoped_lock eventLock(mState->eventMutex);
    const reflect::type_info *removed = nullptr;
    {
        const std::scoped_lock lock(mState->mutex);

        removed = static_cast<const reflect::type_info *>(mState->typeIndex.find(id.value));
        if (removed == nullptr) {
            return error(error_code::NOT_FOUND, std::format("no type with id {} is registered", id));
        }

        const field_info *referencingField = nullptr;
        const reflect::type_info *referrer =
          find_reference(mState->liveTypes, field_kind::STRUCT, id, referencingField);
        if (referrer != nullptr) {
            return error(
              error_code::INVALID_STATE,
              std::format(
                "type '{}' is referenced by field '{}.{}'",
                removed->name,
                referrer->name,
                referencingField->name
              )
            );
        }

        std::erase(mState->liveTypes, removed);
        mState->typeIndex.erase(id.value);
    }

    mState->onUnregistered.emit(*removed);
    return result<void>::success();
}

result<void> type_registry::unregister_enum(type_id id) {
    const std::scoped_lock lock(mState->mutex);

    const enum_info *removed = static_cast<const enum_info *>(mState->enumIndex.find(id.value));
    if (removed == nullptr) {
        return error(error_code::NOT_FOUND, std::format("no enum with id {} is registered", id));
    }

    const field_info *referencingField = nullptr;
    const reflect::type_info *referrer = find_reference(mState->liveTypes, field_kind::ENUM, id, referencingField);
    if (referrer != nullptr) {
        return error(
          error_code::INVALID_STATE,
          std::format("enum '{}' is referenced by field '{}.{}'", removed->name, referrer->name, referencingField->name)
        );
    }

    std::erase(mState->liveEnums, removed);
    mState->enumIndex.erase(id.value);
    return result<void>::success();
}

/*===========================================================================*/
/*===========================================================================*/
// Lookup

const reflect::type_info *type_registry::find(type_id id) const noexcept {
    return static_cast<const reflect::type_info *>(mState->typeIndex.find(id.value));
}

const reflect::type_info *type_registry::find(std::string_view name) const noexcept {
    const reflect::type_info *candidate = find(type_id::from_name(name));
    if (candidate == nullptr || candidate->name != name) {
        return nullptr;
    }
    return candidate;
}

const enum_info *type_registry::find_enum(type_id id) const noexcept {
    return static_cast<const enum_info *>(mState->enumIndex.find(id.value));
}

const enum_info *type_registry::find_enum(std::string_view name) const noexcept {
    const enum_info *candidate = find_enum(type_id::from_name(name));
    if (candidate == nullptr || candidate->name != name) {
        return nullptr;
    }
    return candidate;
}

std::span<const reflect::type_info *const> type_registry::types() const noexcept {
    return std::span<const reflect::type_info *const>(mState->liveTypes);
}

std::span<const enum_info *const> type_registry::enums() const noexcept {
    return std::span<const enum_info *const>(mState->liveEnums);
}

usize type_registry::type_count() const noexcept {
    const std::scoped_lock lock(mState->mutex);
    return mState->liveTypes.size();
}

usize type_registry::enum_count() const noexcept {
    const std::scoped_lock lock(mState->mutex);
    return mState->liveEnums.size();
}

event<const reflect::type_info &> &type_registry::on_registered() noexcept {
    return mState->onRegistered;
}

event<const reflect::type_info &> &type_registry::on_unregistered() noexcept {
    return mState->onUnregistered;
}

/*===========================================================================*/
/*===========================================================================*/
// Process-wide registry

type_registry &conjure::core::reflect::registry() noexcept {
    // Function-local so it exists before any static initialiser or lazily registering function
    // touches it, whatever the translation unit order.
    static type_registry sRegistry = {};
    return sRegistry;
}

const reflect::type_info *conjure::core::reflect::detail::register_described_type(reflect::type_info info) noexcept {
    const std::string name = info.name;
    result<const reflect::type_info *> registered = registry().register_type(std::move(info));
    if (!registered.has_value()) {
        CONJURE_LOG_ERROR(LOG_CORE, "reflect: cannot register type '{}': {}", name, registered.get_error());
        return nullptr;
    }
    return registered.value();
}

const enum_info *conjure::core::reflect::detail::register_described_enum(enum_info info) noexcept {
    const std::string name = info.name;
    result<const enum_info *> registered = registry().register_enum(std::move(info));
    if (!registered.has_value()) {
        CONJURE_LOG_ERROR(LOG_CORE, "reflect: cannot register enum '{}': {}", name, registered.get_error());
        return nullptr;
    }
    return registered.value();
}
