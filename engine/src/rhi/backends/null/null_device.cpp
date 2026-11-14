#include "rhi/backends/null/null_device.h"

#include "rhi/backends/null/null_command_list.h"
#include "rhi/backends/null/null_resources.h"
#include "rhi/backends/null/null_swapchain.h"
#include "rhi/message_callback.h"

#include "conjure/rhi/log_category.h"

#include "conjure/core/log.h"

#include <algorithm>
#include <cstddef>
#include <span>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::rhi::null_backend;

namespace {
    constexpr u64 VIRTUAL_ADDRESS_ALIGNMENT = 64 * KIB;

    /// <summary>
    /// Bytes of a mip chain of one array slice, blocks rounded up, for the memory requirement queries.
    /// </summary>
    u64 texture_bytes(const nvrhi::TextureDesc &desc) {
        const nvrhi::FormatInfo &info = nvrhi::getFormatInfo(desc.format);
        const u64 block = std::max<u64>(info.blockSize, 1);
        u64 total = 0;
        for (u32 mip = 0; mip < std::max(desc.mipLevels, 1u); ++mip) {
            const u64 width = std::max<u64>(desc.width >> mip, 1);
            const u64 height = std::max<u64>(desc.height >> mip, 1);
            const u64 depth = std::max<u64>(desc.depth >> mip, 1);
            const u64 blocks = ((width + block - 1) / block) * ((height + block - 1) / block) * depth;
            total += blocks * info.bytesPerBlock;
        }
        return total * std::max(desc.arraySize, 1u) * std::max(desc.sampleCount, 1u);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

null_device::null_device(message_callback &messages) :
    mMessages(messages) {}

null_device::~null_device() {}


/*===========================================================================*/
/*===========================================================================*/
// Statistics

void null_device::record_marker(const char *name) {
    const std::scoped_lock lock(mStatsMutex);
    ++mStats.markersBegun;
    if (!mRecordMarkerNames || mStats.markerNames.size() >= MAX_RECORDED_MARKER_NAMES) {
        ++mStats.markerNamesDropped;
        return;
    }
    mStats.markerNames.emplace_back(name != nullptr ? name : "");
}

null_device_stats null_device::stats() const {
    const std::scoped_lock lock(mStatsMutex);
    return mStats;
}

void null_device::reset_stats() {
    const std::scoped_lock lock(mStatsMutex);
    mStats.reset();
    mRecordMarkerNames = true;
}

void null_device::inject_fault(null_fault fault) noexcept {
    mPendingFault.store(fault, std::memory_order_release);
}

bool null_device::consume_fault(null_fault fault) noexcept {
    null_fault expected = fault;
    return fault != null_fault::NONE &&
      mPendingFault.compare_exchange_strong(expected, null_fault::NONE, std::memory_order_acq_rel);
}


/*===========================================================================*/
/*===========================================================================*/
// Memory and textures

nvrhi::HeapHandle null_device::createHeap(const nvrhi::HeapDesc &d) {
    record([](null_device_stats &stats) {
        ++stats.heapsCreated;
    });
    return make_resource<null_heap>(d);
}

bool null_device::queryTopLevelAccelStructPrebuildInfo(
  const nvrhi::rt::AccelStructDesc &,
  uint32_t,
  nvrhi::rt::AccelStructPrebuildInfo &
) {
    return false;
}

nvrhi::TextureHandle null_device::createTexture(const nvrhi::TextureDesc &d) {
    if (d.width == 0 || d.height == 0 || d.depth == 0 || d.format == nvrhi::Format::UNKNOWN) {
        report_error("createTexture: a texture needs a non-zero size and a known format");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.texturesCreated;
    });
    return make_resource<null_texture>(d);
}

nvrhi::MemoryRequirements null_device::getTextureMemoryRequirements(nvrhi::ITexture *texture) {
    nvrhi::MemoryRequirements requirements = {};
    if (texture == nullptr) {
        return requirements;
    }
    requirements.size = texture_bytes(texture->getDesc());
    requirements.alignment = VIRTUAL_ADDRESS_ALIGNMENT;
    return requirements;
}

bool null_device::bindTextureMemory(nvrhi::ITexture *texture, nvrhi::IHeap *heap, uint64_t) {
    return texture != nullptr && heap != nullptr;
}

nvrhi::TextureHandle null_device::createHandleForNativeTexture(
  nvrhi::ObjectType,
  nvrhi::Object,
  const nvrhi::TextureDesc &desc
) {
    // There is no native object on the null device; the handle is a plain null texture with the
    // caller's description, which is what the null swapchain wraps its buffers in.
    record([](null_device_stats &stats) {
        ++stats.texturesCreated;
    });
    return make_resource<null_texture>(desc);
}

nvrhi::StagingTextureHandle null_device::createStagingTexture(
  const nvrhi::TextureDesc &d,
  nvrhi::CpuAccessMode cpuAccess
) {
    if (d.width == 0 || d.height == 0 || d.depth == 0 || d.format == nvrhi::Format::UNKNOWN) {
        report_error("createStagingTexture: a staging texture needs a non-zero size and a known format");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.stagingTexturesCreated;
    });
    return make_resource<null_staging_texture>(d, cpuAccess);
}

void *null_device::mapStagingTexture(
  nvrhi::IStagingTexture *tex,
  const nvrhi::TextureSlice &slice,
  nvrhi::CpuAccessMode,
  size_t *outRowPitch
) {
    null_staging_texture *staging = dynamic_cast<null_staging_texture *>(tex);
    if (staging == nullptr) {
        return nullptr;
    }
    // Only the creation mode matters, as on D3D12: mapping with another mode or twice is legal.
    if (staging->cpu_access() == nvrhi::CpuAccessMode::None) {
        report_error("mapStagingTexture: the staging texture was created without CPU access");
        return nullptr;
    }
    return staging->map(slice, outRowPitch);
}

void null_device::unmapStagingTexture(nvrhi::IStagingTexture *tex) {
    null_staging_texture *staging = dynamic_cast<null_staging_texture *>(tex);
    if (staging != nullptr) {
        staging->unmap();
    }
}

void null_device::getTextureTiling(
  nvrhi::ITexture *,
  uint32_t *numTiles,
  nvrhi::PackedMipDesc *desc,
  nvrhi::TileShape *tileShape,
  uint32_t *subresourceTilingsNum,
  nvrhi::SubresourceTiling *
) {
    if (numTiles != nullptr) {
        *numTiles = 0;
    }
    if (desc != nullptr) {
        *desc = nvrhi::PackedMipDesc();
    }
    if (tileShape != nullptr) {
        *tileShape = nvrhi::TileShape();
    }
    if (subresourceTilingsNum != nullptr) {
        *subresourceTilingsNum = 0;
    }
}

void null_device::updateTextureTileMappings(
  nvrhi::ITexture *,
  const nvrhi::TextureTilesMapping *,
  uint32_t,
  nvrhi::CommandQueue
) {}

nvrhi::SamplerFeedbackTextureHandle null_device::createSamplerFeedbackTexture(
  nvrhi::ITexture *pairedTexture,
  const nvrhi::SamplerFeedbackTextureDesc &desc
) {
    if (pairedTexture == nullptr) {
        report_error("createSamplerFeedbackTexture: paired texture is null");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.texturesCreated;
    });
    return make_resource<null_sampler_feedback_texture>(desc, pairedTexture);
}

nvrhi::SamplerFeedbackTextureHandle null_device::createSamplerFeedbackForNativeTexture(
  nvrhi::ObjectType,
  nvrhi::Object,
  nvrhi::ITexture *pairedTexture
) {
    return createSamplerFeedbackTexture(pairedTexture, nvrhi::SamplerFeedbackTextureDesc());
}


/*===========================================================================*/
/*===========================================================================*/
// Buffers, shaders, samplers, input layouts

nvrhi::BufferHandle null_device::createBuffer(const nvrhi::BufferDesc &d) {
    if (d.byteSize == 0) {
        report_error("createBuffer: byteSize is zero");
        return nullptr;
    }
    if (d.isVolatile && d.maxVersions == 0) {
        report_error("createBuffer: a volatile buffer needs maxVersions > 0");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.buffersCreated;
    });
    return make_resource<null_buffer>(d, next_virtual_address(d.byteSize));
}

void *null_device::mapBuffer(nvrhi::IBuffer *buffer, nvrhi::CpuAccessMode) {
    null_buffer *target = dynamic_cast<null_buffer *>(buffer);
    if (target == nullptr) {
        return nullptr;
    }
    // A GPU-only buffer lives in a default heap on D3D12 and cannot be mapped there either.
    if (target->getDesc().cpuAccess == nvrhi::CpuAccessMode::None) {
        report_error("mapBuffer: the buffer was created without CPU access");
        return nullptr;
    }
    return target->map();
}

void null_device::unmapBuffer(nvrhi::IBuffer *buffer) {
    null_buffer *target = dynamic_cast<null_buffer *>(buffer);
    if (target != nullptr) {
        target->unmap();
    }
}

nvrhi::MemoryRequirements null_device::getBufferMemoryRequirements(nvrhi::IBuffer *buffer) {
    nvrhi::MemoryRequirements requirements = {};
    if (buffer == nullptr) {
        return requirements;
    }
    requirements.size = buffer->getDesc().byteSize;
    requirements.alignment = VIRTUAL_ADDRESS_ALIGNMENT;
    return requirements;
}

bool null_device::bindBufferMemory(nvrhi::IBuffer *buffer, nvrhi::IHeap *heap, uint64_t) {
    return buffer != nullptr && heap != nullptr;
}

nvrhi::BufferHandle null_device::createHandleForNativeBuffer(
  nvrhi::ObjectType,
  nvrhi::Object,
  const nvrhi::BufferDesc &desc
) {
    record([](null_device_stats &stats) {
        ++stats.buffersCreated;
    });
    return make_resource<null_buffer>(desc, next_virtual_address(desc.byteSize));
}

nvrhi::ShaderHandle null_device::createShader(const nvrhi::ShaderDesc &d, const void *binary, size_t binarySize) {
    // A real backend refuses an empty blob too; failing here keeps "fail at init" honest on the null device.
    if (binary == nullptr || binarySize == 0) {
        report_error("createShader: the shader bytecode is empty");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.shadersCreated;
    });
    return make_resource<null_shader>(
      d,
      std::span<const std::byte>(static_cast<const std::byte *>(binary), binarySize)
    );
}

nvrhi::ShaderHandle null_device::createShaderSpecialization(
  nvrhi::IShader *baseShader,
  const nvrhi::ShaderSpecialization *,
  uint32_t
) {
    if (baseShader == nullptr) {
        report_error("createShaderSpecialization: the base shader is null");
        return nullptr;
    }
    const void *bytecode = nullptr;
    size_t size = 0;
    baseShader->getBytecode(&bytecode, &size);
    record([](null_device_stats &stats) {
        ++stats.shadersCreated;
    });
    return make_resource<null_shader>(
      baseShader->getDesc(),
      std::span<const std::byte>(static_cast<const std::byte *>(bytecode), size)
    );
}

nvrhi::ShaderLibraryHandle null_device::createShaderLibrary(const void *binary, size_t binarySize) {
    if (binary == nullptr || binarySize == 0) {
        report_error("createShaderLibrary: the library bytecode is empty");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.shadersCreated;
    });
    return make_resource<null_shader_library>(
      std::span<const std::byte>(static_cast<const std::byte *>(binary), binarySize)
    );
}

nvrhi::SamplerHandle null_device::createSampler(const nvrhi::SamplerDesc &d) {
    record([](null_device_stats &stats) {
        ++stats.samplersCreated;
    });
    return make_resource<null_sampler>(d);
}

nvrhi::InputLayoutHandle null_device::createInputLayout(
  const nvrhi::VertexAttributeDesc *d,
  uint32_t attributeCount,
  nvrhi::IShader *
) {
    if (attributeCount > 0 && d == nullptr) {
        report_error("createInputLayout: attribute array is null");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.inputLayoutsCreated;
    });
    return make_resource<null_input_layout>(std::span<const nvrhi::VertexAttributeDesc>(d, attributeCount));
}


/*===========================================================================*/
/*===========================================================================*/
// Queries

nvrhi::EventQueryHandle null_device::createEventQuery() {
    record([](null_device_stats &stats) {
        ++stats.eventQueriesCreated;
    });
    return make_resource<null_event_query>();
}

void null_device::setEventQuery(nvrhi::IEventQuery *query, nvrhi::CommandQueue) {
    null_event_query *event = dynamic_cast<null_event_query *>(query);
    if (event != nullptr) {
        event->set();
    }
}

bool null_device::pollEventQuery(nvrhi::IEventQuery *query) {
    const null_event_query *event = dynamic_cast<null_event_query *>(query);
    return event != nullptr && event->is_signalled();
}

void null_device::waitEventQuery(nvrhi::IEventQuery *) {}

void null_device::resetEventQuery(nvrhi::IEventQuery *query) {
    null_event_query *event = dynamic_cast<null_event_query *>(query);
    if (event != nullptr) {
        event->reset();
    }
}

nvrhi::TimerQueryHandle null_device::createTimerQuery() {
    record([](null_device_stats &stats) {
        ++stats.timerQueriesCreated;
    });
    return make_resource<null_timer_query>();
}

bool null_device::pollTimerQuery(nvrhi::ITimerQuery *query) {
    const null_timer_query *timer = dynamic_cast<null_timer_query *>(query);
    return timer != nullptr && timer->is_started() && timer->is_ended();
}

float null_device::getTimerQueryTime(nvrhi::ITimerQuery *) {
    return 0.0f;
}

void null_device::resetTimerQuery(nvrhi::ITimerQuery *query) {
    null_timer_query *timer = dynamic_cast<null_timer_query *>(query);
    if (timer != nullptr) {
        timer->reset();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Framebuffers, pipelines, bindings

nvrhi::GraphicsAPI null_device::getGraphicsAPI() {
    return nvrhi::GraphicsAPI::D3D12;
}

nvrhi::FramebufferHandle null_device::createFramebuffer(const nvrhi::FramebufferDesc &desc) {
    if (desc.colorAttachments.empty() && !desc.depthAttachment.valid()) {
        report_error("createFramebuffer: a framebuffer needs at least one attachment");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.framebuffersCreated;
    });
    return make_resource<null_framebuffer>(desc);
}

nvrhi::GraphicsPipelineHandle null_device::createGraphicsPipeline(
  const nvrhi::GraphicsPipelineDesc &desc,
  const nvrhi::FramebufferInfo &fbinfo
) {
    if (desc.VS == nullptr) {
        report_error("createGraphicsPipeline: a vertex shader is required");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.graphicsPipelinesCreated;
    });
    return make_resource<null_graphics_pipeline>(desc, fbinfo);
}

nvrhi::GraphicsPipelineHandle null_device::createGraphicsPipeline(
  const nvrhi::GraphicsPipelineDesc &desc,
  nvrhi::IFramebuffer *fb
) {
    if (fb == nullptr) {
        report_error("createGraphicsPipeline: framebuffer is null");
        return nullptr;
    }
    return createGraphicsPipeline(desc, fb->getFramebufferInfo());
}

nvrhi::ComputePipelineHandle null_device::createComputePipeline(const nvrhi::ComputePipelineDesc &desc) {
    if (desc.CS == nullptr) {
        report_error("createComputePipeline: a compute shader is required");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.computePipelinesCreated;
    });
    return make_resource<null_compute_pipeline>(desc);
}

nvrhi::MeshletPipelineHandle null_device::createMeshletPipeline(
  const nvrhi::MeshletPipelineDesc &desc,
  const nvrhi::FramebufferInfo &fbinfo
) {
    if (desc.MS == nullptr) {
        report_error("createMeshletPipeline: a mesh shader is required");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.meshletPipelinesCreated;
    });
    return make_resource<null_meshlet_pipeline>(desc, fbinfo);
}

nvrhi::MeshletPipelineHandle null_device::createMeshletPipeline(
  const nvrhi::MeshletPipelineDesc &desc,
  nvrhi::IFramebuffer *fb
) {
    if (fb == nullptr) {
        report_error("createMeshletPipeline: framebuffer is null");
        return nullptr;
    }
    return createMeshletPipeline(desc, fb->getFramebufferInfo());
}

nvrhi::rt::PipelineHandle null_device::createRayTracingPipeline(const nvrhi::rt::PipelineDesc &desc) {
    return make_resource<null_ray_tracing_pipeline>(desc);
}

nvrhi::BindingLayoutHandle null_device::createBindingLayout(const nvrhi::BindingLayoutDesc &desc) {
    record([](null_device_stats &stats) {
        ++stats.bindingLayoutsCreated;
    });
    return make_resource<null_binding_layout>(desc);
}

nvrhi::BindingLayoutHandle null_device::createBindlessLayout(const nvrhi::BindlessLayoutDesc &desc) {
    record([](null_device_stats &stats) {
        ++stats.bindingLayoutsCreated;
    });
    return make_resource<null_binding_layout>(desc);
}

nvrhi::BindingSetHandle null_device::createBindingSet(
  const nvrhi::BindingSetDesc &desc,
  nvrhi::IBindingLayout *layout
) {
    if (layout == nullptr) {
        report_error("createBindingSet: layout is null");
        return nullptr;
    }
    record([](null_device_stats &stats) {
        ++stats.bindingSetsCreated;
    });
    return make_resource<null_binding_set>(desc, layout);
}

nvrhi::DescriptorTableHandle null_device::createDescriptorTable(nvrhi::IBindingLayout *layout) {
    record([](null_device_stats &stats) {
        ++stats.descriptorTablesCreated;
    });
    return make_resource<null_descriptor_table>(layout);
}

void null_device::resizeDescriptorTable(nvrhi::IDescriptorTable *descriptorTable, uint32_t newSize, bool keepContents) {
    null_descriptor_table *table = dynamic_cast<null_descriptor_table *>(descriptorTable);
    if (table != nullptr) {
        table->resize(newSize, keepContents);
    }
}

bool null_device::writeDescriptorTable(nvrhi::IDescriptorTable *descriptorTable, const nvrhi::BindingSetItem &item) {
    null_descriptor_table *table = dynamic_cast<null_descriptor_table *>(descriptorTable);
    return table != nullptr && table->write(item);
}


/*===========================================================================*/
/*===========================================================================*/
// Ray tracing

nvrhi::rt::OpacityMicromapHandle null_device::createOpacityMicromap(const nvrhi::rt::OpacityMicromapDesc &desc) {
    return make_resource<null_opacity_micromap>(desc, next_virtual_address(VIRTUAL_ADDRESS_ALIGNMENT));
}

nvrhi::rt::AccelStructHandle null_device::createAccelStruct(const nvrhi::rt::AccelStructDesc &desc) {
    return make_resource<null_accel_struct>(desc, next_virtual_address(VIRTUAL_ADDRESS_ALIGNMENT));
}

nvrhi::MemoryRequirements null_device::getAccelStructMemoryRequirements(nvrhi::rt::IAccelStruct *) {
    nvrhi::MemoryRequirements requirements = {};
    requirements.size = VIRTUAL_ADDRESS_ALIGNMENT;
    requirements.alignment = VIRTUAL_ADDRESS_ALIGNMENT;
    return requirements;
}

nvrhi::rt::cluster::OperationSizeInfo null_device::getClusterOperationSizeInfo(
  const nvrhi::rt::cluster::OperationParams &
) {
    return nvrhi::rt::cluster::OperationSizeInfo();
}

bool null_device::bindAccelStructMemory(nvrhi::rt::IAccelStruct *as, nvrhi::IHeap *heap, uint64_t) {
    return as != nullptr && heap != nullptr;
}


/*===========================================================================*/
/*===========================================================================*/
// Command lists and execution

nvrhi::CommandListHandle null_device::createCommandList(const nvrhi::CommandListParameters &params) {
    record([](null_device_stats &stats) {
        ++stats.commandListsCreated;
    });
    return make_resource<null_command_list>(*this, params);
}

uint64_t null_device::executeCommandLists(
  nvrhi::ICommandList *const *pCommandLists,
  size_t numCommandLists,
  nvrhi::CommandQueue
) {
    u32 submitted = 0;
    for (size_t index = 0; index < numCommandLists; ++index) {
        if (pCommandLists == nullptr || pCommandLists[index] == nullptr) {
            continue;
        }
        null_command_list *commandList = dynamic_cast<null_command_list *>(pCommandLists[index]);
        if (commandList != nullptr && commandList->is_open()) {
            report_error("executeCommandLists: a command list is still open");
            continue;
        }
        if (commandList != nullptr) {
            commandList->release_references();
        }
        ++submitted;
    }
    record([submitted](null_device_stats &stats) {
        stats.commandListsSubmitted += submitted;
    });
    return mNextFenceValue.fetch_add(1, std::memory_order_relaxed);
}

void null_device::queueWaitForCommandList(nvrhi::CommandQueue, nvrhi::CommandQueue, uint64_t) {}

bool null_device::waitForIdle() {
    record([](null_device_stats &stats) {
        ++stats.waitsForIdle;
    });
    // An armed WAIT_IDLE fault is the null device's stand-in for a fence wait that never completes.
    return !consume_fault(null_fault::WAIT_IDLE);
}

nvrhi::CommandListLifetimeTrackerHandle null_device::createCommandListLifetimeTracker(
  nvrhi::CommandQueue executionQueue
) {
    return make_resource<null_lifetime_tracker>(executionQueue);
}

void null_device::runGarbageCollection() {
    record([](null_device_stats &stats) {
        ++stats.garbageCollections;
    });
}


/*===========================================================================*/
/*===========================================================================*/
// Capabilities

bool null_device::queryFeatureSupport(nvrhi::Feature feature, void *pInfo, size_t infoSize) {
    switch (feature) {
        case nvrhi::Feature::ComputeQueue:
        case nvrhi::Feature::CopyQueue:
        case nvrhi::Feature::ConstantBufferRanges:
        case nvrhi::Feature::DeferredCommandLists:
        case nvrhi::Feature::VirtualResources: {
            return true;
        }
        case nvrhi::Feature::WaveLaneCountMinMax: {
            if (pInfo != nullptr && infoSize == sizeof(nvrhi::WaveLaneCountMinMaxFeatureInfo)) {
                nvrhi::WaveLaneCountMinMaxFeatureInfo *info = static_cast<nvrhi::WaveLaneCountMinMaxFeatureInfo *>(
                  pInfo
                );
                info->minWaveLaneCount = 32;
                info->maxWaveLaneCount = 32;
            }
            return true;
        }
        default: {
            return false;
        }
    }
}

nvrhi::FormatSupport null_device::queryFormatSupport(nvrhi::Format format) {
    const nvrhi::FormatInfo &info = nvrhi::getFormatInfo(format);
    if (format == nvrhi::Format::UNKNOWN || format >= nvrhi::Format::COUNT) {
        return nvrhi::FormatSupport::None;
    }

    // Like a D3D12 device: integer formats are loaded, never filtered, and block-compressed formats are
    // sampled but cannot be rendered to.
    nvrhi::FormatSupport support = nvrhi::FormatSupport::Texture | nvrhi::FormatSupport::ShaderLoad;
    if (info.kind != nvrhi::FormatKind::Integer) {
        support = support | nvrhi::FormatSupport::ShaderSample;
    }
    const bool compressed = info.blockSize > 1;
    if (info.hasDepth || info.hasStencil) {
        support = support | nvrhi::FormatSupport::DepthStencil;
        return support;
    }
    if (info.kind != nvrhi::FormatKind::Integer && !compressed) {
        support = support | nvrhi::FormatSupport::Blendable;
    }
    if (!compressed) {
        support = support | nvrhi::FormatSupport::RenderTarget | nvrhi::FormatSupport::Buffer |
          nvrhi::FormatSupport::VertexBuffer;
        if (!info.isSRGB) {
            support = support | nvrhi::FormatSupport::ShaderUavLoad | nvrhi::FormatSupport::ShaderUavStore;
        }
        if (format == nvrhi::Format::R32_UINT || format == nvrhi::Format::R32_SINT) {
            support = support | nvrhi::FormatSupport::ShaderAtomic;
        }
    }
    if (format == nvrhi::Format::R16_UINT || format == nvrhi::Format::R32_UINT) {
        support = support | nvrhi::FormatSupport::IndexBuffer;
    }
    return support;
}

nvrhi::coopvec::DeviceFeatures null_device::queryCoopVecFeatures() {
    return nvrhi::coopvec::DeviceFeatures();
}

nvrhi::coopvec::MatMulFormatSupport null_device::queryCoopVecMatMulFormatSupport(
  const nvrhi::coopvec::MatMulFormatCombo &
) {
    return nvrhi::coopvec::MatMulFormatSupport();
}

nvrhi::coopvec::TrainingFormatSupport null_device::queryCoopVecTrainingFormatSupport(nvrhi::coopvec::DataType) {
    return nvrhi::coopvec::TrainingFormatSupport();
}

size_t null_device::getCoopVecMatrixSize(nvrhi::coopvec::DataType, nvrhi::coopvec::MatrixLayout, int, int) {
    return 0;
}

nvrhi::Object null_device::getNativeQueue(nvrhi::ObjectType, nvrhi::CommandQueue) {
    return nvrhi::Object(nullptr);
}

nvrhi::IMessageCallback *null_device::getMessageCallback() {
    return &mMessages;
}

bool null_device::isAftermathEnabled() {
    return false;
}

nvrhi::AftermathCrashDumpHelper &null_device::getAftermathCrashDumpHelper() {
    return mAftermathHelper;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

u64 null_device::next_virtual_address(u64 bytes) noexcept {
    const u64 span = ((std::max<u64>(bytes, 1) + VIRTUAL_ADDRESS_ALIGNMENT - 1) / VIRTUAL_ADDRESS_ALIGNMENT) *
      VIRTUAL_ADDRESS_ALIGNMENT;
    return mNextVirtualAddress.fetch_add(span, std::memory_order_relaxed);
}

void null_device::report_error(const char *text) {
    mMessages.message(nvrhi::MessageSeverity::Error, text);
}


/*===========================================================================*/
/*===========================================================================*/
// null_device_backend

null_device_backend::null_device_backend(const device_desc &desc, message_callback &messages) :
    mDevice(make_resource<null_device>(messages)) {
    mCapabilities.backend = graphics_backend::NULL_DEVICE;
    mCapabilities.adapterName = "Conjure Null Device";
    mCapabilities.maxTextureDimension2d = 16384;
    mCapabilities.dedicatedVideoMemoryBytes = 0;
    mCapabilities.computeQueue = true;
    mCapabilities.timerQueries = true;
    mCapabilities.softwareAdapter = true;
    CONJURE_LOG_INFO(
      LOG_RHI,
      "created null device (debug layer {}, software adapter requested {})",
      desc.enableDebugLayer ? "on" : "off",
      desc.preferSoftwareAdapter ? "yes" : "no"
    );
}

null_device_backend::~null_device_backend() {}

nvrhi::IDevice *null_device_backend::nvrhi_device() const noexcept {
    return mDevice.Get();
}

const device_capabilities &null_device_backend::capabilities() const noexcept {
    return mCapabilities;
}

core::result<std::unique_ptr<swapchain_backend>> null_device_backend::create_swapchain(
  const native_window &,
  const swapchain_desc &desc
) {
    return std::unique_ptr<swapchain_backend>(std::make_unique<null_swapchain>(*mDevice, desc));
}

bool null_device_backend::poll_device_removed() noexcept {
    return false;
}

null_device *null_device_backend::as_null() noexcept {
    return mDevice.Get();
}

core::result<std::unique_ptr<device_backend>> conjure::rhi::null_backend::create_null_backend(
  const device_desc &desc,
  message_callback &messages
) {
    return std::unique_ptr<device_backend>(std::make_unique<null_device_backend>(desc, messages));
}
