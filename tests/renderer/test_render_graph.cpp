#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "renderer_test_support.h"

#include "renderer/framebuffer_cache.h"
#include "renderer/gpu_types.h"
#include "renderer/render_graph.h"
#include "renderer/transient_pool.h"

#include "conjure/renderer/renderer.h"

#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/null_device_stats.h"
#include "conjure/rhi/texture_format.h"

#include <nvrhi/nvrhi.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// The render graph (docs/design/02_renderer.md R3, 2.7, 10 "test_render_graph"): declaration
// order is execution order, dead passes are culled backwards, validation rejects reads before
// writes, lifetimes are computed over the live passes and transients come from the pool.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    /// <summary>
    /// A pass whose declarations come from a lambda and whose execution appends its name to a log.
    /// </summary>
    class scripted_pass final : public render_pass {
        private:
            std::string mName = {};
            std::function<void(pass_builder &)> mSetup = {};
            std::function<void(pass_context &)> mExecute = {};
            std::vector<std::string> *mLog = nullptr;

        public:
            scripted_pass(
              std::string name,
              std::vector<std::string> &log,
              std::function<void(pass_builder &)> setup,
              std::function<void(pass_context &)> execute = {}
            ) :
                mName(std::move(name)),
                mSetup(std::move(setup)),
                mExecute(std::move(execute)),
                mLog(&log) {}

            ~scripted_pass() override = default;

            [[nodiscard]] const char *name() const noexcept override {
                return mName.c_str();
            }

            void setup(pass_builder &builder) override {
                mSetup(builder);
            }

            void execute(pass_context &context) override {
                mLog->push_back(mName);
                if (mExecute) {
                    mExecute(context);
                }
            }
    };

    struct graph_fixture {
        std::unique_ptr<rhi::device> device = nullptr;
        std::unique_ptr<framebuffer_cache> framebuffers = nullptr;
        std::unique_ptr<transient_pool> pool = nullptr;
        nvrhi::TextureHandle target = nullptr;
        nvrhi::BufferHandle buffer = nullptr;
        nvrhi::CommandListHandle commandList = nullptr;
        gpu::view_constants constants = {};
        render_stats stats = {};
        std::vector<std::string> log = {};
        render_graph graph = {};

        graph_fixture() {
            device = renderer_support::make_device();
            nvrhi::IDevice &native = *device->native_device();
            framebuffers = std::make_unique<framebuffer_cache>(native);
            pool = std::make_unique<transient_pool>(native, framebuffers.get());
            nvrhi::TextureDesc desc = {};
            desc.width = 64;
            desc.height = 32;
            desc.format = nvrhi::Format::SRGBA8_UNORM;
            desc.isRenderTarget = true;
            desc.initialState = nvrhi::ResourceStates::RenderTarget;
            desc.keepInitialState = true;
            desc.debugName = "graph_target";
            target = native.createTexture(desc);
            REQUIRE(target != nullptr);
            nvrhi::BufferDesc bufferDesc = {};
            bufferDesc.byteSize = 256;
            bufferDesc.canHaveUAVs = true;
            bufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            bufferDesc.keepInitialState = true;
            bufferDesc.debugName = "graph_buffer";
            buffer = native.createBuffer(bufferDesc);
            REQUIRE(buffer != nullptr);
            commandList = native.createCommandList();
            REQUIRE(commandList != nullptr);
        }

        ~graph_fixture() {
            graph.reset();
            pool.reset();
            framebuffers.reset();
        }

        graph_fixture(const graph_fixture &) = delete;
        graph_fixture &operator=(const graph_fixture &) = delete;

        void run(u64 frameIndex = 1) {
            graph.compile(*pool, frameIndex);
            stats.reset();
            commandList->open();
            graph.execute(*commandList, *framebuffers, constants, stats, nullptr);
            commandList->close();
            nvrhi::ICommandList *list = commandList.Get();
            device->native_device()->executeCommandLists(&list, 1);
            pool->end_frame(frameIndex);
        }
    };

    [[nodiscard]] graph_texture_desc colour_desc(const char *name = "colour") {
        graph_texture_desc desc = {};
        desc.width = 64;
        desc.height = 32;
        desc.format = rhi::texture_format::RGBA16_FLOAT;
        desc.isRenderTarget = true;
        desc.debugName = name;
        return desc;
    }
}

TEST_SUITE("renderer/render_graph") {

    TEST_CASE("render_graph - passes execute in declaration order and transients flow between them") {
        graph_fixture fixture = {};
        graph_resource hdr = INVALID_GRAPH_RESOURCE;
        const graph_resource target = fixture.graph.import_texture(fixture.target, "target");
        CHECK(fixture.graph.is_imported(target));
        nvrhi::ITexture *seenInB = nullptr;
        nvrhi::IFramebuffer *framebufferA = nullptr;
        scripted_pass a("a", fixture.log, [&](pass_builder &builder) {
            hdr = builder.create_texture(colour_desc("hdr"));
            builder.colour_target(hdr, 0);
            builder.write(hdr);
        }, [&](pass_context &context) {
            framebufferA = context.framebuffer();
            CHECK(context.texture(hdr) != nullptr);
            CHECK(&context.view_constants() == &fixture.constants);
            ++context.stats().drawCalls;
        });
        scripted_pass b("b", fixture.log, [&](pass_builder &builder) {
            builder.read(hdr);
            builder.colour_target(target, 0);
            builder.write(target);
        }, [&](pass_context &context) {
            seenInB = context.texture(hdr);
            CHECK(context.framebuffer() != nullptr);
            CHECK(context.framebuffer() != framebufferA);
        });
        fixture.graph.add_pass(a);
        fixture.graph.add_pass(b);
        CHECK(fixture.graph.pass_count() == 2);
        CHECK(fixture.graph.resource_count() == 1);   // hdr is created during setup
        fixture.run();
        CHECK(fixture.graph.resource_count() == 2);
        CHECK(fixture.log == std::vector<std::string>{"a", "b"});
        CHECK(fixture.graph.is_pass_live(0));
        CHECK(fixture.graph.is_pass_live(1));
        CHECK_FALSE(fixture.graph.is_pass_live(2));
        CHECK(fixture.stats.passesExecuted == 2);
        CHECK(fixture.stats.passesCulled == 0);
        CHECK(fixture.stats.drawCalls == 1);
        REQUIRE(fixture.stats.passNames.size() == 2);
        CHECK(std::string(fixture.stats.passNames[0]) == "a");
        CHECK(std::string(fixture.stats.passNames[1]) == "b");
        CHECK(fixture.graph.executed_names().size() == 2);
        CHECK(framebufferA != nullptr);
        CHECK(seenInB != nullptr);
        CHECK(seenInB == fixture.graph.texture(hdr));
        CHECK(fixture.graph.texture(target) == fixture.target.Get());
        CHECK(fixture.graph.buffer(hdr) == nullptr);
        CHECK(fixture.graph.texture(INVALID_GRAPH_RESOURCE) == nullptr);
        CHECK_FALSE(fixture.graph.is_imported(hdr));
        CHECK(fixture.graph.live_transient_count() == 1);
        usize first = 9;
        usize last = 9;
        CHECK(fixture.graph.lifetime(hdr, first, last));
        CHECK(first == 0);
        CHECK(last == 1);
        CHECK(fixture.graph.lifetime(target, first, last));
        CHECK(first == 1);
        CHECK(last == 1);
        CHECK_FALSE(fixture.graph.lifetime(INVALID_GRAPH_RESOURCE, first, last));
        // Markers wrap each executed pass.
        const std::optional<rhi::null_device_stats> nullStats = fixture.device->null_stats();
        REQUIRE(nullStats.has_value());
        CHECK(nullStats->markersBegun == 2);
        CHECK(nullStats->markersEnded == 2);
        REQUIRE(nullStats->markerNames.size() == 2);
        CHECK(nullStats->markerNames[0] == "a");
        CHECK(nullStats->markerNames[1] == "b");
    }

    TEST_CASE(
      "render_graph - a pass whose writes nobody reads is culled unless it writes an import or has a side effect"
    ) {
        graph_fixture fixture = {};
        const graph_resource target = fixture.graph.import_texture(fixture.target, "target");
        const graph_resource persistent = fixture.graph.import_buffer(fixture.buffer, "persistent");
        graph_resource unused = INVALID_GRAPH_RESOURCE;
        graph_resource chainA = INVALID_GRAPH_RESOURCE;
        graph_resource chainB = INVALID_GRAPH_RESOURCE;
        scripted_pass dead("dead", fixture.log, [&](pass_builder &builder) {
            unused = builder.create_texture(colour_desc("unused"));
            builder.write(unused);
        });
        scripted_pass deadChainStart("dead_chain_start", fixture.log, [&](pass_builder &builder) {
            chainA = builder.create_texture(colour_desc("chain_a"));
            builder.write(chainA);
        });
        scripted_pass deadChainEnd("dead_chain_end", fixture.log, [&](pass_builder &builder) {
            builder.read(chainA);
            chainB = builder.create_texture(colour_desc("chain_b"));
            builder.write(chainB);
        });
        scripted_pass sideEffect("side_effect", fixture.log, [&](pass_builder &builder) {
            builder.side_effect();
        });
        scripted_pass importWriter("import_writer", fixture.log, [&](pass_builder &builder) {
            builder.write(persistent);
        });
        scripted_pass live("live", fixture.log, [&](pass_builder &builder) {
            builder.read(persistent);
            builder.colour_target(target, 0);
            builder.write(target);
        });
        fixture.graph.add_pass(dead);
        fixture.graph.add_pass(deadChainStart);
        fixture.graph.add_pass(deadChainEnd);
        fixture.graph.add_pass(sideEffect);
        fixture.graph.add_pass(importWriter);
        fixture.graph.add_pass(live);
        fixture.run();
        CHECK(fixture.log == std::vector<std::string>{"side_effect", "import_writer", "live"});
        CHECK(fixture.stats.passesExecuted == 3);
        CHECK(fixture.stats.passesCulled == 3);
        CHECK_FALSE(fixture.graph.is_pass_live(0));
        CHECK_FALSE(fixture.graph.is_pass_live(1));
        CHECK_FALSE(fixture.graph.is_pass_live(2));
        CHECK(fixture.graph.is_pass_live(3));
        CHECK(fixture.graph.is_pass_live(4));
        CHECK(fixture.graph.is_pass_live(5));
        // Culled passes allocate nothing.
        CHECK(fixture.graph.live_transient_count() == 0);
        CHECK(fixture.graph.texture(unused) == nullptr);
        CHECK(fixture.graph.texture(chainB) == nullptr);
        usize first = 0;
        usize last = 0;
        CHECK_FALSE(fixture.graph.lifetime(unused, first, last));
        CHECK(fixture.graph.lifetime(persistent, first, last));
        CHECK(first == 4);
        CHECK(last == 5);
        CHECK(fixture.pool->textures_created() == 0);
    }

    TEST_CASE("render_graph - a read before any write invalidates the pass with an ERROR and an assertion") {
        graph_fixture fixture = {};
        log_capture logs = {};
        logs.allow_errors();
        const graph_resource target = fixture.graph.import_texture(fixture.target, "target");
        graph_resource never = INVALID_GRAPH_RESOURCE;
        scripted_pass declarer("declarer", fixture.log, [&](pass_builder &builder) {
            never = builder.create_texture(colour_desc("never_written"));
            builder.write(target);
        });
        scripted_pass early("early_reader", fixture.log, [&](pass_builder &builder) {
            builder.read(never);
            builder.write(target);
        });
        scripted_pass unknown("unknown_reader", fixture.log, [&](pass_builder &builder) {
            builder.read(static_cast<graph_resource>(1234));
            builder.write(target);
        });
        scripted_pass unknownWriter("unknown_writer", fixture.log, [&](pass_builder &builder) {
            builder.write(static_cast<graph_resource>(4321));
            builder.side_effect();
        });
        scripted_pass fine("fine", fixture.log, [&](pass_builder &builder) {
            builder.write(target);
        });
        fixture.graph.add_pass(declarer);
        fixture.graph.add_pass(early);
        fixture.graph.add_pass(unknown);
        fixture.graph.add_pass(unknownWriter);
        fixture.graph.add_pass(fine);
        {
            expect_assert asserts = {};
            fixture.run();
            CHECK(asserts.count() == (CONJURE_ASSERTS_ENABLED ? 3u : 0u));
        }
        CHECK(fixture.log == std::vector<std::string>{"declarer", "fine"});
        CHECK(fixture.stats.passesExecuted == 2);
        CHECK(fixture.stats.passesCulled == 3);
        CHECK(logs.count(core::log_level::ERR) == 3);
        CHECK(logs.contains(core::log_level::ERR, "early_reader"));
        CHECK(logs.contains(core::log_level::ERR, "before any pass wrote it"));
        CHECK(logs.contains(core::log_level::ERR, "unknown_reader"));
        CHECK(logs.contains(core::log_level::ERR, "unknown_writer"));
        // A transient written by an earlier pass is a valid read, even across a culled pass.
        fixture.graph.reset();
        fixture.log.clear();
        const graph_resource targetAgain = fixture.graph.import_texture(fixture.target, "target");
        graph_resource written = INVALID_GRAPH_RESOURCE;
        scripted_pass writer("writer", fixture.log, [&](pass_builder &builder) {
            written = builder.create_texture(colour_desc("written"));
            builder.write(written);
        });
        scripted_pass culled("culled", fixture.log, [&](pass_builder &builder) {
            builder.read(written);
            static_cast<void>(builder.create_texture(colour_desc("nothing")));
        });
        scripted_pass reader("reader", fixture.log, [&](pass_builder &builder) {
            builder.read(written);
            builder.write(targetAgain);
        });
        fixture.graph.add_pass(writer);
        fixture.graph.add_pass(culled);
        fixture.graph.add_pass(reader);
        fixture.run(2);
        CHECK(fixture.log == std::vector<std::string>{"writer", "reader"});
        CHECK(logs.count(core::log_level::ERR) == 3);
    }

    TEST_CASE("render_graph - reset forgets passes and resources and a rebuilt frame reuses the pooled transients") {
        graph_fixture fixture = {};
        graph_resource hdr = INVALID_GRAPH_RESOURCE;
        // Passes are non-owning: the graph is rebuilt from the same objects every frame.
        const graph_resource target = fixture.graph.import_texture(fixture.target, "target");
        scripted_pass x("x", fixture.log, [&](pass_builder &builder) {
            hdr = builder.create_texture(colour_desc("hdr"));
            builder.write(hdr);
            builder.colour_target(hdr, 0);
        }, [](pass_context &context) {
            CHECK(context.framebuffer() != nullptr);
        });
        scripted_pass y("y", fixture.log, [&](pass_builder &builder) {
            builder.read(hdr);
            builder.write(target);
            builder.colour_target(target, 0);
        }, [](pass_context &context) {
            CHECK(context.framebuffer() != nullptr);
        });
        fixture.graph.add_pass(x);
        fixture.graph.add_pass(y);
        fixture.run(1);
        CHECK(fixture.pool->textures_created() == 1);
        nvrhi::ITexture *firstTexture = fixture.graph.texture(hdr);
        CHECK(fixture.framebuffers->size() == 2);

        fixture.graph.reset();
        CHECK(fixture.graph.pass_count() == 0);
        CHECK(fixture.graph.resource_count() == 0);
        CHECK(fixture.graph.live_transient_count() == 0);
        const graph_resource targetAgain = fixture.graph.import_texture(fixture.target, "target");
        CHECK(targetAgain == target);
        fixture.graph.add_pass(x);
        fixture.graph.add_pass(y);
        fixture.log.clear();
        fixture.run(2);
        CHECK(fixture.log == std::vector<std::string>{"x", "y"});
        CHECK(fixture.pool->textures_created() == 1);
        CHECK(fixture.graph.texture(hdr) == firstTexture);
        CHECK(fixture.framebuffers->size() == 2);
        CHECK(fixture.framebuffers->hits() >= 2);
    }

    TEST_CASE("render_graph - two live transients of the same description get distinct textures within a frame") {
        graph_fixture fixture = {};
        const graph_resource target = fixture.graph.import_texture(fixture.target, "target");
        graph_resource first = INVALID_GRAPH_RESOURCE;
        graph_resource second = INVALID_GRAPH_RESOURCE;
        graph_resource bufferResource = INVALID_GRAPH_RESOURCE;
        scripted_pass producer("producer", fixture.log, [&](pass_builder &builder) {
            first = builder.create_texture(colour_desc("first"));
            second = builder.create_texture(colour_desc("second"));
            graph_buffer_desc bufferDesc = {};
            bufferDesc.byteSize = 1024;
            bufferDesc.structStride = 16;
            bufferDesc.isUnorderedAccess = true;
            bufferResource = builder.create_buffer(bufferDesc);
            builder.write(first);
            builder.write(second);
            builder.write(bufferResource);
        });
        scripted_pass consumer("consumer", fixture.log, [&](pass_builder &builder) {
            builder.read(first);
            builder.read(second);
            builder.read(bufferResource);
            builder.write(target);
            builder.colour_target(target, 0);
        });
        fixture.graph.add_pass(producer);
        fixture.graph.add_pass(consumer);
        fixture.run();
        CHECK(fixture.graph.texture(first) != nullptr);
        CHECK(fixture.graph.texture(second) != nullptr);
        CHECK(fixture.graph.texture(first) != fixture.graph.texture(second));
        CHECK(fixture.graph.buffer(bufferResource) != nullptr);
        CHECK(fixture.graph.texture(bufferResource) == nullptr);
        CHECK(fixture.graph.live_transient_count() == 2);
        CHECK(fixture.pool->textures_created() == 2);
        CHECK(fixture.pool->buffers_created() == 1);
        CHECK(fixture.log.size() == 2);
    }

    TEST_CASE("regression - a transient the device refuses skips its passes with an ERROR and no assertion") {
        graph_fixture fixture = {};
        log_capture logs = {};
        logs.allow_errors();
        const graph_resource target = fixture.graph.import_texture(fixture.target, "target");
        graph_resource refused = INVALID_GRAPH_RESOURCE;
        scripted_pass producer("producer", fixture.log, [&](pass_builder &builder) {
            graph_texture_desc impossible = colour_desc("impossible");
            impossible.width = 0;   // the null device refuses a zero-sized texture
            refused = builder.create_texture(impossible);
            builder.write(refused);
        });
        scripted_pass consumer("consumer", fixture.log, [&](pass_builder &builder) {
            builder.read(refused);
            builder.write(target);
        });
        scripted_pass fine("fine", fixture.log, [&](pass_builder &builder) {
            builder.write(target);
        });
        fixture.graph.add_pass(producer);
        fixture.graph.add_pass(consumer);
        fixture.graph.add_pass(fine);
        {
            expect_assert asserts = {};
            fixture.run();
            CHECK(asserts.count() == 0);
        }
        CHECK(fixture.log == std::vector<std::string>{"fine"});
        CHECK(fixture.stats.passesExecuted == 1);
        CHECK(fixture.stats.passesCulled == 2);
        CHECK_FALSE(fixture.graph.is_pass_live(0));
        CHECK_FALSE(fixture.graph.is_pass_live(1));
        CHECK(fixture.graph.is_pass_live(2));
        CHECK(fixture.graph.texture(refused) == nullptr);
        CHECK(logs.contains(core::log_level::ERR, "could not be allocated"));
        CHECK(logs.contains(core::log_level::ERR, "producer"));
    }

    TEST_CASE("render_graph - a pass without targets has no framebuffer and executes before compile is refused") {
        graph_fixture fixture = {};
        scripted_pass compute("compute", fixture.log, [&](pass_builder &builder) {
            builder.side_effect();
        }, [&](pass_context &context) {
            CHECK(context.framebuffer() == nullptr);
            ++context.stats().dispatches;
        });
        fixture.graph.add_pass(compute);
        fixture.run();
        CHECK(fixture.stats.dispatches == 1);
        CHECK(fixture.log == std::vector<std::string>{"compute"});
        // An empty graph compiles and executes to nothing.
        fixture.graph.reset();
        fixture.log.clear();
        fixture.run(2);
        CHECK(fixture.log.empty());
        CHECK(fixture.stats.passesExecuted == 0);
    }
}
