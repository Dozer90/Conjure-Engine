#include "renderer/gpu_resource_cache.h"
#include "conjure/renderer/log_category.h"
#include "conjure/renderer/renderer_error.h"

#include "rhi/texture_format_mapping.h"
#include "conjure/core/error.h"

#include <nvrhi/utils.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr usize MAX_STALE_IDS_REPORTED = 1024;

    [[nodiscard]] nvrhi::BufferDesc static_buffer_desc(u64 byteSize, nvrhi::ResourceStates state, const char *name) {
        nvrhi::BufferDesc desc = {};
        desc.byteSize = byteSize;
        desc.debugName = name;
        desc.isVertexBuffer = state == nvrhi::ResourceStates::VertexBuffer;
        desc.isIndexBuffer = state == nvrhi::ResourceStates::IndexBuffer;
        desc.isConstantBuffer = state == nvrhi::ResourceStates::ConstantBuffer;
        desc.initialState = state;
        desc.keepInitialState = true;
        return desc;
    }

    [[nodiscard]] usize sampler_index(sampler_filter filter, texture_wrap wrap) noexcept {
        return static_cast<usize>(filter) * 3 + static_cast<usize>(wrap);
    }

    /// <summary>
    /// The pointer behind a slot, or nullptr for a stale, unknown or retired id.
    /// </summary>
    template <typename Entry, typename Id>
    [[nodiscard]] Entry *live_entry(core::slot_map<std::unique_ptr<Entry>, Id> &entries, Id id) noexcept {
        std::unique_ptr<Entry> *slot = entries.find(id);
        return slot == nullptr ? nullptr : slot->get();
    }

    template <typename Entry, typename Id>
    [[nodiscard]] const Entry *live_entry(const core::slot_map<std::unique_ptr<Entry>, Id> &entries, Id id) noexcept {
        const std::unique_ptr<Entry> *slot = entries.find(id);
        return slot == nullptr ? nullptr : slot->get();
    }

    /// <summary>
    /// Moves the entry of a live id out of its slot (the slot is erased) so the memory outlives the
    /// erase; nullptr when the id is stale.
    /// </summary>
    template <typename Entry, typename Id>
    [[nodiscard]] std::unique_ptr<Entry> retire_entry(core::slot_map<std::unique_ptr<Entry>, Id> &entries, Id id) {
        std::unique_ptr<Entry> *slot = entries.find(id);
        if (slot == nullptr) {
            return nullptr;
        }
        std::unique_ptr<Entry> retired = std::move(*slot);
        static_cast<void>(entries.erase(id));
        return retired;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Construction

gpu_resource_cache::gpu_resource_cache(create_tag) {}

gpu_resource_cache::~gpu_resource_cache() {}

u64 gpu_resource_cache::upload_record::key() const noexcept {
    return (static_cast<u64>(payload.index()) << 56) | ((index & 0xFFFFFFFFu) << 16) | generation;
}

core::result<std::unique_ptr<gpu_resource_cache>> gpu_resource_cache::create(
  nvrhi::IDevice &device,
  const binding_layouts &layouts,
  nvrhi::ICommandList &commandList,
  u32 releaseDelayFrames
) {
    std::unique_ptr<gpu_resource_cache> cache = std::make_unique<gpu_resource_cache>(create_tag{});
    cache->mDevice = &device;
    cache->mLayouts = &layouts;
    cache->mReleaseDelayFrames = std::max<u32>(releaseDelayFrames, 1);

    for (u32 filter = 0; filter < 2; ++filter) {
        for (u32 wrap = 0; wrap < 3; ++wrap) {
            nvrhi::SamplerDesc desc = {};
            const bool linear = filter == static_cast<u32>(sampler_filter::LINEAR);
            desc.setAllFilters(linear);
            desc.maxAnisotropy = linear ? 8.0f : 1.0f;
            nvrhi::SamplerAddressMode mode = nvrhi::SamplerAddressMode::Repeat;
            if (wrap == static_cast<u32>(texture_wrap::CLAMP)) {
                mode = nvrhi::SamplerAddressMode::Clamp;
            } else if (wrap == static_cast<u32>(texture_wrap::MIRROR)) {
                mode = nvrhi::SamplerAddressMode::Mirror;
            }
            desc.setAllAddressModes(mode);
            nvrhi::SamplerHandle sampler = device.createSampler(desc);
            if (sampler == nullptr) {
                return core::make_error(
                  renderer_error::RESOURCE_CREATION_FAILED,
                  "the device refused a material sampler"
                );
            }
            cache->mSamplers[filter * 3 + wrap] = sampler;
        }
    }
    CONJURE_TRY(cache->create_fallbacks(commandList));
    return cache;
}

core::result<void> gpu_resource_cache::create_fallbacks(nvrhi::ICommandList &commandList) {
    if (!build_mesh(mFallbackMesh, mesh_asset::unit_cube(), commandList, "fallback_cube")) {
        return core::make_error(renderer_error::RESOURCE_CREATION_FAILED, "the device refused the fallback mesh");
    }
    const std::array<texture_asset, 4> fallbacks = {
      texture_asset::solid_colour(core::rgba8{255, 255, 255, 255}, texture_colour_space::SRGB),
      texture_asset::solid_colour(core::rgba8{128, 128, 255, 255}, texture_colour_space::LINEAR),
      texture_asset::solid_colour(core::rgba8{0, 0, 0, 255}, texture_colour_space::LINEAR),
      texture_asset::checker(core::rgba8{255, 0, 255, 255}, core::rgba8{32, 32, 32, 255}, 16, 4),
    };
    const std::array<const char *, 4> names = {
      "fallback_white", "fallback_normal", "fallback_black", "fallback_checker"
    };
    for (usize index = 0; index < fallbacks.size(); ++index) {
        if (!build_texture(mFallbackTextures[index], fallbacks[index], commandList, names[index])) {
            return core::make_error(renderer_error::RESOURCE_CREATION_FAILED, "the device refused a fallback texture");
        }
    }
    if (!build_material(mFallbackMaterial, material_asset::fallback(), commandList, "fallback_material")) {
        return core::make_error(renderer_error::RESOURCE_CREATION_FAILED, "the device refused the fallback material");
    }
    const environment_asset greyEnvironment = environment_asset::uniform(math::vec3(0.25f));
    if (!build_environment(mFallbackEnvironment, greyEnvironment, commandList, "fallback_environment")) {
        return core::make_error(
          renderer_error::RESOURCE_CREATION_FAILED,
          "the device refused the fallback environment"
        );
    }
    return core::result<void>();
}

/*===========================================================================*/
/*===========================================================================*/
// Builders (render thread, caller holds the mutex)

bool gpu_resource_cache::build_mesh(
  gpu_mesh &mirror,
  const mesh_asset &mesh,
  nvrhi::ICommandList &commandList,
  const char *name
) {
    const mesh_streams &streams = mesh.streams();
    if (streams.positions.empty() || streams.indices.empty()) {
        return false;
    }
    const std::string base(name);
    mirror.stream0 = mDevice->createBuffer(
      static_buffer_desc(
        streams.positions.size() * sizeof(math::vec3),
        nvrhi::ResourceStates::VertexBuffer,
        (base + ".stream0").c_str()
      )
    );
    mirror.stream1 = mDevice->createBuffer(
      static_buffer_desc(
        streams.attributes.size() * sizeof(packed_vertex_attributes),
        nvrhi::ResourceStates::VertexBuffer,
        (base + ".stream1").c_str()
      )
    );
    mirror.indices = mDevice->createBuffer(
      static_buffer_desc(
        streams.indices.size() * sizeof(u32),
        nvrhi::ResourceStates::IndexBuffer,
        (base + ".indices").c_str()
      )
    );
    if (mirror.stream0 == nullptr || mirror.stream1 == nullptr || mirror.indices == nullptr) {
        mirror = gpu_mesh();
        return false;
    }
    commandList.writeBuffer(mirror.stream0, streams.positions.data(), streams.positions.size() * sizeof(math::vec3));
    commandList.writeBuffer(
      mirror.stream1,
      streams.attributes.data(),
      streams.attributes.size() * sizeof(packed_vertex_attributes)
    );
    commandList.writeBuffer(mirror.indices, streams.indices.data(), streams.indices.size() * sizeof(u32));
    commandList.setPermanentBufferState(mirror.stream0, nvrhi::ResourceStates::VertexBuffer);
    commandList.setPermanentBufferState(mirror.stream1, nvrhi::ResourceStates::VertexBuffer);
    commandList.setPermanentBufferState(mirror.indices, nvrhi::ResourceStates::IndexBuffer);
    mirror.submeshes.assign(mesh.submeshes().begin(), mesh.submeshes().end());
    mirror.bounds = mesh.bounds();
    mirror.indexCount = mesh.index_count();
    mirror.vertexCount = mesh.vertex_count();
    return true;
}

bool gpu_resource_cache::build_texture(
  gpu_texture &mirror,
  const texture_asset &texture,
  nvrhi::ICommandList &commandList,
  const char *name
) {
    if (texture.mip_count() == 0 || texture.width() == 0) {
        return false;
    }
    nvrhi::TextureDesc desc = {};
    desc.width = texture.width();
    desc.height = texture.height();
    desc.mipLevels = texture.mip_count();
    desc.format = rhi::to_nvrhi_format(texture.format());
    desc.dimension = nvrhi::TextureDimension::Texture2D;
    desc.debugName = name;
    desc.isShaderResource = true;
    desc.initialState = nvrhi::ResourceStates::ShaderResource;
    desc.keepInitialState = true;
    nvrhi::TextureHandle created = mDevice->createTexture(desc);
    if (created == nullptr) {
        return false;
    }
    u32 mip = 0;
    for (const texture_mip &level : texture.mips()) {
        const usize rowPitch = static_cast<usize>(rhi::row_pitch(texture.format(), level.width));
        commandList.writeTexture(created, 0, mip, level.pixels.data(), rowPitch, rowPitch * level.height);
        ++mip;
    }
    commandList.setPermanentTextureState(created, nvrhi::ResourceStates::ShaderResource);
    mirror.texture = created;
    mirror.format = texture.format();
    mirror.width = texture.width();
    mirror.height = texture.height();
    mirror.mipCount = texture.mip_count();
    mirror.filter = texture.filter();
    mirror.wrap = texture.wrap();
    return true;
}

bool gpu_resource_cache::build_material(
  gpu_material &mirror,
  const material_asset &material,
  nvrhi::ICommandList &commandList,
  const char *name
) {
    const material_desc &desc = material.desc();
    mirror.data.baseColourFactor = desc.baseColourFactor;
    mirror.data.emissive = math::vec4(desc.emissiveFactor, desc.emissiveStrength);
    mirror.data.params = math::vec4(
      desc.metallicFactor,
      desc.roughnessFactor,
      desc.normalScale,
      desc.occlusionStrength
    );
    u32 uvSetBits = 0;
    for (usize slot = 0; slot < MATERIAL_TEXTURE_SLOTS; ++slot) {
        if (desc.textureUvSets[slot] != 0) {
            uvSetBits |= 1u << slot;
        }
        mirror.textureAssets[slot] = desc.textures[slot].id();
    }
    const u32 materialFlags = (
      desc.doubleSided ? gpu::MATERIAL_FLAG_DOUBLE_SIDED : 0u
    ) | (desc.unlit ? gpu::MATERIAL_FLAG_UNLIT : 0u);
    mirror.data.flags = math::uvec4(0, materialFlags, uvSetBits, gpu::float_as_bits(desc.alphaCutoff));
    mirror.alphaMode = desc.alphaMode;
    mirror.doubleSided = desc.doubleSided;
    mirror.unlit = desc.unlit;
    mirror.rebindPending = false;
    mirror.constants = mDevice->createBuffer(
      static_buffer_desc(gpu::MATERIAL_CONSTANTS_SIZE, nvrhi::ResourceStates::ConstantBuffer, name)
    );
    if (mirror.constants == nullptr) {
        return false;
    }
    rebind_material(mirror);
    // Not permanent: the texture flags are rewritten when a referenced texture arrives or leaves.
    commandList.writeBuffer(mirror.constants, &mirror.data, sizeof(mirror.data));
    mirror.constantsDirty = false;
    return mirror.bindingSet != nullptr;
}

void gpu_resource_cache::rebind_material(gpu_material &mirror) {
    // Each slot binds its texture when the asset is known and ready, otherwise the matching
    // fallback with the textureFlags bit cleared; the sampler follows the base colour texture.
    u32 textureFlags = 0;
    nvrhi::BindingSetDesc desc = {};
    desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, mirror.constants));
    sampler_filter filter = sampler_filter::LINEAR;
    texture_wrap wrap = texture_wrap::REPEAT;
    for (usize slot = 0; slot < MATERIAL_TEXTURE_SLOTS; ++slot) {
        const gpu_texture *bound = nullptr;
        mirror.textures[slot] = gpu_texture_id();
        if (!mirror.textureAssets[slot].is_null()) {
            const auto found = mTextureByAsset.find(mirror.textureAssets[slot]);
            if (found != mTextureByAsset.end()) {
                const entry<gpu_texture> *record = live_entry(mTextures, found->second);
                if (record != nullptr && record->ready) {
                    bound = &record->mirror;
                    mirror.textures[slot] = found->second;
                    textureFlags |= 1u << slot;
                }
            }
        }
        if (bound == nullptr) {
            bound = &fallback_texture(static_cast<material_texture>(slot));
        }
        if (slot == 0) {
            filter = bound->filter;
            wrap = bound->wrap;
        }
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(static_cast<u32>(slot), bound->texture));
    }
    desc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler(filter, wrap)));
    if (mirror.data.flags.x != textureFlags) {
        mirror.data.flags.x = textureFlags;
        mirror.constantsDirty = true;
    }
    mirror.bindingSet = mDevice->createBindingSet(desc, mLayouts->material);
}

u32 gpu_resource_cache::rebind_materials_using(const assets::asset_id &textureAsset, u64 frameIndex) {
    u32 rebound = 0;
    for (auto &[key, record] : mMaterials) {
        if (record == nullptr || !record->ready) {
            continue;
        }
        const auto &textureAssets = record->mirror.textureAssets;
        const bool uses = std::any_of(textureAssets.begin(), textureAssets.end(), [&](const assets::asset_id &id) {
            return id == textureAsset;
        });
        if (!uses) {
            continue;
        }
        if (record->mirror.bindingSet != nullptr) {
            mGraveyard.emplace_back(record->mirror.bindingSet);
            mGraveyardFrames.push_back(frameIndex);
        }
        rebind_material(record->mirror);
        record->mirror.rebindPending = false;
        ++rebound;
    }
    return rebound;
}

void gpu_resource_cache::mark_materials_using(const assets::asset_id &textureAsset) {
    for (auto &[key, record] : mMaterials) {
        if (record == nullptr || !record->ready) {
            continue;
        }
        const auto &textureAssets = record->mirror.textureAssets;
        const bool uses = std::any_of(textureAssets.begin(), textureAssets.end(), [&](const assets::asset_id &id) {
            return id == textureAsset;
        });
        if (uses) {
            record->mirror.rebindPending = true;
        }
    }
}

bool gpu_resource_cache::upload_cubemap(
  nvrhi::TextureHandle &target,
  const cubemap_data &data,
  nvrhi::ICommandList &commandList,
  const char *name
) {
    if (!data.is_consistent() || data.size == 0) {
        return false;
    }
    nvrhi::TextureDesc desc = {};
    desc.width = data.size;
    desc.height = data.size;
    desc.arraySize = CUBEMAP_FACE_COUNT;
    desc.mipLevels = data.mipCount;
    desc.format = nvrhi::Format::RGBA16_FLOAT;
    desc.dimension = nvrhi::TextureDimension::TextureCube;
    desc.debugName = name;
    desc.isShaderResource = true;
    desc.initialState = nvrhi::ResourceStates::ShaderResource;
    desc.keepInitialState = true;
    target = mDevice->createTexture(desc);
    if (target == nullptr) {
        return false;
    }
    for (u32 mip = 0; mip < data.mipCount; ++mip) {
        const u32 edge = data.mip_size(mip);
        const usize rowPitch = static_cast<usize>(edge) * 4 * sizeof(u16);
        for (u32 face = 0; face < CUBEMAP_FACE_COUNT; ++face) {
            commandList.writeTexture(
              target,
              face,
              mip,
              data.texels.data() + data.face_offset(mip, face),
              rowPitch,
              rowPitch * edge
            );
        }
    }
    commandList.setPermanentTextureState(target, nvrhi::ResourceStates::ShaderResource);
    return true;
}

bool gpu_resource_cache::build_environment(
  gpu_environment &mirror,
  const environment_asset &environment,
  nvrhi::ICommandList &commandList,
  const char *name
) {
    const std::string base(name);
    if (!upload_cubemap(mirror.sky, environment.sky(), commandList, (base + ".sky").c_str())) {
        return false;
    }
    if (!upload_cubemap(mirror.prefiltered, environment.prefiltered(), commandList, (base + ".prefiltered").c_str())) {
        mirror = gpu_environment();
        return false;
    }
    mirror.irradiance = environment.irradiance();
    mirror.prefilteredMipCount = environment.prefiltered().mipCount;
    return true;
}

/*===========================================================================*/
/*===========================================================================*/
// Enqueue / release (any thread)

gpu_mesh_id gpu_resource_cache::enqueue_mesh(const assets::asset_id &asset, const mesh_asset &mesh) {
    const std::scoped_lock lock(mMutex);
    gpu_mesh_id id = {};
    const auto found = asset.is_null() ? mMeshByAsset.end() : mMeshByAsset.find(asset);
    if (found != mMeshByAsset.end() && mMeshes.contains(found->second)) {
        id = found->second;
    } else {
        entry_ptr<gpu_mesh> record = std::make_unique<entry<gpu_mesh>>();
        record->asset = asset;
        id = mMeshes.emplace(std::move(record));
        if (!asset.is_null()) {
            mMeshByAsset[asset] = id;
        }
    }
    upload_record upload = {};
    upload.payload = mesh;
    upload.index = id.index;
    upload.generation = id.generation;
    upload.bytes = mesh.gpu_byte_size();
    mPending.push_back(std::move(upload));
    return id;
}

gpu_texture_id gpu_resource_cache::enqueue_texture(const assets::asset_id &asset, const texture_asset &texture) {
    const std::scoped_lock lock(mMutex);
    gpu_texture_id id = {};
    const auto found = asset.is_null() ? mTextureByAsset.end() : mTextureByAsset.find(asset);
    if (found != mTextureByAsset.end() && mTextures.contains(found->second)) {
        id = found->second;
    } else {
        entry_ptr<gpu_texture> record = std::make_unique<entry<gpu_texture>>();
        record->asset = asset;
        id = mTextures.emplace(std::move(record));
        if (!asset.is_null()) {
            mTextureByAsset[asset] = id;
        }
    }
    upload_record upload = {};
    upload.payload = texture;
    upload.index = id.index;
    upload.generation = id.generation;
    upload.bytes = texture.gpu_byte_size();
    mPending.push_back(std::move(upload));
    return id;
}

gpu_material_id gpu_resource_cache::enqueue_material(const assets::asset_id &asset, const material_asset &material) {
    const std::scoped_lock lock(mMutex);
    gpu_material_id id = {};
    const auto found = asset.is_null() ? mMaterialByAsset.end() : mMaterialByAsset.find(asset);
    if (found != mMaterialByAsset.end() && mMaterials.contains(found->second)) {
        id = found->second;
    } else {
        entry_ptr<gpu_material> record = std::make_unique<entry<gpu_material>>();
        record->asset = asset;
        id = mMaterials.emplace(std::move(record));
        if (!asset.is_null()) {
            mMaterialByAsset[asset] = id;
        }
    }
    upload_record upload = {};
    upload.payload = material;
    upload.index = id.index;
    upload.generation = id.generation;
    upload.bytes = gpu::MATERIAL_CONSTANTS_SIZE;
    mPending.push_back(std::move(upload));
    return id;
}

gpu_environment_id gpu_resource_cache::enqueue_environment(
  const assets::asset_id &asset,
  const environment_asset &environment
) {
    const std::scoped_lock lock(mMutex);
    gpu_environment_id id = {};
    const auto found = asset.is_null() ? mEnvironmentByAsset.end() : mEnvironmentByAsset.find(asset);
    if (found != mEnvironmentByAsset.end() && mEnvironments.contains(found->second)) {
        id = found->second;
    } else {
        entry_ptr<gpu_environment> record = std::make_unique<entry<gpu_environment>>();
        record->asset = asset;
        id = mEnvironments.emplace(std::move(record));
        if (!asset.is_null()) {
            mEnvironmentByAsset[asset] = id;
        }
    }
    upload_record upload = {};
    upload.payload = environment;
    upload.index = id.index;
    upload.generation = id.generation;
    upload.bytes = environment.gpu_byte_size();
    mPending.push_back(std::move(upload));
    return id;
}

void gpu_resource_cache::release_mesh(gpu_mesh_id id, u64 frameIndex) {
    const std::scoped_lock lock(mMutex);
    entry_ptr<gpu_mesh> record = retire_entry(mMeshes, id);
    if (record == nullptr) {
        return;
    }
    if (!record->asset.is_null()) {
        mMeshByAsset.erase(record->asset);
    }
    bury(record->mirror, frameIndex);
    mRetiredMeshes.push_back(std::move(record));
}

void gpu_resource_cache::release_texture(gpu_texture_id id, u64 frameIndex) {
    const std::scoped_lock lock(mMutex);
    entry_ptr<gpu_texture> record = retire_entry(mTextures, id);
    if (record == nullptr) {
        return;
    }
    const assets::asset_id asset = record->asset;
    if (!asset.is_null()) {
        mTextureByAsset.erase(asset);
    }
    bury(record->mirror, frameIndex);
    mRetiredTextures.push_back(std::move(record));
    if (!asset.is_null()) {
        // The binding sets are rebuilt by the next flush on the render thread (R5); until then they
        // keep the buried texture alive through their own references.
        mark_materials_using(asset);
    }
}

void gpu_resource_cache::release_material(gpu_material_id id, u64 frameIndex) {
    const std::scoped_lock lock(mMutex);
    entry_ptr<gpu_material> record = retire_entry(mMaterials, id);
    if (record == nullptr) {
        return;
    }
    if (!record->asset.is_null()) {
        mMaterialByAsset.erase(record->asset);
    }
    bury(record->mirror, frameIndex);
    mRetiredMaterials.push_back(std::move(record));
}

void gpu_resource_cache::release_environment(gpu_environment_id id, u64 frameIndex) {
    const std::scoped_lock lock(mMutex);
    entry_ptr<gpu_environment> record = retire_entry(mEnvironments, id);
    if (record == nullptr) {
        return;
    }
    if (!record->asset.is_null()) {
        mEnvironmentByAsset.erase(record->asset);
    }
    bury(record->mirror, frameIndex);
    mRetiredEnvironments.push_back(std::move(record));
}

u32 gpu_resource_cache::pending_count() const {
    const std::scoped_lock lock(mMutex);
    return static_cast<u32>(mPending.size());
}

// Burying copies the handles (one more reference) so the mirror stays usable by the frame in flight;
// the copy dies after the delay, the mirror's own reference with its entry.

void gpu_resource_cache::bury(const gpu_mesh &mirror, u64 frameIndex) {
    const auto buryBuffer = [this, frameIndex](const nvrhi::BufferHandle &buffer) {
        if (buffer != nullptr) {
            mGraveyard.emplace_back(buffer);
            mGraveyardFrames.push_back(frameIndex);
        }
    };
    buryBuffer(mirror.stream0);
    buryBuffer(mirror.stream1);
    buryBuffer(mirror.indices);
}

void gpu_resource_cache::bury(const gpu_texture &mirror, u64 frameIndex) {
    if (mirror.texture != nullptr) {
        mGraveyard.emplace_back(mirror.texture);
        mGraveyardFrames.push_back(frameIndex);
    }
}

void gpu_resource_cache::bury(const gpu_material &mirror, u64 frameIndex) {
    if (mirror.bindingSet != nullptr) {
        mGraveyard.emplace_back(mirror.bindingSet);
        mGraveyardFrames.push_back(frameIndex);
    }
    if (mirror.constants != nullptr) {
        mGraveyard.emplace_back(mirror.constants);
        mGraveyardFrames.push_back(frameIndex);
    }
}

void gpu_resource_cache::bury(const gpu_environment &mirror, u64 frameIndex) {
    const auto buryTexture = [this, frameIndex](const nvrhi::TextureHandle &texture) {
        if (texture != nullptr) {
            mGraveyard.emplace_back(texture);
            mGraveyardFrames.push_back(frameIndex);
        }
    };
    buryTexture(mirror.sky);
    buryTexture(mirror.prefiltered);
}

/*===========================================================================*/
/*===========================================================================*/
// Lookups

gpu_mesh_id gpu_resource_cache::find_mesh(const assets::asset_id &asset) const noexcept {
    const std::scoped_lock lock(mMutex);
    const auto found = mMeshByAsset.find(asset);
    return found == mMeshByAsset.end() ? gpu_mesh_id() : found->second;
}

gpu_texture_id gpu_resource_cache::find_texture(const assets::asset_id &asset) const noexcept {
    const std::scoped_lock lock(mMutex);
    const auto found = mTextureByAsset.find(asset);
    return found == mTextureByAsset.end() ? gpu_texture_id() : found->second;
}

gpu_material_id gpu_resource_cache::find_material(const assets::asset_id &asset) const noexcept {
    const std::scoped_lock lock(mMutex);
    const auto found = mMaterialByAsset.find(asset);
    return found == mMaterialByAsset.end() ? gpu_material_id() : found->second;
}

gpu_environment_id gpu_resource_cache::find_environment(const assets::asset_id &asset) const noexcept {
    const std::scoped_lock lock(mMutex);
    const auto found = mEnvironmentByAsset.find(asset);
    return found == mEnvironmentByAsset.end() ? gpu_environment_id() : found->second;
}

bool gpu_resource_cache::is_mesh_ready(gpu_mesh_id id) const noexcept {
    const std::scoped_lock lock(mMutex);
    const entry<gpu_mesh> *record = live_entry(mMeshes, id);
    return record != nullptr && record->ready;
}

bool gpu_resource_cache::is_texture_ready(gpu_texture_id id) const noexcept {
    const std::scoped_lock lock(mMutex);
    const entry<gpu_texture> *record = live_entry(mTextures, id);
    return record != nullptr && record->ready;
}

bool gpu_resource_cache::is_material_ready(gpu_material_id id) const noexcept {
    const std::scoped_lock lock(mMutex);
    const entry<gpu_material> *record = live_entry(mMaterials, id);
    return record != nullptr && record->ready;
}

bool gpu_resource_cache::is_environment_ready(gpu_environment_id id) const noexcept {
    const std::scoped_lock lock(mMutex);
    const entry<gpu_environment> *record = live_entry(mEnvironments, id);
    return record != nullptr && record->ready;
}

u32 gpu_resource_cache::fallbacks_used() const noexcept {
    const std::scoped_lock lock(mMutex);
    return mFallbacksUsed;
}

void gpu_resource_cache::reset_counters() noexcept {
    const std::scoped_lock lock(mMutex);
    mFallbacksUsed = 0;
}

usize gpu_resource_cache::mesh_count() const noexcept {
    const std::scoped_lock lock(mMutex);
    return mMeshes.size();
}

usize gpu_resource_cache::texture_count() const noexcept {
    const std::scoped_lock lock(mMutex);
    return mTextures.size();
}

usize gpu_resource_cache::material_count() const noexcept {
    const std::scoped_lock lock(mMutex);
    return mMaterials.size();
}

usize gpu_resource_cache::environment_count() const noexcept {
    const std::scoped_lock lock(mMutex);
    return mEnvironments.size();
}

usize gpu_resource_cache::graveyard_count() const noexcept {
    const std::scoped_lock lock(mMutex);
    return mGraveyard.size();
}

usize gpu_resource_cache::retired_count() const noexcept {
    const std::scoped_lock lock(mMutex);
    return mRetiredMeshes.size() + mRetiredTextures.size() + mRetiredMaterials.size() + mRetiredEnvironments.size();
}

/*===========================================================================*/
/*===========================================================================*/
// Flush and collect (render thread)

upload_report gpu_resource_cache::flush_uploads(nvrhi::ICommandList &commandList, u64 frameIndex, u64 budgetBytes) {
    const std::scoped_lock lock(mMutex);
    upload_report report = {};
    mDeferred.clear();
    mDeferredKeys.clear();
    for (usize index = 0; index < mPending.size(); ++index) {
        upload_record &upload = mPending[index];
        // Records of one id flush in queue order: once one of them waits, the later ones wait too,
        // or an older payload would replace a newer one on the next flush.
        const u64 key = upload.key();
        const bool overBudget = report.completed > 0 && report.bytes + upload.bytes > budgetBytes;
        if (overBudget || mDeferredKeys.contains(key)) {
            mDeferredKeys.insert(key);
            mDeferred.push_back(std::move(upload));
            continue;
        }
        const u64 bytes = upload.bytes;
        bool done = false;
        if (const mesh_asset *mesh = std::get_if<mesh_asset>(&upload.payload); mesh != nullptr) {
            gpu_mesh_id id = {};
            id.index = static_cast<u32>(upload.index);
            id.generation = upload.generation;
            entry<gpu_mesh> *record = live_entry(mMeshes, id);
            if (record != nullptr) {
                bury(record->mirror, frameIndex);
                record->mirror = gpu_mesh();
                const std::string name = mesh->name().empty() ? "mesh" : std::string(mesh->name());
                record->ready = build_mesh(record->mirror, *mesh, commandList, name.c_str());
                done = true;
            }
        } else if (const texture_asset *texture = std::get_if<texture_asset>(&upload.payload); texture != nullptr) {
            gpu_texture_id id = {};
            id.index = static_cast<u32>(upload.index);
            id.generation = upload.generation;
            entry<gpu_texture> *record = live_entry(mTextures, id);
            if (record != nullptr) {
                bury(record->mirror, frameIndex);
                record->mirror = gpu_texture();
                const std::string name = texture->name().empty() ? "texture" : std::string(texture->name());
                record->ready = build_texture(record->mirror, *texture, commandList, name.c_str());
                done = true;
                if (record->ready && !record->asset.is_null()) {
                    report.materialsRebound += rebind_materials_using(record->asset, frameIndex);
                }
            }
        } else if (const material_asset *material = std::get_if<material_asset>(&upload.payload); material != nullptr) {
            gpu_material_id id = {};
            id.index = static_cast<u32>(upload.index);
            id.generation = upload.generation;
            entry<gpu_material> *record = live_entry(mMaterials, id);
            if (record != nullptr) {
                bury(record->mirror, frameIndex);
                record->mirror = gpu_material();
                const std::string name = material->name().empty() ? "material" : std::string(material->name());
                record->ready = build_material(record->mirror, *material, commandList, name.c_str());
                done = true;
            }
        } else if (const environment_asset *environment = std::get_if<environment_asset>(
          &upload.payload
        ); environment != nullptr) {
            gpu_environment_id id = {};
            id.index = static_cast<u32>(upload.index);
            id.generation = upload.generation;
            entry<gpu_environment> *record = live_entry(mEnvironments, id);
            if (record != nullptr) {
                bury(record->mirror, frameIndex);
                record->mirror = gpu_environment();
                const std::string name = environment->name().empty() ? "environment" : std::string(environment->name());
                record->ready = build_environment(record->mirror, *environment, commandList, name.c_str());
                done = true;
            }
        }
        if (done) {
            ++report.completed;
            report.bytes += bytes;
        }
    }

    // Materials whose textures were released since the previous flush bind the fallbacks now, on the
    // render thread; then only the constant buffers whose texture flags changed are rewritten.
    for (auto &[key, record] : mMaterials) {
        if (record == nullptr || !record->ready) {
            continue;
        }
        gpu_material &mirror = record->mirror;
        if (mirror.rebindPending) {
            if (mirror.bindingSet != nullptr) {
                mGraveyard.emplace_back(mirror.bindingSet);
                mGraveyardFrames.push_back(frameIndex);
            }
            rebind_material(mirror);
            mirror.rebindPending = false;
            ++report.materialsRebound;
        }
        if (mirror.constantsDirty && mirror.constants != nullptr) {
            commandList.writeBuffer(mirror.constants, &mirror.data, sizeof(mirror.data));
            mirror.constantsDirty = false;
            ++report.constantWrites;
        }
    }
    mPending.swap(mDeferred);
    mDeferred.clear();
    report.pending = static_cast<u32>(mPending.size());
    return report;
}

void gpu_resource_cache::collect(u64 frameIndex) {
    const std::scoped_lock lock(mMutex);
    mRetiredMeshes.clear();
    mRetiredTextures.clear();
    mRetiredMaterials.clear();
    mRetiredEnvironments.clear();
    usize kept = 0;
    for (usize index = 0; index < mGraveyard.size(); ++index) {
        const bool expired = frameIndex >= mGraveyardFrames[index]
          && frameIndex - mGraveyardFrames[index] > mReleaseDelayFrames;
        if (expired) {
            continue;
        }
        if (kept != index) {
            mGraveyard[kept] = std::move(mGraveyard[index]);
            mGraveyardFrames[kept] = mGraveyardFrames[index];
        }
        ++kept;
    }
    mGraveyard.resize(kept);
    mGraveyardFrames.resize(kept);
}

void gpu_resource_cache::drop_all() {
    const std::scoped_lock lock(mMutex);
    mMeshes.clear();
    mTextures.clear();
    mMaterials.clear();
    mEnvironments.clear();
    mMeshByAsset.clear();
    mTextureByAsset.clear();
    mMaterialByAsset.clear();
    mEnvironmentByAsset.clear();
    mPending.clear();
    mDeferred.clear();
    mDeferredKeys.clear();
    mRetiredMeshes.clear();
    mRetiredTextures.clear();
    mRetiredMaterials.clear();
    mRetiredEnvironments.clear();
    mGraveyard.clear();
    mGraveyardFrames.clear();
}

/*===========================================================================*/
/*===========================================================================*/
// Resolution (any thread; locks internally)

void gpu_resource_cache::warn_stale(u8 kindIndex, const char *kind, u64 packedId, u32 index) {
    // Once per (kind, id); past the bound nothing more is logged (a scene full of stale ids would
    // otherwise log every frame).
    const u64 key = (static_cast<u64>(kindIndex + 1u) << 56) | packedId;
    if (mStaleReported.contains(key) || mStaleReported.size() >= MAX_STALE_IDS_REPORTED) {
        return;
    }
    mStaleReported.insert(key);
    CONJURE_LOG_WARN(LOG_RENDERER, "{} id {} is stale or unknown; drawing the fallback", kind, index);
}

const gpu_mesh &gpu_resource_cache::resolve_mesh(gpu_mesh_id id) {
    const std::scoped_lock lock(mMutex);
    entry<gpu_mesh> *record = live_entry(mMeshes, id);
    if (record != nullptr && record->ready) {
        return record->mirror;
    }
    ++mFallbacksUsed;
    if (record == nullptr) {
        warn_stale(0, "mesh", id.to_u64(), id.index);
    } else if (!record->pendingReported) {
        record->pendingReported = true;
        CONJURE_LOG_DEBUG(LOG_RENDERER, "mesh id {} has not been uploaded yet; drawing the fallback", id.index);
    }
    return mFallbackMesh;
}

const gpu_texture &gpu_resource_cache::resolve_texture(gpu_texture_id id) {
    const std::scoped_lock lock(mMutex);
    entry<gpu_texture> *record = live_entry(mTextures, id);
    if (record != nullptr && record->ready) {
        return record->mirror;
    }
    ++mFallbacksUsed;
    if (record == nullptr) {
        warn_stale(1, "texture", id.to_u64(), id.index);
    } else if (!record->pendingReported) {
        record->pendingReported = true;
        CONJURE_LOG_DEBUG(LOG_RENDERER, "texture id {} has not been uploaded yet; drawing the fallback", id.index);
    }
    return mFallbackTextures[0];
}

const gpu_material &gpu_resource_cache::resolve_material(gpu_material_id id) {
    const std::scoped_lock lock(mMutex);
    entry<gpu_material> *record = live_entry(mMaterials, id);
    if (record != nullptr && record->ready) {
        return record->mirror;
    }
    ++mFallbacksUsed;
    if (record == nullptr) {
        warn_stale(2, "material", id.to_u64(), id.index);
    } else if (!record->pendingReported) {
        record->pendingReported = true;
        CONJURE_LOG_DEBUG(LOG_RENDERER, "material id {} has not been uploaded yet; drawing the fallback", id.index);
    }
    return mFallbackMaterial;
}

const gpu_environment &gpu_resource_cache::resolve_environment(gpu_environment_id id) {
    const std::scoped_lock lock(mMutex);
    entry<gpu_environment> *record = live_entry(mEnvironments, id);
    if (record != nullptr && record->ready) {
        return record->mirror;
    }
    ++mFallbacksUsed;
    if (record == nullptr) {
        warn_stale(3, "environment", id.to_u64(), id.index);
    } else if (!record->pendingReported) {
        record->pendingReported = true;
        CONJURE_LOG_DEBUG(
          LOG_RENDERER,
          "environment id {} has not been uploaded yet; drawing the fallback",
          id.index
        );
    }
    return mFallbackEnvironment;
}

gpu_texture_id gpu_resource_cache::register_external_texture(
  nvrhi::ITexture *texture,
  rhi::texture_format format,
  u32 width,
  u32 height
) {
    const std::scoped_lock lock(mMutex);
    entry_ptr<gpu_texture> record = std::make_unique<entry<gpu_texture>>();
    record->mirror.texture = texture;
    record->mirror.format = format;
    record->mirror.width = width;
    record->mirror.height = height;
    record->mirror.mipCount = 1;
    record->mirror.filter = sampler_filter::LINEAR;
    record->mirror.wrap = texture_wrap::CLAMP;
    record->ready = true;
    return mTextures.emplace(std::move(record));
}

void gpu_resource_cache::unregister_external_texture(gpu_texture_id id) {
    const std::scoped_lock lock(mMutex);
    entry_ptr<gpu_texture> record = retire_entry(mTextures, id);
    if (record == nullptr) {
        return;
    }
    // The caller owns the texture: nothing is buried, the entry's reference goes at the next collect.
    mRetiredTextures.push_back(std::move(record));
}

const gpu_texture &gpu_resource_cache::fallback_texture(material_texture slot) const noexcept {
    switch (slot) {
        case material_texture::NORMAL:
            return mFallbackTextures[1];
        case material_texture::EMISSIVE:
            return mFallbackTextures[2];
        case material_texture::BASE_COLOUR:
        case material_texture::METALLIC_ROUGHNESS:
        case material_texture::OCCLUSION:
            return mFallbackTextures[0];
    }
    return mFallbackTextures[3];
}

nvrhi::ISampler *gpu_resource_cache::sampler(sampler_filter filter, texture_wrap wrap) const noexcept {
    return mSamplers[sampler_index(filter, wrap)].Get();
}
