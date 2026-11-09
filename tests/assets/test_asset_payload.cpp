#include "conjure/testing/doctest.h"
#include "conjure/testing/memory_guard.h"

#include "conjure/assets/asset_payload.h"

#include <string>
#include <utility>

using namespace conjure;
using namespace conjure::assets;

namespace {
    struct counted {
        static inline int sLive = 0;
        std::string name = {};

        explicit counted(std::string text) :
            name(std::move(text)) {
            ++sLive;
        }
        counted(const counted &other) :
            name(other.name) {
            ++sLive;
        }
        counted(counted &&other) noexcept :
            name(std::move(other.name)) {
            ++sLive;
        }
        counted &operator=(const counted &) = default;
        counted &operator=(counted &&) noexcept = default;
        ~counted() {
            --sLive;
        }
    };
}

TEST_SUITE("assets/asset_payload") {

    TEST_CASE("make_payload - owns the value and destroys it through its real type") {
        counted::sLive = 0;
        {
            asset_payload payload = make_payload(counted("a"));
            CHECK(payload != nullptr);
            CHECK(counted::sLive == 1);
            CHECK(payload_as<counted>(payload)->name == "a");
        }
        CHECK(counted::sLive == 0);
    }

    TEST_CASE("make_payload - copies an lvalue and leaves the original intact") {
        counted::sLive = 0;
        counted original("keep");
        {
            const asset_payload payload = make_payload(original);
            CHECK(payload_as<counted>(payload)->name == "keep");
            CHECK(original.name == "keep");
            CHECK(counted::sLive == 2);
        }
        CHECK(counted::sLive == 1);
    }

    TEST_CASE("make_null_payload - owns nothing and is safe to destroy or reset") {
        asset_payload payload = make_null_payload();
        CHECK(payload == nullptr);
        CHECK(payload.get() == nullptr);
        payload.reset();
        CHECK(payload == nullptr);
    }

    TEST_CASE("asset_payload - move transfers ownership exactly once") {
        counted::sLive = 0;
        asset_payload first = make_payload(counted("x"));
        asset_payload second = std::move(first);
        CHECK(first == nullptr);
        CHECK(second != nullptr);
        CHECK(counted::sLive == 1);
        second = make_null_payload();
        CHECK(counted::sLive == 0);
    }

    TEST_CASE("asset_payload - no allocation is leaked") {
        conjure::testing::memory_guard guard = {};
        {
            asset_payload payload = make_payload(std::string(64, 'z'));
            CHECK(payload_as<std::string>(payload)->size() == 64);
        }
        guard.check();
    }
}
