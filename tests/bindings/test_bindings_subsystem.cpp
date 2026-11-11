#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "bindings_test_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/binary_asset.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/bindings_subsystem.h"
#include "conjure/bindings/call_counters.h"
#include "conjure/bindings/marshal.h"
#include "conjure/bindings/native_api_entries.h"
#include "conjure/core/string_id.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_subsystem.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene_serializer.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_runtime.h"
#include "bindings/entry_support.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

// bindings_subsystem.h (docs/design/15_bindings.md 3.1, 10): startup step 16 fills and wraps the table before
// scripting's post_startup, does nothing without scripting, survives a failed startup, clears the residency
// when a scene unloads and resets the per-frame state in begin_frame.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    /// <summary>
    /// A subsystem added after the bindings whose startup fails, so the unwind runs the bindings' shutdown.
    /// </summary>
    class failing_subsystem final : public engine::engine_subsystem {
        public:
            failing_subsystem() = default;
            ~failing_subsystem() override = default;

        public:
            [[nodiscard]] std::string_view name() const noexcept override {
                return "failing";
            }

            [[nodiscard]] core::result<void> startup(engine::engine_context &) override {
                return core::make_error(bindings_error::INTERNAL, "provoked failure");
            }

            void shutdown(engine::engine_context &) override {}
    };

    void add_binary_source(engine::application &app, const assets::asset_id &id, std::string_view path) {
        std::unique_ptr<assets::memory_asset_source> owned = std::make_unique<assets::memory_asset_source>();
        assets::cooked_blob_header header = {};
        header.type = assets::asset_type_traits<assets::binary_asset>::id();
        header.typeVersion = 1;
        header.importerVersion = 1;
        header.id = id;
        const std::string content = "bytes";
        const std::byte *begin = reinterpret_cast<const std::byte *>(content.data());
        owned->add(assets::make_cooked_blob(header, {}, std::vector<std::byte>(begin, begin + content.size())), path);
        if (app.context().assets->types().find("conjure.assets.binary") == nullptr) {
            CONJURE_REQUIRE_OK(assets::register_binary_asset_type(app.context().assets->types()));
        }
        app.context().assets->add_source(std::move(owned));
    }

    void add_scene_source(engine::application &app, const assets::asset_id &id) {
        std::unique_ptr<assets::memory_asset_source> owned = std::make_unique<assets::memory_asset_source>();
        assets::cooked_blob_header header = {};
        header.type = assets::asset_type_traits<scene::scene_asset>::id();
        header.typeVersion = 1;
        header.importerVersion = 1;
        header.id = id;
        const std::string guid = test_uuid(81).to_string();
        const std::string text = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"held\",\"settings\":{},"
          "\"roots\":[\"" + guid + "\"],\"archive\":{\"entities\":[{\"components\":{"
          "\"conjure.scene.name\":{\"value\":\"held_root\"},\"conjure.scene.guid\":{\"value\":\"" + guid + "\"}}}]}}";
        const std::byte *begin = reinterpret_cast<const std::byte *>(text.data());
        owned->add(assets::make_cooked_blob(header, {}, std::vector<std::byte>(begin, begin + text.size())));
        app.context().assets->add_source(std::move(owned));
    }
}

TEST_SUITE("bindings/bindings_subsystem") {

    TEST_CASE("bindings_subsystem - startup fills the table before scripting's post_startup") {
        log_capture logs = {};
        std::unique_ptr<engine::application> app = make_app(headless_config());
        CONJURE_REQUIRE_OK(app->initialise());
        scripting_bindings_subsystem *bindings = find_bindings(*app);
        REQUIRE(bindings != nullptr);
        CHECK(bindings->is_filled());
        CHECK(bindings->name() == "bindings");
        CHECK(bindings->context().world == app->context().world);
        CHECK(bindings->context().scenes == app->context().scenes);
        CHECK(bindings->context().physics == app->context().physics);
        CHECK(bindings->context().audio == app->context().audio);
        CHECK(bindings->context().modules != nullptr);
        CHECK(bindings->context().time == app->context().time);
        CHECK(scripting_bindings_subsystem::active_residency() == &bindings->residency());
        REQUIRE(app->context().scripting != nullptr);
        CHECK(app->context().scripting->status() == scripting::script_host_status::READY);
        conjure_native_api &api = app->context().scripting->native_api();
        for (void *const slot : scripting::native_api_slots(api)) {
            CHECK(slot != nullptr);
        }
        for (const native_entry_descriptor &descriptor : native_entry_descriptors()) {
            CAPTURE(function_name(descriptor.id));
            if (descriptor.ownedByBindings && group_compiled(descriptor.group)) {
                CHECK(is_implementation_installed(api, descriptor.id));
            }
#if !CONJURE_DISTRIBUTION
            CHECK(is_counting_installed(api, descriptor.id));          // the core entries too, after step 17
#endif
        }
        // The entries act on the application's world.
        reset_call_counters();
        const core::result<ecs::entity> player =
          app->context().scenes->active().create_entity(core::string_id("player"));
        CONJURE_REQUIRE_OK(player);
        conjure_transform local = {};
        CHECK(api.transform_get_local(player.value().to_u64(), &local) == 0);
        local.position.y = 3.0f;
        CHECK(api.transform_set_local(player.value().to_u64(), &local) == 0);
        CHECK(call_count(native_function_id::transform_set_local) == 1);
        CHECK(api.modules_count() > 0);
        CONJURE_REQUIRE_OK(app->step_frame());
        app->shutdown();
        CHECK_FALSE(bindings->is_filled());
        CHECK(scripting_bindings_subsystem::active_residency() == nullptr);
        CHECK(active_context().world == nullptr);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - a second wrap after post_startup leaves query scopes and fixups working") {
        // wrap_with_counters used to chain the query_end hook behind its own counting thunk when called on a
        // table whose query_end was already filled and wrapped (a second startup, a fixture, a tool); the
        // first scope close then recursed until the stack overflowed.
        log_capture logs = {};
        std::unique_ptr<engine::application> app = make_app(headless_config());
        CONJURE_REQUIRE_OK(app->initialise());
        scripting::script_runtime &runtime = *app->context().scripting;
        conjure_native_api &api = runtime.native_api();
        const auto wrapped = api.query_end;
        wrap_with_counters(api);
        wrap_with_counters(api);
        CHECK(api.query_end == wrapped);
        CHECK(is_scope_hook_installed(api));
#if !CONJURE_DISTRIBUTION
        CHECK(is_counting_installed(api, native_function_id::query_end));
#endif
        reset_call_counters();
        scene::scene &active = app->context().scenes->active();
        const core::result<ecs::entity> parent = active.create_entity(core::string_id("parent"));
        CONJURE_REQUIRE_OK(parent);
        std::array<u32, 1> transformIds = {app->context().components->id_of<scene::local_transform>().value};
        std::array<uint64_t, 2> created = {};
        scripting::null_script_host &host = static_cast<scripting::null_script_host &>(runtime.host());
        host.set_phase_hook([&](scripting::script_phase, f32) {
            conjure_query_scope scope = {};
            REQUIRE(api.query_begin(transformIds.data(), 1, &scope) == 0);
            REQUIRE(app->context().world->is_locked());
            CHECK(api.entity_create_many(2, parent.value().to_u64(), created.data()) == 2);
            api.query_end(scope.scope_id);
            CHECK_FALSE(app->context().world->is_locked());
        });
        runtime.run_phase(scripting::script_phase::UPDATE, 1.0f / 60.0f);
        host.set_phase_hook({});
        for (const uint64_t packed : created) {
            CHECK(app->context().world->is_alive(marshal::to_entity(packed)));
            CHECK(active.tree().parent_of(marshal::to_entity(packed)) == parent.value());
        }
#if !CONJURE_DISTRIBUTION
        CHECK(call_count(native_function_id::query_begin) == 1);
        CHECK(call_count(native_function_id::query_end) == 1);
#endif
        app->shutdown();
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - a re-initialised runtime gets the hook and the counters back at begin_frame") {
        log_capture logs = {};
        std::unique_ptr<engine::application> app = make_app(headless_config());
        CONJURE_REQUIRE_OK(app->initialise());
        scripting::script_runtime &runtime = *app->context().scripting;
        conjure_native_api &api = runtime.native_api();
        CHECK(is_scope_hook_installed(api));
        // script_runtime::initialize refills the core slots with the raw entries: the hook and the core
        // thunks are gone until the bindings' begin_frame notices.
        scripting::fill_core_native_entries(api);
        CHECK_FALSE(is_scope_hook_installed(api));
        CHECK_FALSE(is_counting_installed(api, native_function_id::query_begin));
        CONJURE_REQUIRE_OK(app->step_frame());
        CHECK(is_scope_hook_installed(api));
#if !CONJURE_DISTRIBUTION
        CHECK(is_counting_installed(api, native_function_id::query_begin));
        CHECK(is_counting_installed(api, native_function_id::query_end));
        CHECK(is_counting_installed(api, native_function_id::transform_set_local));
#endif
        // The re-installed hook applies the scene fixups of a locked entry at the scope close again.
        scene::scene &active = app->context().scenes->active();
        std::array<u32, 1> transformIds = {app->context().components->id_of<scene::local_transform>().value};
        uint64_t created = 0;
        scripting::null_script_host &host = static_cast<scripting::null_script_host &>(runtime.host());
        host.set_phase_hook([&](scripting::script_phase, f32) {
            conjure_query_scope scope = {};
            REQUIRE(api.query_begin(transformIds.data(), 1, &scope) == 0);
            CHECK(api.entity_create_many(1, 0, &created) == 1);
            CHECK(bindings::detail::pending_fixup_count() == 1);
            api.query_end(scope.scope_id);
            CHECK(bindings::detail::pending_fixup_count() == 0);
        });
        runtime.run_phase(scripting::script_phase::UPDATE, 1.0f / 60.0f);
        host.set_phase_hook({});
        CHECK(app->context().world->is_alive(marshal::to_entity(created)));
        CHECK(active.tree().parent_of(marshal::to_entity(created)).is_null());
        app->shutdown();
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("bindings_subsystem - startup without scripting is a no-op") {
        log_capture logs = {};
        engine::engine_config config = headless_config();
        config.scripting.enabled = false;
        std::unique_ptr<engine::application> app = make_app(config);
        CONJURE_REQUIRE_OK(app->initialise());
        scripting_bindings_subsystem *bindings = find_bindings(*app);
        REQUIRE(bindings != nullptr);
        CHECK_FALSE(bindings->is_filled());
        CHECK(bindings->context().world == nullptr);
        CHECK(scripting_bindings_subsystem::active_residency() == nullptr);
        CONJURE_REQUIRE_OK(app->step_frame());
        app->shutdown();
    }

    TEST_CASE("bindings_subsystem - shutdown is safe after a failed startup and when called twice") {
        log_capture logs = {};
        logs.allow_errors();
        std::unique_ptr<engine::application> app = make_app(headless_config());
        app->add_subsystem(std::make_unique<failing_subsystem>());
        const core::result<void> started = app->initialise();
        CHECK_FALSE(started.has_value());
        CHECK(app->state() == engine::application_state::CREATED);
        CHECK(scripting_bindings_subsystem::active_residency() == nullptr);
        CHECK(active_context().world == nullptr);
        app->shutdown();
        app->shutdown();
        // A fresh subsystem can also be shut down without ever starting.
        scripting_bindings_subsystem untouched = {};
        engine::engine_context context = {};
        untouched.shutdown(context);
        untouched.shutdown(context);
        untouched.begin_frame(context);
        CHECK_FALSE(untouched.is_filled());
    }

    TEST_CASE("bindings_subsystem - a scene unload clears the residency") {
        log_capture logs = {};
        std::unique_ptr<engine::application> app = make_app(headless_config());
        CONJURE_REQUIRE_OK(app->initialise());
        scripting_bindings_subsystem *bindings = find_bindings(*app);
        REQUIRE(bindings != nullptr);
        const assets::asset_id blob = test_uuid(7);
        add_binary_source(*app, blob, "assets://data/held.bin");
        conjure_native_api &api = app->context().scripting->native_api();
        const conjure_uuid id = marshal::from_uuid(blob);
        CHECK(api.asset_load(&id, 0, 1) == 0);
        CHECK(bindings->residency().size() == 1);
        // A loaded scene unloads: on_before_unload fires and the residency empties.
        const assets::asset_id sceneId = test_uuid(8);
        add_scene_source(*app, sceneId);
        const core::result<scene::scene_id> loaded = app->context().scenes->load(sceneId, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(bindings->residency().size() == 1);
        CONJURE_REQUIRE_OK(app->context().scenes->unload(loaded.value()));
        CHECK(bindings->residency().size() == 0);
        app->shutdown();
    }

    TEST_CASE("bindings_subsystem - begin_frame clears the last error and the rate-limit bits") {
        log_capture logs = {};
        std::unique_ptr<engine::application> app = make_app(headless_config());
        CONJURE_REQUIRE_OK(app->initialise());
        conjure_native_api &api = app->context().scripting->native_api();
        conjure_transform local = {};
        CHECK(api.transform_get_local(999, &local) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        CHECK(bindings::detail::failure_logged_this_frame(native_function_id::transform_get_local));
        CHECK(last_error_code() == bindings_error::ENTITY_NOT_ALIVE);
        CONJURE_REQUIRE_OK(app->step_frame());
        CHECK_FALSE(bindings::detail::failure_logged_this_frame(native_function_id::transform_get_local));
        CHECK(last_error_code() == bindings_error::NONE);
        CHECK(last_error_message().empty());
        app->shutdown();
    }

    TEST_CASE("bindings_subsystem - the engine's frame index reaches diagnostics_check") {
        std::unique_ptr<engine::application> app = make_app(headless_config());
        CONJURE_REQUIRE_OK(app->initialise());
        conjure_native_api &api = app->context().scripting->native_api();
        CONJURE_REQUIRE_OK(app->step_frame());
        CONJURE_REQUIRE_OK(app->step_frame());
        const utf8_argument name = utf8("frames.counted");
        CHECK(api.diagnostics_check(name.data, name.length, 1, nullptr, 0) == 0);
        const std::vector<engine::diagnostics::check_record> records = app->context().checks->records();
        REQUIRE(records.size() == 1);
        CHECK(records[0].frame == app->time().frameIndex);
        CHECK(app->context().checks->all_passed());
        app->shutdown();
    }
}
