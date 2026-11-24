#include "conjure/testing/doctest.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_payload.h"

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    struct plain_loader final : public asset_loader {
        [[nodiscard]] asset_type_id type() const noexcept override {
            return asset_type_traits<other_asset>::id();
        }

        [[nodiscard]] core::result<asset_payload> load_cpu(
          const load_context &,
          std::span<const std::byte> bytes
        ) const override {
            return make_payload(other_asset{text_of(bytes)});
        }

        [[nodiscard]] core::result<asset_payload> finalise(
          const finalise_context &,
          asset_payload &&cpuOutput
        ) const override {
            return std::move(cpuOutput);
        }
    };
}

TEST_SUITE("assets/asset_loader") {

    TEST_CASE("asset_loader - the default fallback is a null payload") {
        const plain_loader loader = {};
        core::result<asset_payload> fallback = loader.create_fallback();
        CONJURE_REQUIRE_OK(fallback);
        CHECK(fallback.value() == nullptr);
    }

    TEST_CASE("asset_loader - a plain loader runs both stages through erased payloads") {
        const plain_loader loader = {};
        const std::vector<std::byte> bytes = bytes_of("plain");
        load_context context = {};
        context.id = test_id(1);
        context.type = loader.type();
        core::result<asset_payload> cpu = loader.load_cpu(context, bytes);
        CONJURE_REQUIRE_OK(cpu);
        core::result<asset_payload> finished = loader.finalise(finalise_context{}, std::move(cpu).value());
        CONJURE_REQUIRE_OK(finished);
        CHECK(payload_as<other_asset>(finished.value())->content == "plain");
    }

    TEST_CASE("typed_asset_loader - type() is the traits id and the stages are erased") {
        const test_loader loader = {};
        CHECK(loader.type() == asset_type_traits<test_asset>::id());

        const std::vector<asset_id> dependencies = {test_id(2), test_id(3)};
        load_context context = {};
        context.id = test_id(1);
        context.type = loader.type();
        context.dependencies = dependencies;
        const std::vector<std::byte> bytes = bytes_of("typed");

        core::result<asset_payload> cpu = loader.load_cpu(context, bytes);
        CONJURE_REQUIRE_OK(cpu);
        const test_asset *intermediate = payload_as<test_asset>(cpu.value());
        REQUIRE(intermediate != nullptr);
        CHECK(intermediate->content == "typed");
        CHECK(intermediate->dependencies == dependencies);
        CHECK(intermediate->finaliseCount == 0);

        finalise_context finaliseContext = {};
        finaliseContext.id = test_id(1);
        finaliseContext.frameIndex = 9;
        core::result<asset_payload> finished = loader.finalise(finaliseContext, std::move(cpu).value());
        CONJURE_REQUIRE_OK(finished);
        CHECK(payload_as<test_asset>(finished.value())->finaliseCount == 1);
        CHECK(payload_as<test_asset>(finished.value())->content == "typed");
    }

    TEST_CASE("typed_asset_loader - a typed load failure propagates and a null CPU output is FINALISE_FAILED") {
        const test_loader loader = {};
        controls().failLoadCpu = true;
        core::result<asset_payload> failed = loader.load_cpu(load_context{}, bytes_of("x"));
        controls().failLoadCpu = false;
        CHECK_FALSE(failed.has_value());
        CHECK(failed.get_error().generic_code() == core::error_code::UNKNOWN);

        core::result<asset_payload> noInput = loader.finalise(finalise_context{}, make_null_payload());
        CONJURE_CHECK_ERROR(noInput, assets_error::FINALISE_FAILED);
    }

    TEST_CASE("typed_asset_loader - a typed finalise failure propagates and the CPU output is released") {
        const test_loader loader = {};
        core::result<asset_payload> cpu = loader.load_cpu(load_context{}, bytes_of("x"));
        CONJURE_REQUIRE_OK(cpu);
        controls().failFinalise = true;
        core::result<asset_payload> failed = loader.finalise(finalise_context{}, std::move(cpu).value());
        controls().failFinalise = false;
        CONJURE_CHECK_ERROR(failed, assets_error::FINALISE_FAILED);
    }

    TEST_CASE("typed_asset_loader - the typed fallback is erased, nullopt means no fallback") {
        const test_loader withFallback = {};
        core::result<asset_payload> fallback = withFallback.create_fallback();
        CONJURE_REQUIRE_OK(fallback);
        REQUIRE(fallback.value() != nullptr);
        CHECK(payload_as<test_asset>(fallback.value())->content == "fallback");

        const other_loader without = {};
        core::result<asset_payload> none = without.create_fallback();
        CONJURE_REQUIRE_OK(none);
        CHECK(none.value() == nullptr);
    }

    TEST_CASE("load_context - carries the dependency order of the blob header") {
        const std::vector<asset_id> order = {test_id(9), test_id(1), test_id(5)};
        load_context context = {};
        context.dependencies = order;
        CHECK(context.dependencies.size() == 3);
        CHECK(context.dependencies[0] == test_id(9));
        CHECK(context.dependencies[2] == test_id(5));
        finalise_context finaliseContext = {};
        CHECK_FALSE(finaliseContext.isReload);
        CHECK(finaliseContext.dependencies.empty());
    }
}
