#include "conjure/testing/doctest.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_pack.h"
#include "conjure/assets/asset_source.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    /// <summary>
    /// Switches shared between a parking_source and the test that drives it.
    /// </summary>
    struct parking_controls {
        std::atomic<bool> parkReads = false;             // read() spins while set
        std::atomic<u32> readsInProgress = 0;
        std::atomic<bool> destroyedDuringRead = false;   // the source died while a read() was running
    };

    /// <summary>
    /// A memory source whose read() can be parked, and which records whether it was destroyed
    /// while a read was still running (the use-after-free swap_source() used to allow).
    /// </summary>
    class parking_source final : public asset_source {
        private:
            memory_asset_source mInner = {};
            parking_controls *mControls = nullptr;

        public:
            explicit parking_source(parking_controls &controls) :
                mControls(&controls) {}

            ~parking_source() override {
                if (mControls->readsInProgress.load(std::memory_order_acquire) != 0) {
                    mControls->destroyedDuringRead.store(true, std::memory_order_release);
                }
            }

        public:
            void add(cooked_blob blob) {
                mInner.add(std::move(blob));
            }

            [[nodiscard]] bool contains(const asset_id &id) const noexcept override {
                return mInner.contains(id);
            }

            [[nodiscard]] std::optional<asset_type_id> type_of(const asset_id &id) const noexcept override {
                return mInner.type_of(id);
            }

            [[nodiscard]] std::optional<asset_id> find_by_path(std::string_view assetPath) const override {
                return mInner.find_by_path(assetPath);
            }

            [[nodiscard]] core::result<cooked_blob> read(const asset_id &id) const override {
                mControls->readsInProgress.fetch_add(1, std::memory_order_acq_rel);
                while (mControls->parkReads.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                core::result<cooked_blob> blob = mInner.read(id);
                mControls->readsInProgress.fetch_sub(1, std::memory_order_acq_rel);
                return blob;
            }
    };

    /// <summary>
    /// The contract every source must honour for a source holding exactly 'known' (content
    /// "known", path "assets://known.ta") and nothing else.
    /// </summary>
    void check_source_contract(const asset_source &source, const asset_id &known, const asset_id &unknown) {
        CHECK(source.contains(known));
        CHECK_FALSE(source.contains(unknown));
        CHECK(source.type_of(known) == asset_type_traits<test_asset>::id());
        CHECK_FALSE(source.type_of(unknown).has_value());
        CHECK(source.find_by_path("assets://known.ta") == known);
        CHECK_FALSE(source.find_by_path("assets://missing.ta").has_value());

        core::result<cooked_blob> blob = source.read(known);
        CONJURE_REQUIRE_OK(blob);
        CHECK(blob.value().header.id == known);
        CHECK(blob.value().header.type == asset_type_traits<test_asset>::id());
        CHECK(text_of(blob.value().payload) == "known");
        CONJURE_CHECK_ERROR(source.read(unknown), asset_error_code::NOT_FOUND);
    }
}

TEST_SUITE("assets/asset_source") {

    TEST_CASE("memory_asset_source - contract, add, replace and remove") {
        memory_asset_source source = {};
        const asset_id known = test_id(1);
        source.add(make_test_blob(known, "known"), "assets://known.ta");
        CHECK(source.size() == 1);
        check_source_contract(source, known, test_id(2));

        source.add(make_test_blob(known, "replaced"), "assets://renamed.ta");
        CHECK(source.size() == 1);
        CHECK_FALSE(source.find_by_path("assets://known.ta").has_value());
        CHECK(source.find_by_path("assets://renamed.ta") == known);
        CHECK(text_of(source.read(known).value().payload) == "replaced");

        CHECK(source.remove(known));
        CHECK_FALSE(source.remove(known));
        CHECK(source.size() == 0);
        CHECK_FALSE(source.find_by_path("assets://renamed.ta").has_value());
    }

    TEST_CASE("cache_asset_source - contract over the database and the cache") {
        pipeline_fixture fixture = {};
        fixture.write_source("known.ta", "known");
        fixture.scan_and_import();
        const asset_id known = fixture.id_of("known.ta");
        const cache_asset_source source(fixture.cache(), *fixture.database);
        check_source_contract(source, known, test_id(99));
    }

    TEST_CASE("cache_asset_source - record status decides the error of read") {
        pipeline_fixture fixture = {};
        fixture.write_source("ok.ta", "fine");
        fixture.write_source("nope.xyz", "no importer");
        fixture.write_source("broken.ta", "x");
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file("assets/broken.ta.meta", bytes_of("{ corrupt")));
        fixture.scan_and_import();
        const cache_asset_source source(fixture.cache(), *fixture.database);

        CONJURE_CHECK_ERROR(source.read(fixture.id_of("nope.xyz")), asset_error_code::NO_IMPORTER);
        CONJURE_CHECK_ERROR(source.read(fixture.id_of("broken.ta")), asset_error_code::META_INVALID);
        CHECK_FALSE(source.type_of(fixture.id_of("nope.xyz")).has_value());

        // A record whose blob was deleted is a CACHE_MISS, not an unknown asset.
        const asset_id ok = fixture.id_of("ok.ta");
        CONJURE_REQUIRE_OK(fixture.cache().remove(ok));
        CONJURE_CHECK_ERROR(source.read(ok), asset_error_code::CACHE_MISS);
        CHECK(source.contains(ok));
    }

    TEST_CASE("pack_asset_source - contract over an opened pack") {
        core::vfs::memory_file_system fileSystem = {};
        pack_writer writer = {};
        const asset_id known = test_id(1);
        CONJURE_REQUIRE_OK(writer.add(make_test_blob(known, "known"), "assets://known.ta"));
        CONJURE_REQUIRE_OK(writer.write(fileSystem, "out/content.cpak"));
        pack_reader reader = {};
        CONJURE_REQUIRE_OK(reader.open(fileSystem, "out/content.cpak"));
        const pack_asset_source source(reader);
        check_source_contract(source, known, test_id(2));
    }

    TEST_CASE("composite_asset_source - resolves in insertion order and reports the union") {
        auto first = std::make_unique<memory_asset_source>();
        auto second = std::make_unique<memory_asset_source>();
        const asset_id known = test_id(1);
        const asset_id shared = test_id(3);
        const asset_id onlySecond = test_id(4);
        first->add(make_test_blob(known, "known"), "assets://known.ta");
        first->add(make_test_blob(shared, "from first"), "assets://shared.ta");
        second->add(
          make_test_blob(shared, "from second", {}, asset_type_traits<other_asset>::id()),
          "assets://shared2.ta"
        );
        second->add(make_test_blob(onlySecond, "second only"), "assets://only.ta");

        composite_asset_source composite = {};
        CHECK(composite.size() == 0);
        CHECK_FALSE(composite.contains(known));
        CONJURE_CHECK_ERROR(composite.read(known), asset_error_code::NOT_FOUND);
        composite.add(std::move(first));
        composite.add(std::move(second));
        composite.add(nullptr);
        CHECK(composite.size() == 2);

        check_source_contract(composite, known, test_id(2));
        CHECK(text_of(composite.read(shared).value().payload) == "from first");
        CHECK(composite.type_of(shared) == asset_type_traits<test_asset>::id());
        CHECK(composite.find_by_path("assets://shared2.ta") == shared);
        CHECK(composite.find_by_path("assets://only.ta") == onlySecond);
        CHECK(text_of(composite.read(onlySecond).value().payload) == "second only");
    }

    TEST_CASE("asset_manager - swap_source while loaded keeps the payload and serves the new source") {
        pipeline_fixture fixture = {};
        auto first = std::make_unique<memory_asset_source>();
        const asset_id a = test_id(1);
        const asset_id b = test_id(2);
        first->add(make_test_blob(a, "from first"), "assets://a.ta");
        fixture.manager->swap_source(std::move(first));

        asset_handle handleA = fixture.manager->load(a);
        settle(*fixture.manager);
        REQUIRE(handleA.state() == asset_state::LOADED);
        CHECK(fixture.manager->find("assets://a.ta") == a);

        auto second = std::make_unique<memory_asset_source>();
        second->add(make_test_blob(b, "from second"), "assets://b.ta");
        fixture.manager->swap_source(std::move(second));

        CHECK(handleA.get<test_asset>()->content == "from first");
        CHECK_FALSE(fixture.manager->find("assets://a.ta").has_value());
        CHECK(fixture.manager->find("assets://b.ta") == b);
        asset_handle handleB = fixture.manager->load(b);
        settle(*fixture.manager);
        CHECK(handleB.get<test_asset>()->content == "from second");

        // Reloading a slot the new source does not know fails and keeps the old payload.
        fixture.manager->reload(a);
        settle(*fixture.manager);
        CHECK(handleA.state() == asset_state::LOADED);
        CHECK(handleA.get<test_asset>()->content == "from first");
        REQUIRE(fixture.manager->last_error(a) != nullptr);
        CHECK(fixture.manager->last_error(a)->is(asset_error_code::NOT_FOUND));
    }

    TEST_CASE("regression - swap_source waits for a read in flight and the parked load still completes [parallel]") {
        pipeline_fixture fixture(4);
        auto shared = std::make_shared<parking_controls>();
        auto first = std::make_unique<parking_source>(*shared);
        first->add(make_test_blob(test_id(1), "from first"));
        fixture.manager->swap_source(std::move(first));

        // The job enters read() and parks there; a helper job releases it once the main thread
        // has asked for the swap, so the swap either waits for the read or follows it, and in
        // both orders the old source must outlive the read.
        shared->parkReads = true;
        std::atomic<bool> swapRequested = false;
        const core::jobs::job_handle release = fixture.jobs.schedule([&shared, &swapRequested]() {
            while (!swapRequested.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            shared->parkReads.store(false, std::memory_order_release);
        });
        asset_handle handle = fixture.manager->load(test_id(1));
        fixture.manager->update();
        while (shared->readsInProgress.load(std::memory_order_acquire) == 0) {
            std::this_thread::yield();
        }
        CHECK(handle.state() == asset_state::LOADING);

        auto second = std::make_unique<memory_asset_source>();
        second->add(make_test_blob(test_id(2), "from second"), "assets://b.ta");
        swapRequested.store(true, std::memory_order_release);
        fixture.manager->swap_source(std::move(second));
        fixture.jobs.wait(release);
        CHECK_FALSE(shared->destroyedDuringRead.load());
        CHECK(shared->readsInProgress.load() == 0);

        fixture.manager->flush();
        CHECK(handle.state() == asset_state::LOADED);
        REQUIRE(handle.get<test_asset>() != nullptr);
        CHECK(handle.get<test_asset>()->content == "from first");
        CHECK(fixture.manager->stats().jobsInFlight == 0);
        CHECK(fixture.manager->find("assets://b.ta") == test_id(2));

        // A load dispatched after the swap resolves the new source.
        asset_handle late = fixture.manager->load(test_id(2));
        fixture.manager->flush();
        CHECK(late.get<test_asset>()->content == "from second");
    }

    TEST_CASE("regression - swap_source while a loader is parked in load_cpu keeps the blob already read [parallel]") {
        pipeline_fixture fixture(4);
        auto first = std::make_unique<memory_asset_source>();
        first->add(make_test_blob(test_id(1), "from first"));
        fixture.manager->swap_source(std::move(first));

        controls().blockLoadCpu = true;
        asset_handle handle = fixture.manager->load(test_id(1));
        fixture.manager->update();
        while (controls().loadCpuCalls.load() == 0) {
            std::this_thread::yield();
        }
        fixture.manager->swap_source(std::make_unique<memory_asset_source>());   // the first source dies here
        controls().blockLoadCpu = false;
        fixture.manager->flush();
        CHECK(handle.state() == asset_state::LOADED);
        CHECK(handle.get<test_asset>()->content == "from first");
        CHECK(fixture.manager->stats().jobsInFlight == 0);
    }

    TEST_CASE("asset_manager - add_source is searched after the primary") {
        pipeline_fixture fixture = {};
        auto primary = std::make_unique<memory_asset_source>();
        auto plugin = std::make_unique<memory_asset_source>();
        const asset_id shared = test_id(1);
        const asset_id pluginOnly = test_id(2);
        primary->add(make_test_blob(shared, "primary"));
        plugin->add(make_test_blob(shared, "plugin"));
        plugin->add(make_test_blob(pluginOnly, "plugin only"), "assets://plugin.ta");
        fixture.manager->swap_source(std::move(primary));
        fixture.manager->add_source(std::move(plugin));

        asset_handle a = fixture.manager->load(shared);
        asset_handle b = fixture.manager->load(pluginOnly);
        settle(*fixture.manager);
        CHECK(a.get<test_asset>()->content == "primary");
        CHECK(b.get<test_asset>()->content == "plugin only");
        CHECK(fixture.manager->find("assets://plugin.ta") == pluginOnly);
    }
}
