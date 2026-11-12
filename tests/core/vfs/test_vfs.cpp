#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"

#include "vfs_test_helpers.h"

#include "conjure/core/vfs/vfs.h"

#include <memory>
#include <string>
#include <type_traits>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;
using namespace conjure::testing::vfs_helpers;

TEST_SUITE("core/vfs") {

    TEST_CASE("vfs - the umbrella header exposes every public type of the area") {
        static_assert(std::is_base_of_v<file_system, native_file_system>);
        static_assert(std::is_base_of_v<file_system, memory_file_system>);
        static_assert(std::is_base_of_v<file_system, mount_table>);
        static_assert(std::is_final_v<native_file_system>);
        static_assert(std::is_final_v<memory_file_system>);
        static_assert(std::is_final_v<mount_table>);
        CHECK(path_utils::normalize("a//b") == "a/b");
    }

    TEST_CASE("vfs - the pieces compose: a mount table over native and memory backends") {
        testing::temp_directory directory("core_vfs_umbrella");
        mount_table mounts = {};
        REQUIRE(mounts.mount("project", std::make_shared<native_file_system>(directory.path() / "project")));
        REQUIRE(mounts.mount("cooked", std::make_shared<memory_file_system>()));

        REQUIRE(mounts.write_text_file("project://scenes/main.scene", "{}"));
        REQUIRE(mounts.create_directories("cooked://scenes"));
        REQUIRE(mounts.rename("project://scenes/main.scene", "cooked://scenes/main.scene"));

        CHECK_FALSE(mounts.exists("project://scenes/main.scene"));
        CHECK(mounts.read_text_file("cooked://scenes/main.scene").value() == "{}");
        CHECK(path_utils::extension("cooked://scenes/main.scene") == ".scene");
    }
}
