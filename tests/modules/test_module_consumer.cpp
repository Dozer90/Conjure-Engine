#include "conjure/testing/doctest.h"

#include "conjure/modules/module_consumer.h"
#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/result.h"
#include "conjure/core/string_id.h"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    std::span<const std::byte> bytes_of(std::string_view text) {
        return std::span<const std::byte>(reinterpret_cast<const std::byte *>(text.data()), text.size());
    }

    /// <summary>
    /// The smallest possible consumer: inherits every default the base class offers.
    /// </summary>
    class minimal_consumer final : public module_consumer {
        public:
            minimal_consumer() = default;
            ~minimal_consumer() override = default;

        public:
            [[nodiscard]] string_id name() const noexcept override {
                return string_id("minimal");
            }

            [[nodiscard]] result<void> validate(
              module_id,
              const module_registration_set &,
              const module_registry &
            ) override {
                return {};
            }

            void attach(module_id, module_context &, const module_registration_set &) override {}

            [[nodiscard]] result<void> can_detach(module_id, unload_policy) override {
                return {};
            }

            void detach(module_id, reload_context *) override {}

            void restore(module_id, const reload_context &) override {}

            [[nodiscard]] u32 live_use_count(module_id) const override {
                return 0;
            }
    };
}

TEST_SUITE("modules/module_consumer") {

    TEST_CASE("reload_context - store, fetch and contains round trip blobs by key") {
        reload_context context = {};
        CHECK(context.size() == 0);
        CHECK_FALSE(context.contains(string_id("ecs.spinner")));
        CHECK(context.fetch(string_id("ecs.spinner")).empty());

        context.store(string_id("ecs.spinner"), bytes_of("spinner-bytes"));
        context.store(string_id("assets.cache"), bytes_of("cache"));
        CHECK(context.size() == 2);
        CHECK(context.contains(string_id("ecs.spinner")));

        const std::span<const std::byte> fetched = context.fetch(string_id("ecs.spinner"));
        REQUIRE(fetched.size() == 13);
        CHECK(std::string_view(reinterpret_cast<const char *>(fetched.data()), fetched.size()) == "spinner-bytes");
    }

    TEST_CASE("reload_context - storing under an existing key replaces the blob and empty blobs are kept") {
        reload_context context = {};
        context.store(string_id("k"), bytes_of("first"));
        context.store(string_id("k"), bytes_of("second!"));
        CHECK(context.size() == 1);
        CHECK(context.fetch(string_id("k")).size() == 7);

        context.store(string_id("empty"), {});
        CHECK(context.contains(string_id("empty")));
        CHECK(context.fetch(string_id("empty")).empty());
        CHECK(context.size() == 2);
    }

    TEST_CASE("reload_context - the fetched span is a copy independent of the source buffer") {
        reload_context context = {};
        std::array<std::byte, 3> source = {std::byte{1}, std::byte{2}, std::byte{3}};
        context.store(string_id("copy"), source);
        source[0] = std::byte{9};
        const std::span<const std::byte> fetched = context.fetch(string_id("copy"));
        REQUIRE(fetched.size() == 3);
        CHECK(fetched[0] == std::byte{1});
        CHECK(fetched.data() != source.data());
    }

    TEST_CASE("module_consumer - does not take part in managed reloads unless it says so") {
        minimal_consumer consumer = {};
        CHECK_FALSE(consumer.participates_in_managed_reload());
        CHECK(consumer.name() == string_id("minimal"));
        CHECK(consumer.live_use_count(module_id{}) == 0);
        CHECK(consumer.can_detach(module_id{}, unload_policy::REFUSE_IF_IN_USE).has_value());
        const module_consumer &base = consumer;
        CHECK_FALSE(base.participates_in_managed_reload());
    }
}
