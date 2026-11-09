#include "conjure/testing/doctest.h"

#include "conjure/modules/module_types.h"

#include "conjure/core/platform_detect.h"

#include <array>
#include <format>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::modules;

TEST_SUITE("modules/module_types") {

    TEST_CASE("module_id - the default id is invalid and ids order by index then generation") {
        const module_id invalid = {};
        CHECK_FALSE(invalid.is_valid());
        CHECK(invalid.index == module_id::INVALID_INDEX);
        CHECK(invalid.generation == 0);

        const module_id first = {0, 0};
        const module_id firstAgain = {0, 1};
        const module_id second = {1, 0};
        CHECK(first.is_valid());
        CHECK(first < firstAgain);
        CHECK(firstAgain < second);
        CHECK(first == module_id{0, 0});
        CHECK(first != firstAgain);

        CHECK(to_string(invalid) == "invalid");
        CHECK(to_string(module_id{3, 1}) == "3:1");
        CHECK(std::format("{}", module_id{7, 2}) == "7:2");
    }

    TEST_CASE("module_types - manifest spellings round trip through to_string and parse") {
        for (const module_kind kind : {module_kind::ENGINE, module_kind::PLUGIN, module_kind::GAME}) {
            CHECK(parse_module_kind(to_string(kind)) == kind);
        }
        CHECK(to_string(module_kind::GAME) == "game");
        CHECK_FALSE(parse_module_kind("Game").has_value());
        CHECK_FALSE(parse_module_kind("").has_value());

        const std::array<module_platform, 3> platforms = {
            module_platform::WINDOWS, module_platform::LINUX, module_platform::MACOS};
        for (const module_platform platform : platforms) {
            CHECK(parse_module_platform(to_string(platform)) == platform);
        }
        CHECK(to_string(module_platform::MACOS) == "macos");
        CHECK_FALSE(parse_module_platform("osx").has_value());

        const std::array<module_feature_flags, 5> features = {
            module_feature_flags::RENDERER,
            module_feature_flags::PHYSICS,
            module_feature_flags::AUDIO,
            module_feature_flags::SCRIPTING,
            module_feature_flags::EDITOR,
        };
        for (const module_feature_flags feature : features) {
            CHECK(parse_module_feature(to_string(feature)) == feature);
        }
        CHECK_FALSE(parse_module_feature("network").has_value());

        for (const build_config config : {build_config::DEBUG, build_config::DEVELOPMENT, build_config::DISTRIBUTION}) {
            CHECK(parse_build_config(to_string(config)) == config);
        }
        CHECK(to_string(build_config::DISTRIBUTION) == "distribution");
        CHECK_FALSE(parse_build_config("Release").has_value());

        CHECK(to_string(module_architecture::X64) == "x64");
        CHECK(to_string(module_architecture::ARM64) == "arm64");
        CHECK(to_string(module_host_kind::EDITOR) == "editor");
        CHECK(to_string(unload_policy::REMOVE_INSTANCES) == "REMOVE_INSTANCES");
        CHECK(to_string(reload_scope::MANAGED_ONLY) == "MANAGED_ONLY");
        CHECK(to_string(module_state::FAULTED) == "FAULTED");
        CHECK(std::format("{}", module_state::LOADED) == "LOADED");
    }

    TEST_CASE("module_types - flag sets print their set members in declaration order") {
        CHECK(to_string(module_feature_flags::NONE).empty());
        CHECK(to_string(module_feature_flags::RENDERER) == "renderer");
        CHECK(to_string(module_feature_flags::PHYSICS | module_feature_flags::EDITOR) == "physics, editor");
        CHECK(to_string(module_platform_flags::ALL) == "windows, linux, macos");
        CHECK(to_string(module_platform_flags::MACOS | module_platform_flags::WINDOWS) == "windows, macos");
        CHECK(to_string(module_platform_flags::NONE).empty());

        CHECK(to_flag(module_platform::WINDOWS) == module_platform_flags::WINDOWS);
        CHECK(to_flag(module_platform::LINUX) == module_platform_flags::LINUX);
        CHECK(to_flag(module_platform::MACOS) == module_platform_flags::MACOS);
        CHECK(has_flag(module_platform_flags::ALL, to_flag(module_platform::LINUX)));
    }

    TEST_CASE("module_types - host facts follow platform_detect and the build options") {
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            CHECK(host_platform() == module_platform::WINDOWS);
        } else if constexpr (CONJURE_PLATFORM_MACOS == 1) {
            CHECK(host_platform() == module_platform::MACOS);
        } else {
            CHECK(host_platform() == module_platform::LINUX);
        }
        if constexpr (CONJURE_ARCH_ARM64 == 1) {
            CHECK(host_architecture() == module_architecture::ARM64);
        } else {
            CHECK(host_architecture() == module_architecture::X64);
        }
        if constexpr (CONJURE_DEBUG == 1) {
            CHECK(host_build_config() == build_config::DEBUG);
        } else if constexpr (CONJURE_DISTRIBUTION == 1) {
            CHECK(host_build_config() == build_config::DISTRIBUTION);
        } else {
            CHECK(host_build_config() == build_config::DEVELOPMENT);
        }
        CHECK(host_platform_flag() == to_flag(host_platform()));

        const binary_triplet triplet = host_binary_triplet();
        CHECK(triplet.platform == host_platform());
        CHECK(triplet.architecture == host_architecture());
        CHECK(triplet.config == host_build_config());
        CHECK(to_string(triplet) == std::format(
          "{}-{}-{}", to_string(host_platform()), to_string(host_architecture()), to_string(host_build_config())));
        CHECK(to_string(host_binary_triplet(build_config::DISTRIBUTION)).ends_with("-distribution"));
        CHECK(std::format("{}", binary_triplet{module_platform::LINUX, module_architecture::ARM64, build_config::DEBUG})
          == "linux-arm64-debug");

        const module_feature_flags features = host_feature_flags();
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        CHECK(has_flag(features, module_feature_flags::RENDERER));
#else
        CHECK_FALSE(has_flag(features, module_feature_flags::RENDERER));
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
        CHECK(has_flag(features, module_feature_flags::SCRIPTING));
#else
        CHECK_FALSE(has_flag(features, module_feature_flags::SCRIPTING));
#endif
#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
        CHECK(has_flag(features, module_feature_flags::EDITOR));
#else
        CHECK_FALSE(has_flag(features, module_feature_flags::EDITOR));
#endif
    }

    TEST_CASE("module_types - enumerations carry their documented values") {
        CHECK(static_cast<u8>(build_config::DEBUG) == 1);
        CHECK(static_cast<u8>(build_config::DEVELOPMENT) == 2);
        CHECK(static_cast<u8>(build_config::DISTRIBUTION) == 3);
        CHECK(static_cast<u32>(module_entry_status::OK) == 0);
        CHECK(static_cast<u32>(module_entry_status::FAILED) == 1);
        CHECK(static_cast<u8>(module_platform_flags::ALL) == 7);
        CHECK(static_cast<u32>(module_feature_flags::EDITOR) == 16);
    }
}
