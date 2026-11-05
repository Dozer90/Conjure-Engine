#include "conjure/testing/doctest.h"

#include "conjure/testing/test_assets.h"
#include "conjure/testing/test_environment.h"

#include <filesystem>

using namespace conjure;
using namespace conjure::testing;

TEST_SUITE("testing/test_assets") {

    TEST_CASE("repo_asset - resolves under tests/assets") {
        const std::filesystem::path path = repo_asset("primitives/cube.gltf");

        CHECK(path == environment().repoAssetRoot / "primitives" / "cube.gltf");
        CHECK(path.parent_path().parent_path().filename() == "assets");
        CHECK(path.is_absolute());
    }

    TEST_CASE("downloaded_asset - empty without the downloaded set, otherwise under tests/.downloaded") {
        const std::filesystem::path path = downloaded_asset("gltf/DamagedHelmet.glb");

        if (environment().downloadedAssetRoot.empty()) {
            CHECK(path.empty());
        } else {
            CHECK(path == environment().downloadedAssetRoot / "gltf" / "DamagedHelmet.glb");
        }
    }
}
