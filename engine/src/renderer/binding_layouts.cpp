#include "renderer/binding_layouts.h"

#include "renderer/gpu_types.h"

#include "conjure/renderer/limits.h"
#include "conjure/renderer/renderer_error.h"

#include "conjure/core/error.h"
#include "conjure/core/hash.h"

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr u32 BINDING_SET_EVICTION_FRAMES = 120;

    [[nodiscard]] nvrhi::BindingLayoutDesc base_layout(u32 space) {
        nvrhi::BindingLayoutDesc desc = {};
        desc.visibility = nvrhi::ShaderType::All;
        desc.registerSpace = space;
        desc.registerSpaceIsDescriptorSet = true;
        return desc;
    }

    [[nodiscard]] core::result<nvrhi::BindingLayoutHandle> make_layout(
      nvrhi::IDevice &device,
      const nvrhi::BindingLayoutDesc &desc,
      const char *name
    ) {
        nvrhi::BindingLayoutHandle layout = device.createBindingLayout(desc);
        if (layout == nullptr) {
            return core::make_error(
              renderer_error::PIPELINE_CREATION_FAILED,
              "binding layout '{}' was refused by the device",
              name
            );
        }
        return layout;
    }

    void hash_u64(u64 &hash, u64 value) noexcept {
        for (u32 byteIndex = 0; byteIndex < 8; ++byteIndex) {
            hash = core::hash::fnv1a64_step(hash, static_cast<u8>(value >> (byteIndex * 8)));
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// binding_layouts

core::result<binding_layouts> binding_layouts::create(nvrhi::IDevice &device) {
    binding_layouts layouts = {};

    nvrhi::BindingLayoutDesc meshView = base_layout(0);
    meshView.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
    meshView.addItem(nvrhi::BindingLayoutItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    meshView.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0));
    meshView.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1));
    meshView.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(2));
    meshView.addItem(nvrhi::BindingLayoutItem::Texture_SRV(3));
    meshView.addItem(nvrhi::BindingLayoutItem::Texture_SRV(4));
    meshView.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(5));
    meshView.addItem(nvrhi::BindingLayoutItem::Texture_SRV(6));
    meshView.addItem(nvrhi::BindingLayoutItem::Texture_SRV(7));
    meshView.addItem(nvrhi::BindingLayoutItem::Texture_SRV(8));
    meshView.addItem(nvrhi::BindingLayoutItem::Texture_SRV(9));
    meshView.addItem(nvrhi::BindingLayoutItem::Sampler(1));
    meshView.addItem(nvrhi::BindingLayoutItem::Sampler(2));
    meshView.addItem(nvrhi::BindingLayoutItem::Sampler(3));
    CONJURE_TRY_ASSIGN(layouts.meshView, make_layout(device, meshView, "mesh_view"));

    nvrhi::BindingLayoutDesc material = base_layout(1);
    material.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
    for (u32 slot = 0; slot < MATERIAL_TEXTURE_SLOTS; ++slot) {
        material.addItem(nvrhi::BindingLayoutItem::Texture_SRV(slot));
    }
    material.addItem(nvrhi::BindingLayoutItem::Sampler(0));
    CONJURE_TRY_ASSIGN(layouts.material, make_layout(device, material, "material"));

    nvrhi::BindingLayoutDesc fullscreen = base_layout(0);
    fullscreen.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
    fullscreen.addItem(nvrhi::BindingLayoutItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    fullscreen.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    fullscreen.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
    fullscreen.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(2));
    fullscreen.addItem(nvrhi::BindingLayoutItem::Sampler(0));
    fullscreen.addItem(nvrhi::BindingLayoutItem::Sampler(1));
    CONJURE_TRY_ASSIGN(layouts.fullscreen, make_layout(device, fullscreen, "fullscreen"));

    nvrhi::BindingLayoutDesc lightCull = base_layout(0);
    lightCull.visibility = nvrhi::ShaderType::Compute;
    lightCull.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
    lightCull.addItem(nvrhi::BindingLayoutItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    lightCull.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    lightCull.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1));
    lightCull.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_UAV(0));
    CONJURE_TRY_ASSIGN(layouts.lightCull, make_layout(device, lightCull, "light_cull"));

    nvrhi::BindingLayoutDesc ssao = base_layout(0);
    ssao.visibility = nvrhi::ShaderType::Compute;
    ssao.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
    ssao.addItem(nvrhi::BindingLayoutItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    ssao.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    ssao.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
    ssao.addItem(nvrhi::BindingLayoutItem::Texture_UAV(0));
    ssao.addItem(nvrhi::BindingLayoutItem::Sampler(0));
    CONJURE_TRY_ASSIGN(layouts.ssao, make_layout(device, ssao, "ssao"));

    // The GTAO pass also writes the half-resolution view depth the forward shader's bilateral
    // upsample weighs its taps with.
    nvrhi::BindingLayoutDesc ssaoGtao = ssao;
    ssaoGtao.addItem(nvrhi::BindingLayoutItem::Texture_UAV(1));
    CONJURE_TRY_ASSIGN(layouts.ssaoGtao, make_layout(device, ssaoGtao, "ssao_gtao"));

    nvrhi::BindingLayoutDesc histogram = base_layout(0);
    histogram.visibility = nvrhi::ShaderType::Compute;
    histogram.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
    histogram.addItem(nvrhi::BindingLayoutItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    histogram.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    histogram.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_UAV(0));
    CONJURE_TRY_ASSIGN(layouts.exposureHistogram, make_layout(device, histogram, "exposure_histogram"));

    nvrhi::BindingLayoutDesc adapt = base_layout(0);
    adapt.visibility = nvrhi::ShaderType::Compute;
    adapt.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
    adapt.addItem(nvrhi::BindingLayoutItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    adapt.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0));
    adapt.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_UAV(0));
    CONJURE_TRY_ASSIGN(layouts.exposureAdapt, make_layout(device, adapt, "exposure_adapt"));

    nvrhi::BindingLayoutDesc bloom = base_layout(0);
    bloom.visibility = nvrhi::ShaderType::Compute;
    bloom.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
    bloom.addItem(nvrhi::BindingLayoutItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    bloom.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    bloom.addItem(nvrhi::BindingLayoutItem::Texture_UAV(0));
    bloom.addItem(nvrhi::BindingLayoutItem::Sampler(0));
    CONJURE_TRY_ASSIGN(layouts.bloom, make_layout(device, bloom, "bloom"));

    nvrhi::BindingLayoutDesc imgui = base_layout(0);
    imgui.addItem(nvrhi::BindingLayoutItem::PushConstants(1, 16));
    imgui.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    imgui.addItem(nvrhi::BindingLayoutItem::Sampler(0));
    CONJURE_TRY_ASSIGN(layouts.imgui, make_layout(device, imgui, "imgui"));

    return layouts;
}

/*===========================================================================*/
/*===========================================================================*/
// binding_set_cache

binding_set_cache::binding_set_cache(nvrhi::IDevice &device) :
    mDevice(device) {}

binding_set_cache::~binding_set_cache() {}

u64 binding_set_cache::hash(nvrhi::IBindingLayout *layout, const nvrhi::BindingSetDesc &desc) noexcept {
    u64 value = core::hash::FNV1A64_OFFSET_BASIS;
    hash_u64(value, reinterpret_cast<u64>(layout));
    hash_u64(value, desc.bindings.size());
    for (const nvrhi::BindingSetItem &item : desc.bindings) {
        hash_u64(value, item.slot);
        hash_u64(value, static_cast<u64>(item.type));
        hash_u64(value, reinterpret_cast<u64>(item.resourceHandle));
        hash_u64(value, static_cast<u64>(item.format));
        hash_u64(value, static_cast<u64>(item.dimension));
        hash_u64(value, item.rawData[0]);
        hash_u64(value, item.rawData[1]);
    }
    return value;
}

nvrhi::IBindingSet *binding_set_cache::get(nvrhi::IBindingLayout *layout, const nvrhi::BindingSetDesc &desc) {
    const u64 key = hash(layout, desc);
    const auto range = mEntries.equal_range(key);
    for (auto iterator = range.first; iterator != range.second; ++iterator) {
        if (iterator->second.layout == layout && iterator->second.desc == desc) {
            iterator->second.lastUsedFrame = mFrameIndex;
            ++mHits;
            return iterator->second.set.Get();
        }
    }
    ++mMisses;
    nvrhi::BindingSetHandle set = mDevice.createBindingSet(desc, layout);
    if (set == nullptr) {
        return nullptr;
    }
    entry record = {};
    record.set = set;
    record.desc = desc;
    record.layout = layout;
    record.lastUsedFrame = mFrameIndex;
    const auto inserted = mEntries.emplace(key, std::move(record));
    return inserted->second.set.Get();
}

void binding_set_cache::end_frame(u64 frameIndex) {
    mFrameIndex = frameIndex;
    for (auto iterator = mEntries.begin(); iterator != mEntries.end();) {
        const bool stale = frameIndex >= iterator->second.lastUsedFrame &&
          frameIndex - iterator->second.lastUsedFrame > BINDING_SET_EVICTION_FRAMES;
        if (stale) {
            iterator = mEntries.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void binding_set_cache::clear() {
    mEntries.clear();
}
