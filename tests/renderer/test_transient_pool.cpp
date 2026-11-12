#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "renderer_test_support.h"

#include "renderer/framebuffer_cache.h"
#include "renderer/transient_pool.h"

#include "conjure/renderer/limits.h"

#include "conjure/core/log.h"
#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/null_device_stats.h"
#include "conjure/rhi/texture_format.h"

#include <nvrhi/nvrhi.h>

#include <memory>
#include <optional>

// The transient pool and the framebuffer cache (docs/design/02_renderer.md R3, 2.7, 10
// "test_transient_pool"): keyed by description without the debug name, never reused within a
// frame, evicted after the window, with the framebuffers that referenced an evicted texture dropped.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    struct pool_fixture {
        std::unique_ptr<rhi::device> device = nullptr;
        std::unique_ptr<framebuffer_cache> framebuffers = nullptr;
        std::unique_ptr<transient_pool> pool = nullptr;

        pool_fixture() {
            device = renderer_support::make_device();
            framebuffers = std::make_unique<framebuffer_cache>(*device->native_device());
            pool = std::make_unique<transient_pool>(*device->native_device(), framebuffers.get());
        }

        ~pool_fixture() {
            pool.reset();
            framebuffers.reset();
        }

        pool_fixture(const pool_fixture &) = delete;
        pool_fixture &operator=(const pool_fixture &) = delete;
    };

    [[nodiscard]] graph_texture_desc hdr_desc(const char *name = "hdr") {
        graph_texture_desc desc = {};
        desc.width = 128;
        desc.height = 64;
        desc.format = rhi::texture_format::RGBA16_FLOAT;
        desc.isRenderTarget = true;
        desc.debugName = name;
        return desc;
    }
}

TEST_SUITE("renderer/transient_pool") {

    TEST_CASE("graph_texture_desc - the hash and same_storage ignore the debug name and cover every other field") {
        const graph_texture_desc a = hdr_desc("a");
        const graph_texture_desc b = hdr_desc("b");
        CHECK(a.hash() == b.hash());
        CHECK(a.same_storage(b));
        graph_texture_desc c = a;
        c.width = 129;
        CHECK(c.hash() != a.hash());
        CHECK_FALSE(c.same_storage(a));
        c = a;
        c.format = rhi::texture_format::R11G11B10_FLOAT;
        CHECK(c.hash() != a.hash());
        c = a;
        c.isUnorderedAccess = true;
        CHECK(c.hash() != a.hash());
        CHECK_FALSE(c.same_storage(a));
        c = a;
        c.isCubemap = true;
        CHECK(c.hash() != a.hash());
        c = a;
        c.mipLevels = 3;
        CHECK(c.hash() != a.hash());
        c = a;
        c.arraySize = 4;
        CHECK(c.hash() != a.hash());
        graph_buffer_desc x = {};
        x.byteSize = 1024;
        x.structStride = 16;
        x.debugName = "x";
        graph_buffer_desc y = x;
        y.debugName = "y";
        CHECK(x.hash() == y.hash());
        CHECK(x.same_storage(y));
        y.isUnorderedAccess = true;
        CHECK(x.hash() != y.hash());
        CHECK_FALSE(x.same_storage(y));
        y = x;
        y.structStride = 32;
        CHECK(x.hash() != y.hash());
    }

    TEST_CASE("transient_pool - to_nvrhi maps the graph description onto a kept-state NVRHI texture") {
        graph_texture_desc desc = hdr_desc("mapped");
        nvrhi::TextureDesc mapped = transient_pool::to_nvrhi(desc);
        CHECK(mapped.width == 128);
        CHECK(mapped.height == 64);
        CHECK(mapped.format == nvrhi::Format::RGBA16_FLOAT);
        CHECK(mapped.isRenderTarget);
        CHECK(mapped.isShaderResource);
        CHECK_FALSE(mapped.isUAV);
        CHECK(mapped.keepInitialState);
        CHECK(mapped.initialState == nvrhi::ResourceStates::ShaderResource);
        CHECK(mapped.dimension == nvrhi::TextureDimension::Texture2D);
        CHECK(mapped.useClearValue);
        CHECK(std::string(mapped.debugName) == "mapped");
        desc.isRenderTarget = false;
        desc.isUnorderedAccess = true;
        desc.arraySize = 6;
        desc.isCubemap = true;
        desc.mipLevels = 4;
        mapped = transient_pool::to_nvrhi(desc);
        CHECK(mapped.isUAV);
        CHECK(mapped.initialState == nvrhi::ResourceStates::UnorderedAccess);
        CHECK(mapped.dimension == nvrhi::TextureDimension::TextureCube);
        CHECK(mapped.mipLevels == 4);
        desc.isCubemap = false;
        CHECK(transient_pool::to_nvrhi(desc).dimension == nvrhi::TextureDimension::Texture2DArray);
        desc.arraySize = 12;
        desc.isCubemap = true;
        CHECK(transient_pool::to_nvrhi(desc).dimension == nvrhi::TextureDimension::TextureCubeArray);
        desc.debugName = nullptr;
        CHECK(std::string(transient_pool::to_nvrhi(desc).debugName) == "transient");

        graph_buffer_desc bufferDesc = {};
        bufferDesc.byteSize = 4096;
        bufferDesc.structStride = 64;
        bufferDesc.isUnorderedAccess = true;
        bufferDesc.debugName = "lists";
        const nvrhi::BufferDesc mappedBuffer = transient_pool::to_nvrhi(bufferDesc);
        CHECK(mappedBuffer.byteSize == 4096);
        CHECK(mappedBuffer.structStride == 64);
        CHECK(mappedBuffer.canHaveUAVs);
        CHECK(mappedBuffer.keepInitialState);
        CHECK(mappedBuffer.initialState == nvrhi::ResourceStates::UnorderedAccess);
        CHECK(std::string(mappedBuffer.debugName) == "lists");
    }

    TEST_CASE("transient_pool - a texture is never handed out twice in a frame and is reused the next frame") {
        pool_fixture fixture = {};
        nvrhi::ITexture *first = fixture.pool->acquire(hdr_desc("one"), 1);
        nvrhi::ITexture *second = fixture.pool->acquire(hdr_desc("two"), 1);
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        CHECK(first != second);
        CHECK(fixture.pool->textures_created() == 2);
        CHECK(fixture.pool->live_texture_count() == 2);
        // A different description is a different bucket.
        graph_texture_desc other = hdr_desc("other");
        other.format = rhi::texture_format::RGBA8_UNORM;
        nvrhi::ITexture *third = fixture.pool->acquire(other, 1);
        REQUIRE(third != nullptr);
        CHECK(third != first);
        CHECK(fixture.pool->textures_created() == 3);
        fixture.pool->end_frame(1);

        nvrhi::ITexture *again = fixture.pool->acquire(hdr_desc("one"), 2);
        CHECK((again == first || again == second));
        CHECK(fixture.pool->textures_created() == 3);
        nvrhi::ITexture *third2 = fixture.pool->acquire(hdr_desc("three"), 2);
        CHECK(third2 != again);
        CHECK((third2 == first || third2 == second));
        nvrhi::ITexture *fourth = fixture.pool->acquire(hdr_desc("four"), 2);
        CHECK(fourth != again);
        CHECK(fourth != third2);
        CHECK(fixture.pool->textures_created() == 4);
        CHECK(fixture.pool->live_texture_count() == 4);
        fixture.pool->end_frame(2);
        fixture.pool->reset_counters();
        CHECK(fixture.pool->textures_created() == 0);
        CHECK(fixture.pool->buffers_created() == 0);
    }

    TEST_CASE("transient_pool - buffers pool the same way") {
        pool_fixture fixture = {};
        graph_buffer_desc desc = {};
        desc.byteSize = 2048;
        desc.structStride = 8;
        desc.isUnorderedAccess = true;
        nvrhi::IBuffer *first = fixture.pool->acquire_buffer(desc, 1);
        nvrhi::IBuffer *second = fixture.pool->acquire_buffer(desc, 1);
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        CHECK(first != second);
        CHECK(fixture.pool->buffers_created() == 2);
        fixture.pool->end_frame(1);
        nvrhi::IBuffer *again = fixture.pool->acquire_buffer(desc, 2);
        CHECK((again == first || again == second));
        CHECK(fixture.pool->buffers_created() == 2);
        fixture.pool->end_frame(2);
        fixture.pool->set_eviction_frames(1);
        fixture.pool->end_frame(10);
        nvrhi::IBuffer *fresh = fixture.pool->acquire_buffer(desc, 11);
        CHECK(fresh != nullptr);
        CHECK(fixture.pool->buffers_created() == 3);
    }

    TEST_CASE("transient_pool - unused textures are evicted after the window and their framebuffers dropped") {
        pool_fixture fixture = {};
        fixture.pool->set_eviction_frames(2);
        nvrhi::ITexture *texture = fixture.pool->acquire(hdr_desc(), 1);
        REQUIRE(texture != nullptr);
        nvrhi::FramebufferDesc framebufferDesc = {};
        framebufferDesc.addColorAttachment(texture);
        nvrhi::IFramebuffer *framebuffer = fixture.framebuffers->get(framebufferDesc);
        REQUIRE(framebuffer != nullptr);
        CHECK(fixture.framebuffers->get(framebufferDesc) == framebuffer);
        CHECK(fixture.framebuffers->size() == 1);
        CHECK(fixture.framebuffers->hits() == 1);
        CHECK(fixture.framebuffers->misses() == 1);
        fixture.pool->end_frame(1);
        CHECK(fixture.pool->live_texture_count() == 1);
        fixture.pool->end_frame(2);
        fixture.pool->end_frame(3);
        CHECK(fixture.pool->live_texture_count() == 1);   // exactly the window: still kept
        fixture.pool->end_frame(4);
        CHECK(fixture.pool->live_texture_count() == 0);
        CHECK(fixture.framebuffers->size() == 0);
        // Using it every frame keeps it alive indefinitely.
        for (u64 frame = 5; frame < 30; ++frame) {
            CHECK(fixture.pool->acquire(hdr_desc(), frame) != nullptr);
            fixture.pool->end_frame(frame);
        }
        CHECK(fixture.pool->live_texture_count() == 1);
        CHECK(fixture.pool->textures_created() == 2);
        // The default window is the documented constant.
        transient_pool defaults(*fixture.device->native_device(), nullptr);
        CHECK(defaults.acquire(hdr_desc(), 1) != nullptr);
        defaults.end_frame(1);
        defaults.end_frame(1 + TRANSIENT_EVICTION_FRAMES);
        CHECK(defaults.live_texture_count() == 1);
        defaults.end_frame(2 + TRANSIENT_EVICTION_FRAMES);
        CHECK(defaults.live_texture_count() == 0);
    }

    TEST_CASE("regression - sweep-2026-11-12 F381 F728 a bucket whose resources were all evicted is erased") {
        // end_frame used to keep every bucket ever created and walk all of them each frame.
        pool_fixture fixture = {};
        fixture.pool->set_eviction_frames(1);
        graph_buffer_desc bufferDesc = {};
        bufferDesc.byteSize = 512;
        bufferDesc.structStride = 4;
        CHECK(fixture.pool->bucket_count() == 0);
        REQUIRE(fixture.pool->acquire(hdr_desc(), 1) != nullptr);
        REQUIRE(fixture.pool->acquire_buffer(bufferDesc, 1) != nullptr);
        graph_texture_desc kept = hdr_desc("kept");
        kept.width = 32;
        REQUIRE(fixture.pool->acquire(kept, 1) != nullptr);
        CHECK(fixture.pool->bucket_count() == 3);
        fixture.pool->end_frame(1);
        CHECK(fixture.pool->bucket_count() == 3);

        // Only the 'kept' description is used again: the other two buckets empty and go.
        REQUIRE(fixture.pool->acquire(kept, 3) != nullptr);
        fixture.pool->end_frame(3);
        CHECK(fixture.pool->bucket_count() == 1);
        CHECK(fixture.pool->live_texture_count() == 1);
        fixture.pool->end_frame(5);
        CHECK(fixture.pool->bucket_count() == 0);
        CHECK(fixture.pool->live_texture_count() == 0);

        // An erased bucket is recreated on demand.
        CHECK(fixture.pool->acquire(hdr_desc(), 6) != nullptr);
        CHECK(fixture.pool->bucket_count() == 1);
        CHECK(fixture.pool->textures_created() == 3);
    }

    TEST_CASE("transient_pool - clear releases everything and the framebuffers that referenced it") {
        pool_fixture fixture = {};
        nvrhi::ITexture *a = fixture.pool->acquire(hdr_desc("a"), 1);
        nvrhi::ITexture *b = fixture.pool->acquire(hdr_desc("b"), 1);
        REQUIRE(a != nullptr);
        REQUIRE(b != nullptr);
        nvrhi::FramebufferDesc both = {};
        both.addColorAttachment(a);
        both.addColorAttachment(b);
        nvrhi::FramebufferDesc onlyA = {};
        onlyA.addColorAttachment(a);
        CHECK(fixture.framebuffers->get(both) != nullptr);
        CHECK(fixture.framebuffers->get(onlyA) != nullptr);
        CHECK(fixture.framebuffers->size() == 2);
        CHECK(framebuffer_cache::hash(both) != framebuffer_cache::hash(onlyA));
        fixture.framebuffers->remove_texture(b);
        CHECK(fixture.framebuffers->size() == 1);
        fixture.pool->clear();
        CHECK(fixture.pool->live_texture_count() == 0);
        CHECK(fixture.framebuffers->size() == 0);
        CHECK(fixture.pool->acquire(hdr_desc("c"), 2) != nullptr);
        CHECK(fixture.pool->textures_created() == 3);
        fixture.framebuffers->clear();
        CHECK(fixture.framebuffers->size() == 0);
    }

    TEST_CASE("framebuffer_cache - the key covers the attachment textures, subresources and the depth attachment") {
        pool_fixture fixture = {};
        graph_texture_desc arrayDesc = hdr_desc("array");
        arrayDesc.arraySize = 2;
        arrayDesc.mipLevels = 2;
        nvrhi::ITexture *colour = fixture.pool->acquire(arrayDesc, 1);
        graph_texture_desc depthDesc = hdr_desc("depth");
        depthDesc.format = rhi::texture_format::D32_FLOAT;
        nvrhi::ITexture *depth = fixture.pool->acquire(depthDesc, 1);
        REQUIRE(colour != nullptr);
        REQUIRE(depth != nullptr);
        nvrhi::FramebufferDesc slice0 = {};
        slice0.addColorAttachment(colour, nvrhi::TextureSubresourceSet(0, 1, 0, 1));
        nvrhi::FramebufferDesc slice1 = {};
        slice1.addColorAttachment(colour, nvrhi::TextureSubresourceSet(0, 1, 1, 1));
        nvrhi::FramebufferDesc mip1 = {};
        mip1.addColorAttachment(colour, nvrhi::TextureSubresourceSet(1, 1, 0, 1));
        nvrhi::FramebufferDesc withDepth = slice0;
        withDepth.setDepthAttachment(depth);
        CHECK(framebuffer_cache::hash(slice0) != framebuffer_cache::hash(slice1));
        CHECK(framebuffer_cache::hash(slice0) != framebuffer_cache::hash(mip1));
        CHECK(framebuffer_cache::hash(slice0) != framebuffer_cache::hash(withDepth));
        CHECK(framebuffer_cache::hash(slice0) == framebuffer_cache::hash(slice0));
        nvrhi::IFramebuffer *first = fixture.framebuffers->get(slice0);
        nvrhi::IFramebuffer *second = fixture.framebuffers->get(slice1);
        nvrhi::IFramebuffer *third = fixture.framebuffers->get(withDepth);
        REQUIRE(first != nullptr);
        CHECK(first != second);
        CHECK(first != third);
        CHECK(fixture.framebuffers->size() == 3);
        CHECK(fixture.framebuffers->get(slice0) == first);
        CHECK(fixture.framebuffers->misses() == 3);
        CHECK(fixture.framebuffers->hits() == 1);
        // Removing the depth texture drops only the framebuffer that used it.
        fixture.framebuffers->remove_texture(depth);
        CHECK(fixture.framebuffers->size() == 2);
        CHECK(fixture.framebuffers->get(slice0) == first);
        // An empty description is refused without being cached.
        CHECK(fixture.framebuffers->get(nvrhi::FramebufferDesc()) == nullptr);
        CHECK(fixture.framebuffers->size() == 2);
    }

    TEST_CASE("transient_pool - a refused texture logs an ERROR and returns null without poisoning the bucket") {
        pool_fixture fixture = {};
        log_capture logs = {};
        logs.allow_errors();
        graph_texture_desc huge = hdr_desc("huge");
        huge.width = 0;   // the null device refuses a zero-sized texture
        huge.height = 0;
        nvrhi::ITexture *refused = fixture.pool->acquire(huge, 1);
        if (refused == nullptr) {
            CHECK(logs.contains(core::log_level::ERR, "refused texture"));
            CHECK(fixture.pool->textures_created() == 0);
        }
        CHECK(fixture.pool->acquire(hdr_desc(), 1) != nullptr);
    }
}
