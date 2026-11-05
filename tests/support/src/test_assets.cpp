#include "conjure/testing/test_assets.h"

#include "conjure/testing/test_environment.h"

using namespace conjure::testing;

std::filesystem::path conjure::testing::repo_asset(std::string_view relativePath) {
    return environment().repoAssetRoot / std::filesystem::path(relativePath);
}

std::filesystem::path conjure::testing::downloaded_asset(std::string_view relativePath) {
    const std::filesystem::path &root = environment().downloadedAssetRoot;
    if (root.empty()) {
        return {};
    }
    return root / std::filesystem::path(relativePath);
}
