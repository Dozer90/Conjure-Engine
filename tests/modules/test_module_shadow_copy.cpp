#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "shared_fixtures.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/dynamic_library.h"
#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

namespace {
    std::vector<std::filesystem::path> files_under(const std::filesystem::path &directory) {
        std::vector<std::filesystem::path> files = {};
        std::error_code ignored = {};
        if (!std::filesystem::is_directory(directory, ignored)) {
            return files;
        }
        const std::filesystem::recursive_directory_iterator entries(directory, ignored);
        for (const std::filesystem::directory_entry &entry : entries) {
            if (entry.is_regular_file(ignored)) {
                files.push_back(entry.path());
            }
        }
        return files;
    }

    bool contains_file(const std::vector<std::filesystem::path> &files, std::string_view name) {
        for (const std::filesystem::path &file : files) {
            if (file.filename().generic_string() == name) {
                return true;
            }
        }
        return false;
    }

    /// <summary>
    /// True when the file can be opened for writing right now (a loaded DLL cannot be, on Windows).
    /// </summary>
    bool is_writable(const std::filesystem::path &file) {
        std::ofstream stream(file, std::ios::binary | std::ios::app);
        return stream.is_open();
    }

    module_manager_desc shadow_desc(const std::filesystem::path &cache) {
        module_manager_desc desc = shared_desc();
        desc.shadowCopyBinaries = true;
        desc.shadowCopyDirectory = cache;
        return desc;
    }

    /// <summary>
    /// A private copy of the alpha fixture tree, so file-lock checks are not disturbed by other
    /// test processes that load the shared fixture binary at the same time. Returns the manifest.
    /// </summary>
    std::filesystem::path private_alpha(const conjure::testing::temp_directory &directory) {
        const std::filesystem::path target = directory.path() / "module_alpha";
        std::error_code failure = {};
        const std::filesystem::copy_options options = std::filesystem::copy_options::recursive;
        std::filesystem::copy(fixture_directory("module_alpha"), target, options, failure);
        REQUIRE_FALSE(static_cast<bool>(failure));
        return target / MODULE_MANIFEST_FILE_NAME;
    }
}

TEST_SUITE("modules/module_shadow_copy") {

    TEST_CASE("module_manager - a shadow copied module loads from the cache and leaves the original writable") {
        const conjure::testing::temp_directory source("modules_shadow_copy_source");
        const conjure::testing::temp_directory cache("modules_shadow_copy");
        shared_host host(shadow_desc(cache.path()));
        const module_id alpha = host.manager.add_manifest(private_alpha(source)).value();
        const std::filesystem::path original = host.manager.registry().binary_path(alpha);
        REQUIRE(std::filesystem::exists(original));

        CONJURE_REQUIRE_OK(host.manager.load(alpha));
        const std::vector<std::filesystem::path> copied = files_under(cache.path() / "module_alpha" / "1");
        CHECK(contains_file(copied, original.filename().generic_string()));
        CHECK(host.manager.registry().binary_path(alpha) == original);
        CHECK(is_writable(original));

        std::filesystem::path pdb = original;
        pdb.replace_extension(".pdb");
        if (std::filesystem::exists(pdb)) {
            CHECK(contains_file(copied, pdb.filename().generic_string()));
        }

        CONJURE_REQUIRE_OK(host.manager.unload(alpha));
        CHECK_FALSE(std::filesystem::exists(cache.path() / "module_alpha" / "1"));
    }

    TEST_CASE("module_manager - every load makes a fresh copy under the next index") {
        const conjure::testing::temp_directory cache("modules_shadow_copy_reload");
        shared_host host(shadow_desc(cache.path()));
        const module_id alpha = host.manager.add_manifest(fixture_manifest("module_alpha")).value();

        CONJURE_REQUIRE_OK(host.manager.load(alpha));
        CONJURE_REQUIRE_OK(host.manager.reload(alpha, reload_scope::FULL));
        CHECK_FALSE(std::filesystem::exists(cache.path() / "module_alpha" / "1"));
        CHECK(std::filesystem::is_directory(cache.path() / "module_alpha" / "2"));
        CHECK(host.manager.registry().load_count(alpha) == 2);

        CONJURE_REQUIRE_OK(host.manager.unload(alpha));
        const module_id again = host.manager.registry().find(string_id("module_alpha"));
        CONJURE_REQUIRE_OK(host.manager.load(again));
        CHECK(std::filesystem::is_directory(cache.path() / "module_alpha" / "3"));
    }

    TEST_CASE("module_manager - keepBinariesResident keeps every earlier copy mapped until the manager goes away") {
        const conjure::testing::temp_directory source("modules_shadow_copy_resident_source");
        const conjure::testing::temp_directory cache("modules_shadow_copy_resident");
        module_manager_desc desc = shadow_desc(cache.path());
        desc.keepBinariesResident = true;
        const std::filesystem::path binaryName = fixture_binary("module_alpha").filename();
        const std::filesystem::path firstCopy = cache.path() / "module_alpha" / "1" / binaryName;
        const std::filesystem::path secondCopy = cache.path() / "module_alpha" / "2" / binaryName;
        const std::filesystem::path thirdCopy = cache.path() / "module_alpha" / "3" / binaryName;
        {
            shared_host host(desc);
            const module_id alpha = host.manager.add_manifest(private_alpha(source)).value();
            CONJURE_REQUIRE_OK(host.manager.load(alpha));
            CONJURE_REQUIRE_OK(host.manager.reload(alpha, reload_scope::FULL));

            // A reload replaced the handle; the debug aid promises the first image stays mapped and
            // its copy stays on disk instead of being freed by the handle's move assignment.
            CHECK(std::filesystem::exists(firstCopy));
            CHECK(std::filesystem::exists(secondCopy));
            if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
                CHECK_FALSE(is_writable(firstCopy));
                CHECK_FALSE(is_writable(secondCopy));
            }

            CONJURE_REQUIRE_OK(host.manager.unload(alpha));
            CHECK(std::filesystem::exists(firstCopy));
            CHECK(std::filesystem::exists(secondCopy));
            if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
                CHECK_FALSE(is_writable(secondCopy));
            }

            const module_id again = host.manager.registry().find(string_id("module_alpha"));
            CONJURE_REQUIRE_OK(host.manager.load(again));
            CHECK(std::filesystem::exists(firstCopy));
            CHECK(std::filesystem::exists(secondCopy));
            CHECK(std::filesystem::exists(thirdCopy));
            if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
                CHECK_FALSE(is_writable(firstCopy));
                CHECK_FALSE(is_writable(secondCopy));
                CHECK_FALSE(is_writable(thirdCopy));
            }
            CHECK(host.manager.registry().load_count(again) == 3);
        }

        // The manager's destruction releases every resident handle.
        CHECK(is_writable(firstCopy));
        CHECK(is_writable(secondCopy));
        CHECK(is_writable(thirdCopy));
    }

    TEST_CASE("module_manager - the cache is cleaned when the manager is constructed") {
        const conjure::testing::temp_directory cache("modules_shadow_copy_clean");
        CONJURE_REQUIRE_OK(cache.write_text("module_alpha/7/stale.txt", "left over"));
        CONJURE_REQUIRE_OK(cache.write_text("other/1/stale.txt", "left over"));
        conjure::testing::log_capture logs = {};
        {
            const shared_host host(shadow_desc(cache.path()));
            CHECK(files_under(cache.path()).empty());
            CHECK(std::filesystem::is_directory(cache.path()));
        }
        CHECK(logs.count(log_level::WARN) == 0);

        // A cache directory that does not exist yet is simply created on the first copy.
        shared_host host(shadow_desc(cache.path() / "nested" / "cache"));
        const module_id alpha = host.manager.add_manifest(fixture_manifest("module_alpha")).value();
        CONJURE_REQUIRE_OK(host.manager.load(alpha));
        CHECK(std::filesystem::is_directory(cache.path() / "nested" / "cache" / "module_alpha" / "1"));
    }

    TEST_CASE("module_manager - a locked cache entry is skipped with a warning where the platform locks files") {
        const conjure::testing::temp_directory cache("modules_shadow_copy_locked");
        const std::filesystem::path lockedDirectory = cache.path() / "module_alpha" / "3";
        std::filesystem::create_directories(lockedDirectory);
        const std::filesystem::path locked = lockedDirectory / fixture_binary("module_alpha").filename();
        std::filesystem::copy_file(fixture_binary("module_alpha"), locked);
        result<dynamic_library> holder = dynamic_library::load(locked);
        REQUIRE(holder.has_value());

        conjure::testing::log_capture logs = {};
        const shared_host host(shadow_desc(cache.path()));
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            CHECK(std::filesystem::exists(locked));
            CHECK(logs.count(log_level::WARN) == 1);
            CHECK(logs.contains(log_level::WARN, "shadow copy cache"));
        } else {
            CHECK_FALSE(std::filesystem::exists(locked));
            CHECK(logs.count(log_level::WARN) == 0);
        }
        holder->unload();
    }

    TEST_CASE("module_manager - shadow copying without a cache directory is BINARY_COPY_FAILED") {
        module_manager_desc desc = shared_desc();
        desc.shadowCopyBinaries = true;
        shared_host host(desc);
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        const module_id alpha = host.manager.add_manifest(fixture_manifest("module_alpha")).value();
        const result<void> loaded = host.manager.load(alpha);
        CONJURE_CHECK_ERROR(loaded, module_error::BINARY_COPY_FAILED);
        CHECK(loaded.get_error().message().find("shadowCopyDirectory") != std::string::npos);
        CHECK(host.manager.registry().state(alpha) == module_state::FAILED);
    }

    TEST_CASE("module_manager - a directly loaded module keeps its binary locked only where the platform does") {
        const conjure::testing::temp_directory source("modules_shadow_copy_direct_source");
        shared_host host = {};
        const module_id alpha = host.manager.add_manifest(private_alpha(source)).value();
        const std::filesystem::path original = host.manager.registry().binary_path(alpha);
        CONJURE_REQUIRE_OK(host.manager.load(alpha));
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            CHECK_FALSE(is_writable(original));
        }
        CONJURE_REQUIRE_OK(host.manager.unload(alpha));
        CHECK(is_writable(original));
    }
}
