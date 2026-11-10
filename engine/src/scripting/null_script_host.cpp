#include "conjure/scripting/null_script_host.h"

#include "conjure/scripting/log_category.h"
#include "conjure/scripting/scripting_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/vfs/native_file_system.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

using namespace conjure;
using namespace conjure::scripting;

namespace {

    null_script_host *sActiveHost = nullptr;

    const std::vector<u8> EMPTY_BLOB = {};

    [[nodiscard]] std::string json_text(const core::json::value &value) {
        if (!value.is_valid()) {
            return {};
        }
        return core::json::document::from_value(value).to_canonical_string();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// null_host_recorder

null_host_recorder::null_host_recorder() {}

null_host_recorder::~null_host_recorder() {}

u32 null_host_recorder::count(std::string_view call) const noexcept {
    return static_cast<u32>(std::count(mCalls.begin(), mCalls.end(), call));
}

void null_host_recorder::clear() {
    *this = null_host_recorder();
}

void null_host_recorder::record_call(std::string_view call) {
    mCalls.emplace_back(call);
}

void null_host_recorder::record_create(const create_record &record) {
    mCreates.push_back(record);
}

void null_host_recorder::record_destroy(const destroy_record &record) {
    mDestroys.push_back(record);
}

void null_host_recorder::record_phase(const phase_record &record) {
    mPhases.push_back(record);
}

void null_host_recorder::record_collision(const collision_record &record) {
    mCollisions.push_back(record);
}

void null_host_recorder::record_apply(const apply_record &record) {
    mApplies.push_back(record);
}

void null_host_recorder::record_load(std::string path) {
    mLoadedAssemblies.push_back(std::move(path));
}

void null_host_recorder::record_unload() {
    ++mUnloadCount;
}

void null_host_recorder::record_collect_garbage() {
    ++mCollectGarbageCount;
}

void null_host_recorder::record_shutdown() {
    ++mShutdownCount;
}

void null_host_recorder::record_collect_fields() {
    ++mCollectFieldsCount;
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

null_script_host::null_script_host() {}

null_script_host::~null_script_host() {
    if (sActiveHost == this) {
        sActiveHost = nullptr;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// script_host

core::result<void> null_script_host::initialize(const script_host_config &config, conjure_native_api &nativeApi) {
    CONJURE_UNUSED(config);
    CONJURE_ASSERT(sActiveHost == nullptr || sActiveHost == this, "a second null_script_host is being initialised");
    sActiveHost = this;
    mNativeApi = &nativeApi;

    mManagedApi = {};
    mManagedApi.size = static_cast<uint32_t>(sizeof(conjure_managed_api));
    mManagedApi.version = CONJURE_NATIVE_API_VERSION;
    mManagedApi.layout_hash = CONJURE_NATIVE_API_HASH;
    mManagedApi.load_game_assembly = &trampoline_load_game_assembly;
    mManagedApi.unload_game_assembly = &trampoline_unload_game_assembly;
    mManagedApi.create_instances = &trampoline_create_instances;
    mManagedApi.destroy_instances = &trampoline_destroy_instances;
    mManagedApi.run_phase = &trampoline_run_phase;
    mManagedApi.dispatch_collisions = &trampoline_dispatch_collisions;
    mManagedApi.apply_fields = &trampoline_apply_fields;
    mManagedApi.collect_fields = &trampoline_collect_fields;
    mManagedApi.collect_garbage = &trampoline_collect_garbage;
    mManagedApi.shutdown = &trampoline_shutdown;
    mStatus = script_host_status::READY;
    mRecorder.record_call("initialize");
    return core::result<void>::success();
}

void null_script_host::shutdown() {
    if (mStatus == script_host_status::READY) {
        static_cast<void>(shutdown_managed());
    }
    mStatus = script_host_status::UNINITIALIZED;
    if (sActiveHost == this) {
        sActiveHost = nullptr;
    }
}

const conjure_managed_api &null_script_host::managed_api() const noexcept {
    return mManagedApi;
}

script_host_status null_script_host::status() const noexcept {
    return mStatus;
}

script_backend null_script_host::backend() const noexcept {
    return script_backend::NULL_HOST;
}


/*===========================================================================*/
/*===========================================================================*/
// Test controls

void null_script_host::add_fake_class(script_class_info info) {
    info.sort_fields();
    for (script_class_info &existing : mFakeClasses) {
        if (existing.hash == info.hash) {
            existing = std::move(info);
            return;
        }
    }
    mFakeClasses.push_back(std::move(info));
}

bool null_script_host::remove_fake_class(u64 classHash) {
    const auto sameHash = [classHash](const script_class_info &info) {
        return info.hash == classHash;
    };
    const auto found = std::find_if(mFakeClasses.begin(), mFakeClasses.end(), sameHash);
    if (found == mFakeClasses.end()) {
        return false;
    }
    mFakeClasses.erase(found);
    return true;
}

void null_script_host::clear_fake_classes() {
    mFakeClasses.clear();
}

void null_script_host::set_load_result(i32 result) noexcept {
    mLoadResult = result;
}

void null_script_host::set_unload_result(i32 result) noexcept {
    mUnloadResult = result;
}

void null_script_host::set_collect_override(i32 slot, std::vector<u8> blob) {
    if (slot < 0 || static_cast<usize>(slot) >= mSlots.size()) {
        return;
    }
    mSlots[static_cast<usize>(slot)].collectOverride = std::move(blob);
}

void null_script_host::set_create_hook(create_hook hook) {
    mCreateHook = std::move(hook);
}

void null_script_host::set_destroy_hook(destroy_hook hook) {
    mDestroyHook = std::move(hook);
}

void null_script_host::set_phase_hook(phase_hook hook) {
    mPhaseHook = std::move(hook);
}

void null_script_host::set_collision_hook(collision_hook hook) {
    mCollisionHook = std::move(hook);
}

bool null_script_host::is_slot_started(i32 slot) const noexcept {
    return is_slot_live(slot) && mSlots[static_cast<usize>(slot)].started;
}

bool null_script_host::is_slot_live(i32 slot) const noexcept {
    return slot >= 0 && static_cast<usize>(slot) < mSlots.size() && mSlots[static_cast<usize>(slot)].live;
}

u64 null_script_host::slot_entity(i32 slot) const noexcept {
    return is_slot_live(slot) ? mSlots[static_cast<usize>(slot)].entity : 0;
}

u64 null_script_host::slot_class(i32 slot) const noexcept {
    return is_slot_live(slot) ? mSlots[static_cast<usize>(slot)].classHash : 0;
}

const std::vector<u8> &null_script_host::slot_fields(i32 slot) const noexcept {
    return is_slot_live(slot) ? mSlots[static_cast<usize>(slot)].fields : EMPTY_BLOB;
}

u32 null_script_host::slot_phase_count(i32 slot, script_phase phase) const noexcept {
    if (!is_slot_live(slot) || phase >= script_phase::COUNT) {
        return 0;
    }
    return mSlots[static_cast<usize>(slot)].phaseCounts[static_cast<usize>(phase)];
}

u32 null_script_host::live_slot_count() const noexcept {
    u32 count = 0;
    for (const slot_state &slot : mSlots) {
        if (slot.live) {
            ++count;
        }
    }
    return count;
}

void null_script_host::reset_recorder() {
    mRecorder.clear();
}

null_script_host *null_script_host::active() noexcept {
    return sActiveHost;
}

core::result<std::vector<script_class_info>> null_script_host::parse_classes_fixture(std::string_view jsonText) {
    CONJURE_TRY_ASSIGN(const core::json::document document, core::json::document::parse(jsonText));
    const core::json::value classes = document.root()["classes"];
    if (!classes.is_array()) {
        return core::error(core::error_code::PARSE_FAILURE, "classes fixture needs a \"classes\" array");
    }

    std::vector<script_class_info> result = {};
    for (const core::json::value classValue : classes) {
        const std::string_view name = classValue["name"].as_string();
        if (name.empty()) {
            return core::error(core::error_code::PARSE_FAILURE, "classes fixture: a class has no \"name\"");
        }
        script_class_info info = {};
        info.fullName = std::string(name);
        info.hash = script_name_hash(name);
        for (const core::json::value callbackValue : classValue["callbacks"]) {
            const script_callback callback = parse_script_callback(callbackValue.as_string());
            if (callback == script_callback::COUNT) {
                return core::error(
                  core::error_code::PARSE_FAILURE,
                  "classes fixture: unknown callback '" + std::string(callbackValue.as_string()) + "'"
                );
            }
            info.callbackMask |= callback_bit(callback);
        }
        for (const core::json::value fieldValue : classValue["fields"]) {
            const std::string_view fieldName = fieldValue["name"].as_string();
            const script_field_kind kind = parse_script_field_kind(fieldValue["kind"].as_string("INT32"));
            if (fieldName.empty() || kind == script_field_kind::COUNT) {
                return core::error(
                  core::error_code::PARSE_FAILURE,
                  "classes fixture: field of '" + info.fullName + "' needs \"name\" and a known \"kind\""
                );
            }
            script_field_info field = {};
            field.name = std::string(fieldName);
            field.hash = script_name_hash(fieldName);
            field.kind = kind;
            field.elementKind = parse_script_field_kind(fieldValue["elementKind"].as_string("INT32"));
            if (field.elementKind == script_field_kind::COUNT) {
                field.elementKind = script_field_kind::INT32;
            }
            if (fieldValue["default"].is_valid()) {
                field.defaultValue = core::json::document::from_value(fieldValue["default"]);
            }
            if (fieldValue["meta"].is_valid()) {
                field.meta = core::json::document::from_value(fieldValue["meta"]);
            }
            info.fields.push_back(std::move(field));
        }
        info.sort_fields();
        result.push_back(std::move(info));
    }
    return result;
}


/*===========================================================================*/
/*===========================================================================*/
// The managed_api implementation

i32 null_script_host::load_game_assembly(std::string_view path) {
    mRecorder.record_call("load_game_assembly");
    mRecorder.record_load(std::string(path));
    if (mLoadResult != 0) {
        return mLoadResult;
    }

    std::vector<script_class_info> classes = mFakeClasses;
    if (!path.empty()) {
        const std::filesystem::path assemblyPath(path);
        std::error_code probe = {};
        const bool exists = std::filesystem::exists(assemblyPath, probe);
        const std::string fixtureName = assemblyPath.stem().string() + ".classes.json";
        const std::filesystem::path fixture = assemblyPath.parent_path() / fixtureName;
        const bool fixtureExists = std::filesystem::exists(fixture, probe);
        if (!exists && !fixtureExists) {
            return status_of(script_error::ASSEMBLY_LOAD_FAILED);
        }
        if (fixtureExists) {
            core::vfs::native_file_system files(fixture.parent_path(), true);
            const core::result<std::vector<std::byte>> bytes = files.read_file(fixture.filename().string());
            if (!bytes.has_value()) {
                return status_of(script_error::ASSEMBLY_LOAD_FAILED);
            }
            const std::string_view text(reinterpret_cast<const char *>(bytes.value().data()), bytes.value().size());
            core::result<std::vector<script_class_info>> parsed = parse_classes_fixture(text);
            if (!parsed.has_value()) {
                CONJURE_LOG_ERROR(LOG_SCRIPTING, "classes fixture {}: {}", fixture.string(), parsed.get_error());
                return status_of(script_error::ASSEMBLY_LOAD_FAILED);
            }
            for (script_class_info &info : parsed.value()) {
                const auto sameClass = [&](const script_class_info &known) {
                    return known.hash == info.hash;
                };
                const auto duplicate = std::find_if(classes.begin(), classes.end(), sameClass);
                if (duplicate != classes.end()) {
                    *duplicate = std::move(info);
                } else {
                    classes.push_back(std::move(info));
                }
            }
        }
    }

    register_classes_with_native(classes);
    mLoadedAssembly = std::string(path);
    mAssemblyLoaded = true;
    return 0;
}

i32 null_script_host::unload_game_assembly() {
    mRecorder.record_call("unload_game_assembly");
    mRecorder.record_unload();
    mAssemblyLoaded = false;
    mLoadedAssembly.clear();
    return mUnloadResult;
}

i32 null_script_host::create_instances(conjure_create_request *requests, i32 count) {
    mRecorder.record_call("create_instances");
    for (i32 index = 0; index < count; ++index) {
        conjure_create_request &request = requests[index];
        const i32 slot = allocate_slot();
        slot_state &state = mSlots[static_cast<usize>(slot)];
        state.entity = request.entity;
        state.classHash = request.class_hash;
        state.started = (request.flags & CREATE_FLAG_ALREADY_STARTED) != 0;
        state.fields.assign(request.fields, request.fields + request.fields_size);
        request.out_slot = slot;

        null_host_recorder::create_record record = {};
        record.entity = request.entity;
        record.classHash = request.class_hash;
        record.slot = slot;
        record.flags = request.flags;
        record.fields = state.fields;
        mRecorder.record_create(record);
        if (mCreateHook) {
            // The hook may create or destroy slots (as OnCreate may): no reference into mSlots survives it.
            mCreateHook(request, slot);
        }
    }
    return count;
}

i32 null_script_host::destroy_instances(const i32 *slots, i32 count, i32 callOnDestroy) {
    mRecorder.record_call("destroy_instances");
    i32 destroyed = 0;
    for (i32 index = 0; index < count; ++index) {
        null_host_recorder::destroy_record record = {};
        record.slot = slots[index];
        record.callOnDestroy = callOnDestroy != 0;
        mRecorder.record_destroy(record);
        if (!is_slot_live(slots[index])) {
            continue;
        }
        if (callOnDestroy != 0 && mDestroyHook) {
            mDestroyHook(slots[index]);
        }
        if (is_slot_live(slots[index])) {
            free_slot(slots[index]);
            ++destroyed;
        }
    }
    return destroyed;
}

i32 null_script_host::run_phase(u8 phase, f32 deltaTime) {
    mRecorder.record_call("run_phase");
    null_host_recorder::phase_record record = {};
    const bool knownPhase = phase < static_cast<u8>(script_phase::COUNT);
    record.phase = knownPhase ? static_cast<script_phase>(phase) : script_phase::COUNT;
    record.deltaTime = deltaTime;
    if (record.phase == script_phase::COUNT) {
        mRecorder.record_phase(record);
        return status_of(script_error::NOT_SUPPORTED);
    }
    for (slot_state &state : mSlots) {
        if (!state.live) {
            continue;
        }
        if (record.phase != script_phase::LATE_UPDATE) {
            state.started = true;
        }
        ++state.phaseCounts[phase];
        ++record.liveSlots;
    }
    mRecorder.record_phase(record);
    if (mPhaseHook) {
        mPhaseHook(record.phase, deltaTime);
    }
    return static_cast<i32>(record.liveSlots);
}

i32 null_script_host::dispatch_collisions(const conjure_collision_record *records, i32 count) {
    mRecorder.record_call("dispatch_collisions");
    for (i32 index = 0; index < count; ++index) {
        null_host_recorder::collision_record record = {};
        record.slot = records[index].slot;
        record.other = records[index].other;
        record.impulse = records[index].impulse;
        record.kind = records[index].kind < static_cast<u8>(collision_kind::COUNT)
          ? static_cast<collision_kind>(records[index].kind)
          : collision_kind::COUNT;
        mRecorder.record_collision(record);
        if (mCollisionHook) {
            mCollisionHook(records[index]);
        }
    }
    return count;
}

i32 null_script_host::apply_fields(i32 slot, const u8 *blob, i32 size) {
    mRecorder.record_call("apply_fields");
    null_host_recorder::apply_record record = {};
    record.slot = slot;
    record.blob.assign(blob, blob + size);
    mRecorder.record_apply(record);
    if (!is_slot_live(slot)) {
        return status_of(script_error::INSTANCE_NOT_FOUND);
    }
    mSlots[static_cast<usize>(slot)].fields = record.blob;
    return 0;
}

i32 null_script_host::collect_fields(i32 slot, u8 *buffer, i32 capacity) {
    mRecorder.record_call("collect_fields");
    mRecorder.record_collect_fields();
    if (!is_slot_live(slot)) {
        return status_of(script_error::INSTANCE_NOT_FOUND);
    }
    const slot_state &state = mSlots[static_cast<usize>(slot)];
    const std::vector<u8> &source = state.collectOverride.has_value() ? *state.collectOverride : state.fields;
    const i32 needed = static_cast<i32>(source.size());
    if (needed <= capacity && buffer != nullptr) {
        std::memcpy(buffer, source.data(), source.size());
    }
    return needed;
}

i32 null_script_host::collect_garbage() {
    mRecorder.record_call("collect_garbage");
    mRecorder.record_collect_garbage();
    return 0;
}

i32 null_script_host::shutdown_managed() {
    mRecorder.record_call("shutdown");
    mRecorder.record_shutdown();
    mSlots.clear();
    mFreeList.clear();
    mAssemblyLoaded = false;
    mLoadedAssembly.clear();
    return 0;
}


/*===========================================================================*/
/*===========================================================================*/
// Private helpers

void null_script_host::register_classes_with_native(const std::vector<script_class_info> &classes) {
    if (mNativeApi == nullptr || mNativeApi->script_register_class == nullptr) {
        return;
    }
    for (const script_class_info &info : classes) {
        conjure_script_class_desc desc = {};
        desc.hash = info.hash;
        desc.full_name = info.fullName.c_str();
        desc.name_length = static_cast<int32_t>(info.fullName.size());
        desc.callback_mask = info.callbackMask;
        desc.field_count = static_cast<uint16_t>(info.fields.size());
        mNativeApi->script_register_class(&desc);

        if (mNativeApi->script_register_field == nullptr) {
            continue;
        }
        for (const script_field_info &field : info.fields) {
            const std::string defaultText = json_text(field.defaultValue.root());
            const std::string metaText = json_text(field.meta.root());
            conjure_script_field_desc fieldDesc = {};
            fieldDesc.hash = field.hash;
            fieldDesc.name = field.name.c_str();
            fieldDesc.name_length = static_cast<int32_t>(field.name.size());
            fieldDesc.kind = static_cast<uint8_t>(field.kind);
            fieldDesc.element_kind = static_cast<uint8_t>(field.elementKind);
            fieldDesc.default_json = defaultText.c_str();
            fieldDesc.default_length = static_cast<int32_t>(defaultText.size());
            fieldDesc.meta_json = metaText.c_str();
            fieldDesc.meta_length = static_cast<int32_t>(metaText.size());
            mNativeApi->script_register_field(info.hash, &fieldDesc);
        }
    }
}

i32 null_script_host::allocate_slot() {
    i32 slot = 0;
    if (!mFreeList.empty()) {
        slot = mFreeList.back();
        mFreeList.pop_back();
    } else {
        slot = static_cast<i32>(mSlots.size());
        mSlots.emplace_back();
    }
    slot_state &state = mSlots[static_cast<usize>(slot)];
    state = {};
    state.live = true;
    return slot;
}

void null_script_host::free_slot(i32 slot) {
    slot_state &state = mSlots[static_cast<usize>(slot)];
    state = {};
    mFreeList.push_back(slot);
}


/*===========================================================================*/
/*===========================================================================*/
// Trampolines

i32 null_script_host::trampoline_load_game_assembly(const char *utf8Path, i32 length) {
    if (sActiveHost == nullptr) {
        return status_of(script_error::NOT_INITIALIZED);
    }
    const std::string_view path =
      utf8Path == nullptr ? std::string_view() : std::string_view(utf8Path, static_cast<usize>(length));
    return sActiveHost->load_game_assembly(path);
}

i32 null_script_host::trampoline_unload_game_assembly() {
    return sActiveHost == nullptr ? status_of(script_error::NOT_INITIALIZED) : sActiveHost->unload_game_assembly();
}

i32 null_script_host::trampoline_create_instances(conjure_create_request *requests, i32 count) {
    if (sActiveHost == nullptr) {
        return status_of(script_error::NOT_INITIALIZED);
    }
    return sActiveHost->create_instances(requests, count);
}

i32 null_script_host::trampoline_destroy_instances(const i32 *slots, i32 count, i32 callOnDestroy) {
    if (sActiveHost == nullptr) {
        return status_of(script_error::NOT_INITIALIZED);
    }
    return sActiveHost->destroy_instances(slots, count, callOnDestroy);
}

i32 null_script_host::trampoline_run_phase(u8 phase, f32 deltaTime) {
    return sActiveHost == nullptr ? status_of(script_error::NOT_INITIALIZED) : sActiveHost->run_phase(phase, deltaTime);
}

i32 null_script_host::trampoline_dispatch_collisions(const conjure_collision_record *records, i32 count) {
    if (sActiveHost == nullptr) {
        return status_of(script_error::NOT_INITIALIZED);
    }
    return sActiveHost->dispatch_collisions(records, count);
}

i32 null_script_host::trampoline_apply_fields(i32 slot, const u8 *blob, i32 size) {
    if (sActiveHost == nullptr) {
        return status_of(script_error::NOT_INITIALIZED);
    }
    return sActiveHost->apply_fields(slot, blob, size);
}

i32 null_script_host::trampoline_collect_fields(i32 slot, u8 *buffer, i32 capacity) {
    if (sActiveHost == nullptr) {
        return status_of(script_error::NOT_INITIALIZED);
    }
    return sActiveHost->collect_fields(slot, buffer, capacity);
}

i32 null_script_host::trampoline_collect_garbage() {
    return sActiveHost == nullptr ? status_of(script_error::NOT_INITIALIZED) : sActiveHost->collect_garbage();
}

i32 null_script_host::trampoline_shutdown() {
    return sActiveHost == nullptr ? status_of(script_error::NOT_INITIALIZED) : sActiveHost->shutdown_managed();
}
