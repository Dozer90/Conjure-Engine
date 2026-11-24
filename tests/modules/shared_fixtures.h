#pragma once

#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"

#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/event_bus.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>

// The fixture plugins built by tests/modules/CMakeLists.txt live under CONJURE_TEST_MODULE_DIR/<name>/
// with a copy of their module.json and the binary at bin/<platform>-<arch>-<config>/.

namespace conjure::modules::testing {

    inline std::filesystem::path fixture_root() {
        return std::filesystem::path(CONJURE_TEST_MODULE_DIR);
    }

    inline std::filesystem::path fixture_directory(std::string_view name) {
        return fixture_root() / std::string(name);
    }

    inline std::filesystem::path fixture_manifest(std::string_view name) {
        return fixture_directory(name) / MODULE_MANIFEST_FILE_NAME;
    }

    // The test's own file calls reach a folder past MAX_PATH through the support library's long form, and the
    // long folders come from there too.
    using conjure::testing::extended_length_path;
    using conjure::testing::long_directory;
    using conjure::testing::PAST_MAX_PATH;

    /// <summary>
    /// The length from which the module system opens a path in Windows' extended-length form and a failed
    /// load names the length (engine/src/modules/long_path.h, EXTENDED_PATH_THRESHOLD).
    /// </summary>
    inline constexpr usize WINDOWS_EXTENDED_PATH_THRESHOLD = 248;

    /// <summary>
    /// True when the file can be opened for writing right now (a loaded DLL cannot be, on Windows). Pass the
    /// extended_length_path() of a file past MAX_PATH.
    /// </summary>
    inline bool is_writable(const std::filesystem::path &file) {
        std::ofstream stream(file, std::ios::binary | std::ios::app);
        return stream.is_open();
    }

    /// <summary>
    /// Copies the tree 'source' to 'root' (created with its parents; it may lie past MAX_PATH) and returns 'root'.
    /// </summary>
    inline std::filesystem::path copy_tree_to(const std::filesystem::path &source, const std::filesystem::path &root) {
        std::error_code failure = {};
        std::filesystem::create_directories(extended_length_path(root.parent_path()), failure);
        REQUIRE_MESSAGE(!failure, "cannot create " << root.parent_path().string() << ": " << failure.message());
        const std::filesystem::copy_options options = std::filesystem::copy_options::recursive;
        std::filesystem::copy(extended_length_path(source), extended_length_path(root), options, failure);
        REQUIRE_MESSAGE(
          !failure, "cannot copy " << source.string() << " to " << root.string() << ": " << failure.message());
        return root;
    }

    /// <summary>
    /// Copies a fixture's tree to 'root' (created with its parents; it may lie past MAX_PATH) and returns 'root'.
    /// </summary>
    inline std::filesystem::path copy_fixture_to(std::string_view name, const std::filesystem::path &root) {
        return copy_tree_to(fixture_directory(name), root);
    }

    /// <summary>
    /// A private copy of a fixture's tree, so a test can add files to it or lock its binary without touching
    /// the fixture other test processes load. The copy is directory/&lt;relativeRoot&gt;/, directory/&lt;name&gt;/
    /// when relativeRoot is empty. Returns the copy's root.
    /// </summary>
    inline std::filesystem::path copy_fixture(
      const conjure::testing::temp_directory &directory,
      std::string_view name,
      std::string_view relativeRoot = {}
    ) {
        return copy_fixture_to(name, directory.path() / std::string(relativeRoot.empty() ? name : relativeRoot));
    }

    /// <summary>
    /// The binary of a fixture whose manifest "binary" equals its directory name, for this build.
    /// </summary>
    inline std::filesystem::path fixture_binary(std::string_view name) {
        module_manifest manifest = {};
        manifest.binary = std::string(name);
        return module_binary_path(manifest, fixture_directory(name), host_binary_triplet());
    }

    /// <summary>
    /// The module_surface fixture (tests/modules/fixtures/module_surface) is built into its own root,
    /// CONJURE_TEST_SURFACE_MODULE_DIR/module_surface/, so discover() over fixture_root() never sees it.
    /// </summary>
    inline std::filesystem::path surface_fixture_directory() {
        return std::filesystem::path(CONJURE_TEST_SURFACE_MODULE_DIR) / "module_surface";
    }

    inline std::filesystem::path surface_fixture_manifest() {
        return surface_fixture_directory() / MODULE_MANIFEST_FILE_NAME;
    }

    inline std::filesystem::path surface_fixture_binary() {
        module_manifest manifest = {};
        manifest.binary = "module_surface";
        return module_binary_path(manifest, surface_fixture_directory(), host_binary_triplet());
    }

    /// <summary>
    /// The module_sibling fixture (tests/modules/fixtures/module_sibling) is built into its own root,
    /// CONJURE_TEST_SIBLING_MODULE_DIR/module_sibling/, with the library it depends on, sibling_helper, next to
    /// its binary and nowhere else. discover() over fixture_root() never sees it.
    /// </summary>
    inline std::filesystem::path sibling_fixture_directory() {
        return std::filesystem::path(CONJURE_TEST_SIBLING_MODULE_DIR) / "module_sibling";
    }

    inline module_manager_desc shared_desc() noexcept {
        module_manager_desc desc = {};
        desc.host = module_host_kind::TEST;
        desc.allowSharedLibraries = true;
        return desc;
    }

    /// <summary>
    /// A manager that loads shared libraries, with its own event bus.
    /// </summary>
    struct shared_host {
        core::event_bus events = {};
        module_manager_desc desc = {};
        module_manager manager;     // no default: built from desc below

        shared_host() :
            desc(shared_desc()),
            manager(desc, events) {}

        explicit shared_host(const module_manager_desc &customDesc) :
            desc(customDesc),
            manager(desc, events) {}
    };
}
