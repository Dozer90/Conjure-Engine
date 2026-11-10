#pragma once

#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_host.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// The C++ fake of the managed side (docs/design/03_scripting.md D13, 7). It implements conjure_managed_api
// with recording functions: slots come from a LIFO free list, apply_fields stores the blob per slot and
// collect_fields echoes it, run_phase and dispatch_collisions are logged, load_game_assembly registers
// fake classes (add_fake_class and an optional <assembly>.classes.json fixture) through the native table
// so the registry, blob and reload paths all run without .NET. Every scripting, scene, engine and editor
// test uses it; one null host is active (initialized) at a time.

namespace conjure::scripting {

    /// <summary>
    /// What the null host recorded. Cleared by null_script_host::reset_recorder().
    /// </summary>
    class null_host_recorder {
        public:
            struct create_record {
                u64 entity = 0;
                u64 classHash = 0;
                i32 slot = -1;
                u8 flags = 0;
                std::vector<u8> fields = {};
            };

            struct destroy_record {
                i32 slot = -1;
                bool callOnDestroy = false;
            };

            struct phase_record {
                script_phase phase = script_phase::COUNT;
                f32 deltaTime = 0.0f;
                u32 liveSlots = 0;
            };

            struct collision_record {
                i32 slot = -1;
                u64 other = 0;
                collision_kind kind = collision_kind::COUNT;
                f32 impulse = 0.0f;
            };

            struct apply_record {
                i32 slot = -1;
                std::vector<u8> blob = {};
            };

        private:
            std::vector<std::string> mCalls = {};
            std::vector<create_record> mCreates = {};
            std::vector<destroy_record> mDestroys = {};
            std::vector<phase_record> mPhases = {};
            std::vector<collision_record> mCollisions = {};
            std::vector<apply_record> mApplies = {};
            std::vector<std::string> mLoadedAssemblies = {};
            u32 mUnloadCount = 0;
            u32 mCollectGarbageCount = 0;
            u32 mShutdownCount = 0;
            u32 mCollectFieldsCount = 0;

        public:
            null_host_recorder();
            ~null_host_recorder();

            null_host_recorder(const null_host_recorder &) = default;
            null_host_recorder &operator=(const null_host_recorder &) = default;

        public:
            /// <summary>
            /// Every managed_api function called, in order, by name ("run_phase", "create_instances", ...).
            /// </summary>
            [[nodiscard]] const std::vector<std::string> &calls() const noexcept {
                return mCalls;
            }

            [[nodiscard]] const std::vector<create_record> &creates() const noexcept {
                return mCreates;
            }

            [[nodiscard]] const std::vector<destroy_record> &destroys() const noexcept {
                return mDestroys;
            }

            [[nodiscard]] const std::vector<phase_record> &phases() const noexcept {
                return mPhases;
            }

            [[nodiscard]] const std::vector<collision_record> &collisions() const noexcept {
                return mCollisions;
            }

            [[nodiscard]] const std::vector<apply_record> &applies() const noexcept {
                return mApplies;
            }

            /// <summary>
            /// Paths handed to load_game_assembly, in order.
            /// </summary>
            [[nodiscard]] const std::vector<std::string> &loaded_assemblies() const noexcept {
                return mLoadedAssemblies;
            }

            [[nodiscard]] u32 unload_count() const noexcept {
                return mUnloadCount;
            }

            [[nodiscard]] u32 collect_garbage_count() const noexcept {
                return mCollectGarbageCount;
            }

            [[nodiscard]] u32 shutdown_count() const noexcept {
                return mShutdownCount;
            }

            [[nodiscard]] u32 collect_fields_count() const noexcept {
                return mCollectFieldsCount;
            }

            /// <summary>
            /// Number of calls with this name.
            /// </summary>
            [[nodiscard]] u32 count(std::string_view call) const noexcept;

            /// <summary>
            /// Forgets everything.
            /// </summary>
            void clear();

            /*---------------------------------------------------------------*/
            // Recording (called by the null host)

            void record_call(std::string_view call);
            void record_create(const create_record &record);
            void record_destroy(const destroy_record &record);
            void record_phase(const phase_record &record);
            void record_collision(const collision_record &record);
            void record_apply(const apply_record &record);
            void record_load(std::string path);
            void record_unload();
            void record_collect_garbage();
            void record_shutdown();
            void record_collect_fields();
    };

    class null_script_host final : public script_host {
        public:
            /// <summary>
            /// Runs inside create_instances for every request once its slot is assigned: what OnCreate would do.
            /// </summary>
            using create_hook = std::function<void(const conjure_create_request &request, i32 slot)>;

            /// <summary>
            /// Runs inside destroy_instances for every live slot before it is freed when callOnDestroy is set:
            /// what OnDestroy would do.
            /// </summary>
            using destroy_hook = std::function<void(i32 slot)>;

            /// <summary>
            /// Runs inside run_phase after the counters were bumped: what the phase callbacks would do.
            /// </summary>
            using phase_hook = std::function<void(script_phase phase, f32 deltaTime)>;

            /// <summary>
            /// Runs inside dispatch_collisions for every record: what OnCollisionEnter and friends would do.
            /// </summary>
            using collision_hook = std::function<void(const conjure_collision_record &record)>;

        private:
            struct slot_state {
                bool live = false;
                u64 entity = 0;
                u64 classHash = 0;
                bool started = false;
                std::vector<u8> fields = {};
                std::optional<std::vector<u8>> collectOverride = std::nullopt;
                std::array<u32, 3> phaseCounts = {};
            };

            conjure_managed_api mManagedApi = {};
            conjure_native_api *mNativeApi = nullptr;
            script_host_status mStatus = script_host_status::UNINITIALIZED;
            null_host_recorder mRecorder = {};
            std::vector<slot_state> mSlots = {};
            std::vector<i32> mFreeList = {};               // LIFO
            std::vector<script_class_info> mFakeClasses = {};
            std::string mLoadedAssembly = {};
            i32 mLoadResult = 0;
            i32 mUnloadResult = 0;
            bool mAssemblyLoaded = false;
            create_hook mCreateHook = {};
            destroy_hook mDestroyHook = {};
            phase_hook mPhaseHook = {};
            collision_hook mCollisionHook = {};

        public:
            null_script_host();
            ~null_script_host() override;

        public:
            /// <summary>
            /// Fills the managed table with the recording functions and becomes the active null host. Never
            /// fails; a second active null host in the process asserts and replaces the first.
            /// </summary>
            [[nodiscard]] core::result<void> initialize(
              const script_host_config &config,
              conjure_native_api &nativeApi
            ) override;

            /// <summary>
            /// Frees every slot, forgets the loaded assembly, deactivates. Recorded as "shutdown".
            /// </summary>
            void shutdown() override;

            [[nodiscard]] const conjure_managed_api &managed_api() const noexcept override;
            [[nodiscard]] script_host_status status() const noexcept override;
            [[nodiscard]] script_backend backend() const noexcept override;

            /*---------------------------------------------------------------*/
            // Test controls

            /// <summary>
            /// A class load_game_assembly reports to native through script_register_class/field (fields may
            /// be in any order; the registry sorts them).
            /// </summary>
            void add_fake_class(script_class_info info);

            /// <summary>
            /// Removes a fake class so the next load models an assembly that dropped it.
            /// </summary>
            bool remove_fake_class(u64 classHash);

            /// <summary>
            /// Forgets every fake class.
            /// </summary>
            void clear_fake_classes();

            /// <summary>
            /// The value load_game_assembly returns (0 = success; a script_error code models a failed load).
            /// </summary>
            void set_load_result(i32 result) noexcept;

            /// <summary>
            /// The value unload_game_assembly returns: 0 collected, 1 leaked.
            /// </summary>
            void set_unload_result(i32 result) noexcept;

            /// <summary>
            /// Bytes collect_fields returns for a slot instead of echoing the last apply_fields blob.
            /// </summary>
            void set_collect_override(i32 slot, std::vector<u8> blob);

            /// <summary>
            /// Callbacks standing in for managed script code (empty = none). They run on the main thread
            /// inside the managed_api call and may call back into the runtime like a script would: add
            /// scripts, destroy entities, open query scopes through the native table.
            /// </summary>
            void set_create_hook(create_hook hook);
            void set_destroy_hook(destroy_hook hook);
            void set_phase_hook(phase_hook hook);
            void set_collision_hook(collision_hook hook);

            /// <summary>
            /// Marks a slot started (as the managed side does after OnStart) so a test can inspect the flag.
            /// </summary>
            [[nodiscard]] bool is_slot_started(i32 slot) const noexcept;

            [[nodiscard]] bool is_slot_live(i32 slot) const noexcept;

            /// <summary>
            /// Entity recorded for a live slot (0 otherwise).
            /// </summary>
            [[nodiscard]] u64 slot_entity(i32 slot) const noexcept;

            [[nodiscard]] u64 slot_class(i32 slot) const noexcept;

            /// <summary>
            /// Blob last applied to a live slot (empty otherwise).
            /// </summary>
            [[nodiscard]] const std::vector<u8> &slot_fields(i32 slot) const noexcept;

            /// <summary>
            /// How often a live slot took part in run_phase(phase).
            /// </summary>
            [[nodiscard]] u32 slot_phase_count(i32 slot, script_phase phase) const noexcept;

            [[nodiscard]] u32 live_slot_count() const noexcept;

            /// <summary>
            /// Slots on the free list, most recently freed first.
            /// </summary>
            [[nodiscard]] const std::vector<i32> &free_list() const noexcept {
                return mFreeList;
            }

            [[nodiscard]] const null_host_recorder &recorder() const noexcept {
                return mRecorder;
            }

            void reset_recorder();

            [[nodiscard]] bool is_assembly_loaded() const noexcept {
                return mAssemblyLoaded;
            }

            [[nodiscard]] const std::string &loaded_assembly() const noexcept {
                return mLoadedAssembly;
            }

            /// <summary>
            /// The null host whose functions the managed table currently calls, or nullptr.
            /// </summary>
            [[nodiscard]] static null_script_host *active() noexcept;

            /// <summary>
            /// Parses a <assembly>.classes.json fixture:
            /// {"classes":[{"name","callbacks":["UPDATE",...],
            ///   "fields":[{"name","kind","elementKind","default","meta"}]}]}.
            /// </summary>
            [[nodiscard]] static core::result<std::vector<script_class_info>> parse_classes_fixture(
              std::string_view jsonText
            );

        private:
            /*---------------------------------------------------------------*/
            // The managed_api implementation (bound through the active host)

            [[nodiscard]] i32 load_game_assembly(std::string_view path);
            [[nodiscard]] i32 unload_game_assembly();
            [[nodiscard]] i32 create_instances(conjure_create_request *requests, i32 count);
            [[nodiscard]] i32 destroy_instances(const i32 *slots, i32 count, i32 callOnDestroy);
            [[nodiscard]] i32 run_phase(u8 phase, f32 deltaTime);
            [[nodiscard]] i32 dispatch_collisions(const conjure_collision_record *records, i32 count);
            [[nodiscard]] i32 apply_fields(i32 slot, const u8 *blob, i32 size);
            [[nodiscard]] i32 collect_fields(i32 slot, u8 *buffer, i32 capacity);
            [[nodiscard]] i32 collect_garbage();
            [[nodiscard]] i32 shutdown_managed();

            void register_classes_with_native(const std::vector<script_class_info> &classes);
            [[nodiscard]] i32 allocate_slot();
            void free_slot(i32 slot);

            static i32 trampoline_load_game_assembly(const char *utf8Path, i32 length);
            static i32 trampoline_unload_game_assembly();
            static i32 trampoline_create_instances(conjure_create_request *requests, i32 count);
            static i32 trampoline_destroy_instances(const i32 *slots, i32 count, i32 callOnDestroy);
            static i32 trampoline_run_phase(u8 phase, f32 deltaTime);
            static i32 trampoline_dispatch_collisions(const conjure_collision_record *records, i32 count);
            static i32 trampoline_apply_fields(i32 slot, const u8 *blob, i32 size);
            static i32 trampoline_collect_fields(i32 slot, u8 *buffer, i32 capacity);
            static i32 trampoline_collect_garbage();
            static i32 trampoline_shutdown();
    };
}
