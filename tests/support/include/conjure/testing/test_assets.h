#pragma once

#include <filesystem>
#include <string_view>

// Paths of test fixtures (docs/design/08_testing.md 4.3). In-repo fixtures live in tests/assets and
// are always present; downloaded fixtures live in tests/.downloaded and are guarded by
// CONJURE_TEST_REQUIRE(precondition::DOWNLOADED_ASSETS).

namespace conjure::testing {

    /// <summary>
    /// Absolute path of a fixture under tests/assets. The file need not exist; a missing in-repo
    /// asset is a test failure at the point of use.
    /// </summary>
    [[nodiscard]] std::filesystem::path repo_asset(std::string_view relativePath);

    /// <summary>
    /// Absolute path of a fixture under tests/.downloaded. Empty when the downloaded set is absent.
    /// </summary>
    [[nodiscard]] std::filesystem::path downloaded_asset(std::string_view relativePath);
}
