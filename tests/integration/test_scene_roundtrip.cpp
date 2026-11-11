#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "conjure/testing/test_assets.h"
#include "conjure/testing/test_environment.h"
#include "integration_engine_support.h"

#include "conjure/core/types.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/scene.h"

#include <string>

// docs/design/08_testing.md 6.1 test_scene_roundtrip: the in-repo fixture tests/assets/scenes/roundtrip.scene.json
// loads and saves back byte for byte, and a scene authored through the API survives save -> load with an equal
// world_hash (the file was produced by author_roundtrip_scene, so the two halves describe the same scene; running
// the programmatic case with --conjure-update-golden rewrites the fixture from the authoring helper, see
// tests/assets/README.md).

using namespace conjure;
using namespace conjure::testing;

namespace {
    constexpr std::string_view ROUNDTRIP_FIXTURE = "scenes/roundtrip.scene.json";
    constexpr u32 ROUNDTRIP_ENTITY_COUNT = 5;
    constexpr usize AUTHORED_TEXT_CAPACITY = 8192;   // the fixture is about 1.6 KB
}

TEST_SUITE("integration/scene_roundtrip") {

    TEST_CASE("scene_roundtrip - the repository fixture loads and saves back byte-identical") {
        const std::filesystem::path fixture = repo_asset(ROUNDTRIP_FIXTURE);
        const std::string expected = integration_test::read_file(fixture);
        REQUIRE_FALSE(expected.empty());

        headless_engine engine(integration_test::quiet_options());
        CONJURE_REQUIRE_OK(engine.load_scene(fixture.generic_string()));
        CHECK(engine.world().entity_count() == ROUNDTRIP_ENTITY_COUNT);
        CHECK(engine.active_scene().name() == "roundtrip");
        engine.step_frames(1);

        const std::string saved = integration_test::serialise_scene(engine.active_scene());
        CHECK(saved == expected);

        // The leaf sits at the sum of its ancestors' offsets once propagation has run.
        const ecs::entity leaf = engine.find_entity("Root/Child/Leaf");
        REQUIRE_FALSE(leaf.is_null());
        const math::vec3 position = integration_test::propagated_position(engine.active_scene(), leaf);
        CHECK(position.x == doctest::Approx(1.0f));
        CHECK(position.y == doctest::Approx(1.0f));
        CHECK(position.z == doctest::Approx(2.0f));
        CHECK_FALSE(engine.active_scene().is_enabled(engine.find_entity("Other")));
    }

    TEST_CASE("scene_roundtrip - a programmatic scene saves, loads and hashes the same") {
        // The saved text outlives the authoring fixture, whose leak check must still see its baseline: the
        // destination is reserved before the fixture exists and the text is copied into that storage inside the
        // fixture's scope, so nothing allocated during the run survives it.
        std::string text = {};
        text.reserve(AUTHORED_TEXT_CAPACITY);
        u64 authoredHash = 0;
        bool resavedIdentical = false;
        {
            headless_engine authoring(integration_test::quiet_options());
            const std::string authored = integration_test::author_roundtrip_scene(authoring);
            REQUIRE(authored.size() <= AUTHORED_TEXT_CAPACITY);
            text.assign(std::string_view(authored));
            authoredHash = authoring.world_hash();
        }
        REQUIRE_FALSE(text.empty());
        REQUIRE(text.capacity() >= AUTHORED_TEXT_CAPACITY);   // the copy reused the reservation
        if (environment().updateGoldens) {
            // --conjure-update-golden: the authoring helper is the source of the repository fixture.
            integration_test::write_file(repo_asset(ROUNDTRIP_FIXTURE), text);
        }
        {
            headless_engine loading(integration_test::quiet_options());
            CONJURE_REQUIRE_OK(loading.load_scene_json(text));
            loading.step_frames(1);
            CHECK(loading.world().entity_count() == ROUNDTRIP_ENTITY_COUNT);
            CHECK(loading.world_hash() == authoredHash);
            // Compared inside the fixture's scope so the text does not outlive its memory baseline.
            resavedIdentical = integration_test::serialise_scene(loading.active_scene()) == text;
        }
        CHECK(resavedIdentical);
    }
}
