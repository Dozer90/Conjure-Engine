#include "rhi/backends/null/null_resources.h"

#include <algorithm>
#include <cstring>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::rhi::null_backend;

/*===========================================================================*/
/*===========================================================================*/
// Memory and textures

null_heap::null_heap(nvrhi::HeapDesc desc) :
    mDesc(std::move(desc)) {}

null_heap::~null_heap() {}

const nvrhi::HeapDesc &null_heap::getDesc() {
    return mDesc;
}

null_texture::null_texture(nvrhi::TextureDesc desc) :
    mDesc(std::move(desc)) {}

null_texture::~null_texture() {}

const nvrhi::TextureDesc &null_texture::getDesc() const {
    return mDesc;
}

nvrhi::Object null_texture::getNativeView(
  nvrhi::ObjectType,
  nvrhi::Format,
  nvrhi::TextureSubresourceSet,
  nvrhi::TextureDimension,
  bool,
  std::optional<nvrhi::ComponentMapping>
) {
    return nvrhi::Object(nullptr);
}

null_staging_texture::null_staging_texture(nvrhi::TextureDesc desc, nvrhi::CpuAccessMode cpuAccess) :
    mDesc(std::move(desc)),
    mCpuAccess(cpuAccess) {}

null_staging_texture::~null_staging_texture() {}

const nvrhi::TextureDesc &null_staging_texture::getDesc() const {
    return mDesc;
}

void *null_staging_texture::map(const nvrhi::TextureSlice &slice, usize *outRowPitch) {
    const nvrhi::TextureSlice resolved = slice.resolve(mDesc);
    const nvrhi::FormatInfo &info = nvrhi::getFormatInfo(mDesc.format);
    const usize block = std::max<usize>(info.blockSize, 1);
    const usize blocksPerRow = (resolved.width + block - 1) / block;
    const usize rows = (resolved.height + block - 1) / block;
    const usize rowPitch = blocksPerRow * info.bytesPerBlock;
    const usize bytes = rowPitch * rows * std::max<usize>(resolved.depth, 1);

    // Readback on the null device returns zero-filled memory (02_renderer.md 6); the storage is
    // re-zeroed on every map so a previous write never leaks into a later readback.
    mStorage.assign(std::max<usize>(bytes, 1), std::byte{0});
    mMapped = true;
    if (outRowPitch != nullptr) {
        *outRowPitch = rowPitch;
    }
    return mStorage.data();
}

void null_staging_texture::unmap() noexcept {
    mMapped = false;
}

null_sampler_feedback_texture::null_sampler_feedback_texture(
  nvrhi::SamplerFeedbackTextureDesc desc,
  nvrhi::ITexture *pairedTexture
) :
    mDesc(desc),
    mPairedTexture(pairedTexture) {}

null_sampler_feedback_texture::~null_sampler_feedback_texture() {}

const nvrhi::SamplerFeedbackTextureDesc &null_sampler_feedback_texture::getDesc() const {
    return mDesc;
}

nvrhi::TextureHandle null_sampler_feedback_texture::getPairedTexture() {
    return mPairedTexture;
}


/*===========================================================================*/
/*===========================================================================*/
// Buffers, shaders, samplers, input layouts

null_buffer::null_buffer(nvrhi::BufferDesc desc, nvrhi::GpuVirtualAddress address) :
    mDesc(std::move(desc)),
    mAddress(address) {}

null_buffer::~null_buffer() {}

const nvrhi::BufferDesc &null_buffer::getDesc() const {
    return mDesc;
}

nvrhi::GpuVirtualAddress null_buffer::getGpuVirtualAddress() const {
    return mAddress;
}

void *null_buffer::map() {
    mMapped = true;
    return storage().empty() ? nullptr : mStorage.data();
}

void null_buffer::unmap() noexcept {
    mMapped = false;
}

void null_buffer::write(u64 offset, std::span<const std::byte> data) {
    const std::span<const std::byte> current = storage();
    if (offset >= current.size()) {
        return;
    }
    const usize available = current.size() - static_cast<usize>(offset);
    const usize count = std::min(available, data.size());
    if (count == 0) {
        return;
    }
    // memmove: copyBuffer passes a span of this same storage as the source.
    std::memmove(mStorage.data() + offset, data.data(), count);
}

std::span<const std::byte> null_buffer::storage() {
    if (mStorage.empty() && mDesc.byteSize > 0) {
        mStorage.assign(static_cast<usize>(mDesc.byteSize), std::byte{0});
    }
    return mStorage;
}

null_shader::null_shader(nvrhi::ShaderDesc desc, std::span<const std::byte> bytecode) :
    mDesc(std::move(desc)),
    mBytecode(bytecode.begin(), bytecode.end()) {}

null_shader::~null_shader() {}

const nvrhi::ShaderDesc &null_shader::getDesc() const {
    return mDesc;
}

void null_shader::getBytecode(const void **ppBytecode, size_t *pSize) const {
    if (ppBytecode != nullptr) {
        *ppBytecode = mBytecode.data();
    }
    if (pSize != nullptr) {
        *pSize = mBytecode.size();
    }
}

null_shader_library::null_shader_library(std::span<const std::byte> bytecode) :
    mBytecode(bytecode.begin(), bytecode.end()) {}

null_shader_library::~null_shader_library() {}

void null_shader_library::getBytecode(const void **ppBytecode, size_t *pSize) const {
    if (ppBytecode != nullptr) {
        *ppBytecode = mBytecode.data();
    }
    if (pSize != nullptr) {
        *pSize = mBytecode.size();
    }
}

nvrhi::ShaderHandle null_shader_library::getShader(const char *entryName, nvrhi::ShaderType shaderType) {
    nvrhi::ShaderDesc desc = {};
    desc.shaderType = shaderType;
    desc.entryName = entryName != nullptr ? entryName : "main";
    desc.debugName = desc.entryName;
    return make_resource<null_shader>(std::move(desc), std::span<const std::byte>(mBytecode));
}

null_sampler::null_sampler(nvrhi::SamplerDesc desc) :
    mDesc(desc) {}

null_sampler::~null_sampler() {}

const nvrhi::SamplerDesc &null_sampler::getDesc() const {
    return mDesc;
}

null_input_layout::null_input_layout(std::span<const nvrhi::VertexAttributeDesc> attributes) :
    mAttributes(attributes.begin(), attributes.end()) {}

null_input_layout::~null_input_layout() {}

uint32_t null_input_layout::getNumAttributes() const {
    return static_cast<uint32_t>(mAttributes.size());
}

const nvrhi::VertexAttributeDesc *null_input_layout::getAttributeDesc(uint32_t index) const {
    if (index >= mAttributes.size()) {
        return nullptr;
    }
    return &mAttributes[index];
}


/*===========================================================================*/
/*===========================================================================*/
// Queries

null_event_query::null_event_query() {}

null_event_query::~null_event_query() {}

null_timer_query::null_timer_query() {}

null_timer_query::~null_timer_query() {}


/*===========================================================================*/
/*===========================================================================*/
// Framebuffers, bindings, pipelines

null_framebuffer::null_framebuffer(const nvrhi::FramebufferDesc &desc) :
    mDesc(desc),
    mInfo(desc) {
    for (const nvrhi::FramebufferAttachment &attachment : desc.colorAttachments) {
        if (attachment.texture != nullptr) {
            mAttachments.emplace_back(attachment.texture);
        }
    }
    if (desc.depthAttachment.texture != nullptr) {
        mAttachments.emplace_back(desc.depthAttachment.texture);
    }
    if (desc.shadingRateAttachment.texture != nullptr) {
        mAttachments.emplace_back(desc.shadingRateAttachment.texture);
    }
}

null_framebuffer::~null_framebuffer() {}

const nvrhi::FramebufferDesc &null_framebuffer::getDesc() const {
    return mDesc;
}

const nvrhi::FramebufferInfoEx &null_framebuffer::getFramebufferInfo() const {
    return mInfo;
}

null_binding_layout::null_binding_layout(nvrhi::BindingLayoutDesc desc) :
    mDesc(std::move(desc)) {}

null_binding_layout::null_binding_layout(nvrhi::BindlessLayoutDesc desc) :
    mBindlessDesc(desc) {}

null_binding_layout::~null_binding_layout() {}

const nvrhi::BindingLayoutDesc *null_binding_layout::getDesc() const {
    return mDesc.has_value() ? &*mDesc : nullptr;
}

const nvrhi::BindlessLayoutDesc *null_binding_layout::getBindlessDesc() const {
    return mBindlessDesc.has_value() ? &*mBindlessDesc : nullptr;
}

null_binding_set::null_binding_set(nvrhi::BindingSetDesc desc, nvrhi::IBindingLayout *layout) :
    mDesc(std::move(desc)),
    mLayout(layout) {
    if (!mDesc.trackLiveness) {
        return;
    }
    for (const nvrhi::BindingSetItem &item : mDesc.bindings) {
        if (item.resourceHandle != nullptr) {
            mResources.emplace_back(item.resourceHandle);
        }
    }
}

null_binding_set::~null_binding_set() {}

const nvrhi::BindingSetDesc *null_binding_set::getDesc() const {
    return &mDesc;
}

nvrhi::IBindingLayout *null_binding_set::getLayout() const {
    return mLayout;
}

null_descriptor_table::null_descriptor_table(nvrhi::IBindingLayout *layout) :
    mLayout(layout) {}

null_descriptor_table::~null_descriptor_table() {}

const nvrhi::BindingSetDesc *null_descriptor_table::getDesc() const {
    return nullptr;
}

nvrhi::IBindingLayout *null_descriptor_table::getLayout() const {
    return mLayout;
}

uint32_t null_descriptor_table::getCapacity() const {
    return static_cast<uint32_t>(mItems.size());
}

uint32_t null_descriptor_table::getFirstDescriptorIndexInHeap() const {
    return 0;
}

void null_descriptor_table::resize(uint32_t newSize, bool keepContents) {
    if (!keepContents) {
        mItems.clear();
    }
    mItems.resize(newSize, nvrhi::BindingSetItem::None());
}

bool null_descriptor_table::write(const nvrhi::BindingSetItem &item) {
    if (item.slot >= mItems.size()) {
        return false;
    }
    mItems[item.slot] = item;
    return true;
}

null_graphics_pipeline::null_graphics_pipeline(
  nvrhi::GraphicsPipelineDesc desc,
  nvrhi::FramebufferInfo framebufferInfo
) :
    mDesc(std::move(desc)),
    mFramebufferInfo(std::move(framebufferInfo)) {}

null_graphics_pipeline::~null_graphics_pipeline() {}

const nvrhi::GraphicsPipelineDesc &null_graphics_pipeline::getDesc() const {
    return mDesc;
}

const nvrhi::FramebufferInfo &null_graphics_pipeline::getFramebufferInfo() const {
    return mFramebufferInfo;
}

null_compute_pipeline::null_compute_pipeline(nvrhi::ComputePipelineDesc desc) :
    mDesc(std::move(desc)) {}

null_compute_pipeline::~null_compute_pipeline() {}

const nvrhi::ComputePipelineDesc &null_compute_pipeline::getDesc() const {
    return mDesc;
}

null_meshlet_pipeline::null_meshlet_pipeline(nvrhi::MeshletPipelineDesc desc, nvrhi::FramebufferInfo framebufferInfo) :
    mDesc(std::move(desc)),
    mFramebufferInfo(std::move(framebufferInfo)) {}

null_meshlet_pipeline::~null_meshlet_pipeline() {}

const nvrhi::MeshletPipelineDesc &null_meshlet_pipeline::getDesc() const {
    return mDesc;
}

const nvrhi::FramebufferInfo &null_meshlet_pipeline::getFramebufferInfo() const {
    return mFramebufferInfo;
}


/*===========================================================================*/
/*===========================================================================*/
// Ray tracing

null_accel_struct::null_accel_struct(nvrhi::rt::AccelStructDesc desc, u64 deviceAddress) :
    mDesc(std::move(desc)),
    mDeviceAddress(deviceAddress) {}

null_accel_struct::~null_accel_struct() {}

const nvrhi::rt::AccelStructDesc &null_accel_struct::getDesc() const {
    return mDesc;
}

bool null_accel_struct::isCompacted() const {
    return false;
}

uint64_t null_accel_struct::getDeviceAddress() const {
    return mDeviceAddress;
}

null_opacity_micromap::null_opacity_micromap(nvrhi::rt::OpacityMicromapDesc desc, u64 deviceAddress) :
    mDesc(std::move(desc)),
    mDeviceAddress(deviceAddress) {}

null_opacity_micromap::~null_opacity_micromap() {}

const nvrhi::rt::OpacityMicromapDesc &null_opacity_micromap::getDesc() const {
    return mDesc;
}

bool null_opacity_micromap::isCompacted() const {
    return false;
}

uint64_t null_opacity_micromap::getDeviceAddress() const {
    return mDeviceAddress;
}

null_shader_table::null_shader_table(nvrhi::rt::ShaderTableDesc desc, null_ray_tracing_pipeline *pipeline) :
    mDesc(std::move(desc)),
    mPipeline(pipeline) {}

null_shader_table::~null_shader_table() {}

const nvrhi::rt::ShaderTableDesc &null_shader_table::getDesc() const {
    return mDesc;
}

uint32_t null_shader_table::getNumEntries() const {
    return (mHasRayGeneration ? 1u : 0u) + mMissShaders + mHitGroups + mCallableShaders;
}

nvrhi::rt::IPipeline *null_shader_table::getPipeline() const {
    return mPipeline;
}

void null_shader_table::setRayGenerationShader(const char *, nvrhi::IBindingSet *) {
    mHasRayGeneration = true;
}

int null_shader_table::addMissShader(const char *, nvrhi::IBindingSet *) {
    return static_cast<int>(mMissShaders++);
}

int null_shader_table::addHitGroup(const char *, nvrhi::IBindingSet *) {
    return static_cast<int>(mHitGroups++);
}

int null_shader_table::addCallableShader(const char *, nvrhi::IBindingSet *) {
    return static_cast<int>(mCallableShaders++);
}

void null_shader_table::clearMissShaders() {
    mMissShaders = 0;
}

void null_shader_table::clearHitShaders() {
    mHitGroups = 0;
}

void null_shader_table::clearCallableShaders() {
    mCallableShaders = 0;
}

null_ray_tracing_pipeline::null_ray_tracing_pipeline(nvrhi::rt::PipelineDesc desc) :
    mDesc(std::move(desc)) {}

null_ray_tracing_pipeline::~null_ray_tracing_pipeline() {}

const nvrhi::rt::PipelineDesc &null_ray_tracing_pipeline::getDesc() const {
    return mDesc;
}

nvrhi::rt::ShaderTableHandle null_ray_tracing_pipeline::createShaderTable(const nvrhi::rt::ShaderTableDesc &desc) {
    return make_resource<null_shader_table>(desc, this);
}


/*===========================================================================*/
/*===========================================================================*/
// Lifetime tracking

null_lifetime_tracker::null_lifetime_tracker(nvrhi::CommandQueue queue) :
    mQueue(queue) {}

null_lifetime_tracker::~null_lifetime_tracker() {}

void null_lifetime_tracker::runGarbageCollection() {
    ++mCollections;
}
