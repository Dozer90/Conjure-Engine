#include "renderer/frame_resources.h"

#include "conjure/renderer/renderer_error.h"

#include "conjure/core/error.h"

#include <nvrhi/utils.h>

#include <algorithm>
#include <string>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr u32 INITIAL_INSTANCE_CAPACITY = 4096;
    constexpr u32 INITIAL_SPRITE_CAPACITY = 1024;        // sprites
    constexpr u32 INITIAL_DEBUG_VERTEX_CAPACITY = 4096;
    constexpr u32 INITIAL_SHADOW_TILE_CAPACITY = 64;

    [[nodiscard]] u32 grow(u32 current, u32 needed) noexcept {
        u32 capacity = std::max<u32>(current, 1);
        while (capacity < needed) {
            capacity = capacity >= 0x40000000u ? needed : capacity * 2;
        }
        return capacity;
    }
}

frame_resources::frame_resources() {}

frame_resources::~frame_resources() {}

nvrhi::BufferHandle frame_resources::create_structured(
  u64 byteSize,
  u32 stride,
  bool unorderedAccess,
  const char *name
) {
    nvrhi::BufferDesc desc = {};
    desc.byteSize = byteSize;
    desc.structStride = stride;
    desc.canHaveUAVs = unorderedAccess;
    desc.debugName = name;
    desc.initialState = unorderedAccess ? nvrhi::ResourceStates::UnorderedAccess
      : nvrhi::ResourceStates::ShaderResource;
    desc.keepInitialState = true;
    nvrhi::BufferHandle buffer = mDevice->createBuffer(desc);
    if (buffer != nullptr) {
        ++mBuffersCreated;
    }
    return buffer;
}

nvrhi::BufferHandle frame_resources::create_vertex(u64 byteSize, const char *name) {
    nvrhi::BufferDesc desc = {};
    desc.byteSize = byteSize;
    desc.isVertexBuffer = true;
    desc.debugName = name;
    desc.initialState = nvrhi::ResourceStates::VertexBuffer;
    desc.keepInitialState = true;
    nvrhi::BufferHandle buffer = mDevice->createBuffer(desc);
    if (buffer != nullptr) {
        ++mBuffersCreated;
    }
    return buffer;
}

nvrhi::BufferHandle frame_resources::create_index(u64 byteSize, const char *name) {
    nvrhi::BufferDesc desc = {};
    desc.byteSize = byteSize;
    desc.isIndexBuffer = true;
    desc.debugName = name;
    desc.initialState = nvrhi::ResourceStates::IndexBuffer;
    desc.keepInitialState = true;
    nvrhi::BufferHandle buffer = mDevice->createBuffer(desc);
    if (buffer != nullptr) {
        ++mBuffersCreated;
    }
    return buffer;
}

core::result<void> frame_resources::create(nvrhi::IDevice &device, u32 cascadeResolution, u32 atlasResolution) {
    mDevice = &device;
    mCascadeResolution = std::clamp<u32>(cascadeResolution, 256, 8192);
    mAtlasResolution = std::clamp<u32>(atlasResolution, MIN_ATLAS_RESOLUTION, MAX_ATLAS_RESOLUTION);
    const auto fail = [](const char *what) {
        return core::make_error(renderer_error::RESOURCE_CREATION_FAILED, "the device refused the {}", what);
    };

    mViewConstants = mDevice->createBuffer(
      nvrhi::utils::CreateVolatileConstantBufferDesc(gpu::VIEW_CONSTANTS_SIZE, "view_constants", VIEW_CONSTANT_VERSIONS)
    );
    if (mViewConstants == nullptr) {
        return fail("view constant buffer");
    }
    ++mBuffersCreated;
    if (!ensure_instance_capacity(INITIAL_INSTANCE_CAPACITY)) {
        return fail("instance buffer");
    }
    if (!ensure_sprite_capacity(INITIAL_SPRITE_CAPACITY * 4, INITIAL_SPRITE_CAPACITY * 6)) {
        return fail("sprite buffers");
    }
    if (!ensure_debug_capacity(INITIAL_DEBUG_VERTEX_CAPACITY)) {
        return fail("debug vertex buffer");
    }
    for (u32 index = 0; index < MAX_VIEWS; ++index) {
        view_resources &view = mViews[index];
        const std::string suffix = "." + std::to_string(index);
        view.lights = create_structured(
          u64{MAX_LIGHTS_PER_VIEW} * gpu::LIGHT_SIZE,
          gpu::LIGHT_SIZE,
          false,
          ("lights" + suffix).c_str()
        );
        view.histogram = create_structured(
          u64{EXPOSURE_HISTOGRAM_BINS} * sizeof(u32),
          sizeof(u32),
          true,
          ("histogram" + suffix).c_str()
        );
        view.exposureState = create_structured(
          gpu::EXPOSURE_STATE_SIZE,
          gpu::EXPOSURE_STATE_SIZE,
          true,
          ("exposure_state" + suffix).c_str()
        );
        if (view.lights == nullptr || view.histogram == nullptr || view.exposureState == nullptr) {
            return fail("per-view light / exposure buffers");
        }
        if (!ensure_shadow_tile_capacity(index, INITIAL_SHADOW_TILE_CAPACITY)) {
            return fail("shadow tile buffer");
        }
    }

    nvrhi::TextureDesc cascades = {};
    cascades.width = mCascadeResolution;
    cascades.height = mCascadeResolution;
    cascades.arraySize = MAX_SHADOW_CASCADES;
    cascades.format = nvrhi::Format::D32;
    cascades.dimension = nvrhi::TextureDimension::Texture2DArray;
    cascades.debugName = "shadow_cascades";
    cascades.isRenderTarget = true;
    cascades.isShaderResource = true;
    cascades.initialState = nvrhi::ResourceStates::ShaderResource;
    cascades.keepInitialState = true;
    cascades.useClearValue = true;
    cascades.clearValue = nvrhi::Color(0.0f);
    mShadowCascades = mDevice->createTexture(cascades);
    if (mShadowCascades == nullptr) {
        return fail("shadow cascade array");
    }

    nvrhi::TextureDesc atlas = cascades;
    atlas.width = mAtlasResolution;
    atlas.height = mAtlasResolution;
    atlas.arraySize = 1;
    atlas.dimension = nvrhi::TextureDimension::Texture2D;
    atlas.debugName = "shadow_atlas";
    mShadowAtlas = mDevice->createTexture(atlas);
    if (mShadowAtlas == nullptr) {
        return fail("shadow atlas");
    }

    nvrhi::TextureDesc lut = {};
    lut.width = BRDF_LUT_SIZE;
    lut.height = BRDF_LUT_SIZE;
    lut.format = nvrhi::Format::RG16_FLOAT;
    lut.dimension = nvrhi::TextureDimension::Texture2D;
    lut.debugName = "brdf_lut";
    lut.isRenderTarget = true;
    lut.isShaderResource = true;
    lut.initialState = nvrhi::ResourceStates::ShaderResource;
    lut.keepInitialState = true;
    mBrdfLut = mDevice->createTexture(lut);
    if (mBrdfLut == nullptr) {
        return fail("BRDF lookup table");
    }
    return core::result<void>();
}

bool frame_resources::ensure_instance_capacity(u32 count) {
    if (mInstances != nullptr && count <= mInstanceCapacity) {
        return true;
    }
    const u32 capacity = grow(mInstanceCapacity, std::max<u32>(count, 1));
    nvrhi::BufferHandle buffer = create_structured(
      u64{capacity} * gpu::INSTANCE_SIZE,
      gpu::INSTANCE_SIZE,
      false,
      "instances"
    );
    if (buffer == nullptr) {
        return false;
    }
    mInstances = std::move(buffer);
    mInstanceCapacity = capacity;
    return true;
}

bool frame_resources::ensure_sprite_capacity(u32 vertexCount, u32 indexCount) {
    if (mSpriteVertices == nullptr || vertexCount > mSpriteVertexCapacity) {
        const u32 capacity = grow(mSpriteVertexCapacity, std::max<u32>(vertexCount, 4));
        nvrhi::BufferHandle buffer = create_vertex(u64{capacity} * gpu::SPRITE_VERTEX_SIZE, "sprite_vertices");
        if (buffer == nullptr) {
            return false;
        }
        mSpriteVertices = std::move(buffer);
        mSpriteVertexCapacity = capacity;
    }
    if (mSpriteIndices == nullptr || indexCount > mSpriteIndexCapacity) {
        const u32 capacity = grow(mSpriteIndexCapacity, std::max<u32>(indexCount, 6));
        nvrhi::BufferHandle buffer = create_index(u64{capacity} * sizeof(u32), "sprite_indices");
        if (buffer == nullptr) {
            return false;
        }
        mSpriteIndices = std::move(buffer);
        mSpriteIndexCapacity = capacity;
    }
    return true;
}

bool frame_resources::ensure_debug_capacity(u32 vertexCount) {
    if (mDebugVertices != nullptr && vertexCount <= mDebugVertexCapacity) {
        return true;
    }
    const u32 capacity = grow(mDebugVertexCapacity, std::max<u32>(vertexCount, 2));
    nvrhi::BufferHandle buffer = create_vertex(u64{capacity} * gpu::DEBUG_VERTEX_SIZE, "debug_vertices");
    if (buffer == nullptr) {
        return false;
    }
    mDebugVertices = std::move(buffer);
    mDebugVertexCapacity = capacity;
    return true;
}

bool frame_resources::ensure_tile_list_capacity(u32 viewIndex, u32 uintCount) {
    view_resources &view = mViews[viewIndex];
    if (view.tileLists != nullptr && uintCount <= view.tileListCapacity) {
        return true;
    }
    const u32 capacity = grow(view.tileListCapacity, std::max<u32>(uintCount, gpu::TILE_LIST_STRIDE));
    const std::string name = "tile_lists." + std::to_string(viewIndex);
    nvrhi::BufferHandle buffer = create_structured(u64{capacity} * sizeof(u32), sizeof(u32), true, name.c_str());
    if (buffer == nullptr) {
        return false;
    }
    view.tileLists = std::move(buffer);
    view.tileListCapacity = capacity;
    return true;
}

bool frame_resources::ensure_shadow_tile_capacity(u32 viewIndex, u32 tileCount) {
    view_resources &view = mViews[viewIndex];
    if (view.shadowTiles != nullptr && tileCount <= view.shadowTileCapacity) {
        return true;
    }
    const u32 capacity = grow(view.shadowTileCapacity, std::max<u32>(tileCount, 1));
    const std::string name = "shadow_tiles." + std::to_string(viewIndex);
    nvrhi::BufferHandle buffer = create_structured(
      u64{capacity} * gpu::SHADOW_TILE_SIZE,
      gpu::SHADOW_TILE_SIZE,
      false,
      name.c_str()
    );
    if (buffer == nullptr) {
        return false;
    }
    view.shadowTiles = std::move(buffer);
    view.shadowTileCapacity = capacity;
    return true;
}

void frame_resources::release() {
    mViewConstants = nullptr;
    mInstances = nullptr;
    mInstanceCapacity = 0;
    mSpriteVertices = nullptr;
    mSpriteVertexCapacity = 0;
    mSpriteIndices = nullptr;
    mSpriteIndexCapacity = 0;
    mDebugVertices = nullptr;
    mDebugVertexCapacity = 0;
    for (view_resources &view : mViews) {
        view = view_resources();
    }
    mShadowCascades = nullptr;
    mShadowAtlas = nullptr;
    mBrdfLut = nullptr;
}
