#include "rhi/backends/null/null_command_list.h"

#include "rhi/backends/null/null_device.h"
#include "rhi/backends/null/null_resources.h"
#include "rhi/message_callback.h"

#include "conjure/core/hash.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::rhi::null_backend;

namespace {
    /// <summary>
    /// True when [offset, offset + size) lies inside a buffer of 'capacity' bytes, without overflowing.
    /// </summary>
    bool range_fits(u64 offset, u64 size, u64 capacity) noexcept {
        return size <= capacity && offset <= capacity - size;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

null_command_list::null_command_list(null_device &device, const nvrhi::CommandListParameters &parameters) :
    mDevice(device),
    mParameters(parameters) {}

null_command_list::~null_command_list() {}


/*===========================================================================*/
/*===========================================================================*/
// Lifecycle

void null_command_list::open() {
    mOpen = true;
    mReferencedResources.clear();
    mTextureStates.clear();
    mBufferStates.clear();
    mDevice.record([](null_device_stats &stats) {
        ++stats.commandListsOpened;
    });
}

void null_command_list::close() {
    mOpen = false;
    mDevice.record([](null_device_stats &stats) {
        ++stats.commandListsClosed;
    });
}

void null_command_list::clearState() {}


/*===========================================================================*/
/*===========================================================================*/
// Clears, copies and writes

void null_command_list::clearTextureFloat(nvrhi::ITexture *t, nvrhi::TextureSubresourceSet, const nvrhi::Color &) {
    reference(t);
    mDevice.record([](null_device_stats &stats) {
        ++stats.textureClears;
    });
}

void null_command_list::clearDepthStencilTexture(
  nvrhi::ITexture *t,
  nvrhi::TextureSubresourceSet,
  bool,
  float,
  bool,
  uint8_t
) {
    reference(t);
    mDevice.record([](null_device_stats &stats) {
        ++stats.depthClears;
    });
}

void null_command_list::clearTextureUInt(nvrhi::ITexture *t, nvrhi::TextureSubresourceSet, uint32_t) {
    reference(t);
    mDevice.record([](null_device_stats &stats) {
        ++stats.textureClears;
    });
}

void null_command_list::copyTexture(
  nvrhi::ITexture *dest,
  const nvrhi::TextureSlice &,
  nvrhi::ITexture *src,
  const nvrhi::TextureSlice &
) {
    reference(dest);
    reference(src);
    mDevice.record([](null_device_stats &stats) {
        ++stats.textureCopies;
    });
}

void null_command_list::copyTexture(
  nvrhi::IStagingTexture *dest,
  const nvrhi::TextureSlice &,
  nvrhi::ITexture *src,
  const nvrhi::TextureSlice &
) {
    reference(dest);
    reference(src);
    mDevice.record([](null_device_stats &stats) {
        ++stats.textureCopies;
    });
}

void null_command_list::copyTexture(
  nvrhi::ITexture *dest,
  const nvrhi::TextureSlice &,
  nvrhi::IStagingTexture *src,
  const nvrhi::TextureSlice &
) {
    reference(dest);
    reference(src);
    mDevice.record([](null_device_stats &stats) {
        ++stats.textureCopies;
    });
}

void null_command_list::writeTexture(nvrhi::ITexture *dest, uint32_t, uint32_t, const void *, size_t, size_t) {
    reference(dest);
    mDevice.record([](null_device_stats &stats) {
        ++stats.textureWrites;
    });
}

void null_command_list::resolveTexture(
  nvrhi::ITexture *dest,
  const nvrhi::TextureSubresourceSet &,
  nvrhi::ITexture *src,
  const nvrhi::TextureSubresourceSet &
) {
    reference(dest);
    reference(src);
    mDevice.record([](null_device_stats &stats) {
        ++stats.textureCopies;
    });
}

void null_command_list::writeBuffer(nvrhi::IBuffer *b, const void *data, size_t dataSize, uint64_t destOffsetBytes) {
    reference(b);
    const u64 stored = store_buffer_write(b, data, dataSize, destOffsetBytes);
    mDevice.record([stored](null_device_stats &stats) {
        ++stats.bufferWrites;
        stats.bufferWriteBytes += stored;
    });
}

void null_command_list::clearBufferUInt(nvrhi::IBuffer *b, uint32_t) {
    reference(b);
    mDevice.record([](null_device_stats &stats) {
        ++stats.bufferClears;
    });
}

void null_command_list::copyBuffer(
  nvrhi::IBuffer *dest,
  uint64_t destOffsetBytes,
  nvrhi::IBuffer *src,
  uint64_t srcOffsetBytes,
  uint64_t dataSizeBytes
) {
    reference(dest);
    reference(src);
    mDevice.record([](null_device_stats &stats) {
        ++stats.bufferCopies;
    });

    null_buffer *destination = dynamic_cast<null_buffer *>(dest);
    null_buffer *source = dynamic_cast<null_buffer *>(src);
    if (destination == nullptr || source == nullptr) {
        return;
    }
    // D3D12 rejects a copy that leaves either buffer; storing the part that fits would hide the bug.
    const bool sourceFits = range_fits(srcOffsetBytes, dataSizeBytes, source->getDesc().byteSize);
    const bool destinationFits = range_fits(destOffsetBytes, dataSizeBytes, destination->getDesc().byteSize);
    if (!sourceFits || !destinationFits) {
        report_error("copyBuffer: the range exceeds the source or destination buffer");
        return;
    }
    // D3D12 also rejects overlapping regions of one buffer. Both ranges fit, so the sums cannot overflow.
    const bool overlaps = destination == source && srcOffsetBytes < destOffsetBytes + dataSizeBytes &&
      destOffsetBytes < srcOffsetBytes + dataSizeBytes;
    if (overlaps) {
        report_error("copyBuffer: the source and destination ranges of one buffer overlap");
        return;
    }
    const std::span<const std::byte> sourceBytes = source->storage();
    destination->write(
      destOffsetBytes,
      sourceBytes.subspan(static_cast<usize>(srcOffsetBytes), static_cast<usize>(dataSizeBytes))
    );
}

void null_command_list::clearSamplerFeedbackTexture(nvrhi::ISamplerFeedbackTexture *texture) {
    reference(texture);
    mDevice.record([](null_device_stats &stats) {
        ++stats.textureClears;
    });
}

void null_command_list::decodeSamplerFeedbackTexture(
  nvrhi::IBuffer *buffer,
  nvrhi::ISamplerFeedbackTexture *texture,
  nvrhi::Format
) {
    reference(buffer);
    reference(texture);
    mDevice.record([](null_device_stats &stats) {
        ++stats.textureCopies;
    });
}

void null_command_list::setSamplerFeedbackTextureState(nvrhi::ISamplerFeedbackTexture *texture, nvrhi::ResourceStates) {
    reference(texture);
}


/*===========================================================================*/
/*===========================================================================*/
// Draws and dispatches

void null_command_list::setPushConstants(const void *, size_t) {
    mDevice.record([](null_device_stats &stats) {
        ++stats.pushConstantWrites;
    });
}

void null_command_list::setGraphicsState(const nvrhi::GraphicsState &state) {
    reference(state.pipeline);
    reference(state.framebuffer);
    for (nvrhi::IBindingSet *bindingSet : state.bindings) {
        reference(bindingSet);
    }
    for (const nvrhi::VertexBufferBinding &binding : state.vertexBuffers) {
        reference(binding.buffer);
    }
    reference(state.indexBuffer.buffer);
    reference(state.indirectParams);
    reference(state.indirectCountBuffer);
    mDevice.record([](null_device_stats &stats) {
        ++stats.graphicsStateChanges;
    });
}

void null_command_list::draw(const nvrhi::DrawArguments &args) {
    mDevice.record([&args](null_device_stats &stats) {
        ++stats.draws;
        stats.instancesDrawn += args.instanceCount;
    });
}

void null_command_list::drawIndexed(const nvrhi::DrawArguments &args) {
    mDevice.record([&args](null_device_stats &stats) {
        ++stats.draws;
        stats.instancesDrawn += args.instanceCount;
    });
}

void null_command_list::drawIndirect(uint32_t, uint32_t drawCount) {
    mDevice.record([drawCount](null_device_stats &stats) {
        stats.drawsIndirect += drawCount;
    });
}

void null_command_list::drawIndexedIndirect(uint32_t, uint32_t drawCount) {
    mDevice.record([drawCount](null_device_stats &stats) {
        stats.drawsIndirect += drawCount;
    });
}

void null_command_list::drawIndexedIndirectCount(uint32_t, uint32_t, uint32_t maxDrawCount) {
    mDevice.record([maxDrawCount](null_device_stats &stats) {
        stats.drawsIndirect += maxDrawCount;
    });
}

void null_command_list::setComputeState(const nvrhi::ComputeState &state) {
    reference(state.pipeline);
    for (nvrhi::IBindingSet *bindingSet : state.bindings) {
        reference(bindingSet);
    }
    reference(state.indirectParams);
    mDevice.record([](null_device_stats &stats) {
        ++stats.computeStateChanges;
    });
}

void null_command_list::dispatch(uint32_t, uint32_t, uint32_t) {
    mDevice.record([](null_device_stats &stats) {
        ++stats.dispatches;
    });
}

void null_command_list::dispatchIndirect(uint32_t) {
    mDevice.record([](null_device_stats &stats) {
        ++stats.dispatchesIndirect;
    });
}

void null_command_list::setMeshletState(const nvrhi::MeshletState &state) {
    reference(state.pipeline);
    reference(state.framebuffer);
    for (nvrhi::IBindingSet *bindingSet : state.bindings) {
        reference(bindingSet);
    }
    reference(state.indirectParams);
    reference(state.indirectCountBuffer);
    mDevice.record([](null_device_stats &stats) {
        ++stats.graphicsStateChanges;
    });
}

void null_command_list::dispatchMesh(uint32_t, uint32_t, uint32_t) {
    mDevice.record([](null_device_stats &stats) {
        ++stats.meshDispatches;
    });
}

void null_command_list::dispatchMeshIndirect(uint32_t, uint32_t) {
    mDevice.record([](null_device_stats &stats) {
        ++stats.meshDispatches;
    });
}

void null_command_list::dispatchMeshIndirectCount(uint32_t, uint32_t, uint32_t) {
    mDevice.record([](null_device_stats &stats) {
        ++stats.meshDispatches;
    });
}


/*===========================================================================*/
/*===========================================================================*/
// Ray tracing (recorded as references only; the null device builds nothing)

void null_command_list::setRayTracingState(const nvrhi::rt::State &state) {
    reference(state.shaderTable);
    for (nvrhi::IBindingSet *bindingSet : state.bindings) {
        reference(bindingSet);
    }
}

void null_command_list::dispatchRays(const nvrhi::rt::DispatchRaysArguments &) {
    mDevice.record([](null_device_stats &stats) {
        ++stats.dispatches;
    });
}

void null_command_list::buildOpacityMicromap(nvrhi::rt::IOpacityMicromap *omm, const nvrhi::rt::OpacityMicromapDesc &) {
    reference(omm);
}

void null_command_list::buildBottomLevelAccelStruct(
  nvrhi::rt::IAccelStruct *as,
  const nvrhi::rt::GeometryDesc *,
  size_t,
  nvrhi::rt::AccelStructBuildFlags
) {
    reference(as);
}

void null_command_list::compactBottomLevelAccelStructs() {}

void null_command_list::copyRaytracingAccelerationStructure(
  nvrhi::rt::IAccelStruct *destination,
  nvrhi::rt::IAccelStruct *source
) {
    reference(destination);
    reference(source);
}

void null_command_list::buildTopLevelAccelStruct(
  nvrhi::rt::IAccelStruct *as,
  const nvrhi::rt::InstanceDesc *,
  size_t,
  nvrhi::rt::AccelStructBuildFlags
) {
    reference(as);
}

void null_command_list::executeMultiIndirectClusterOperation(const nvrhi::rt::cluster::OperationDesc &desc) {
    reference(desc.inIndirectArgCountBuffer);
    reference(desc.inIndirectArgsBuffer);
    reference(desc.inOutAddressesBuffer);
    reference(desc.outSizesBuffer);
    reference(desc.outAccelerationStructuresBuffer);
}

void null_command_list::buildTopLevelAccelStructFromBuffer(
  nvrhi::rt::IAccelStruct *as,
  nvrhi::IBuffer *instanceBuffer,
  uint64_t,
  size_t,
  nvrhi::rt::AccelStructBuildFlags
) {
    reference(as);
    reference(instanceBuffer);
}

void null_command_list::convertCoopVecMatrices(
  const nvrhi::coopvec::ConvertMatrixLayoutDesc *convertDescs,
  size_t numDescs
) {
    if (convertDescs == nullptr) {
        return;
    }
    for (size_t index = 0; index < numDescs; ++index) {
        reference(convertDescs[index].src.buffer);
        reference(convertDescs[index].dst.buffer);
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Queries and markers

void null_command_list::beginTimerQuery(nvrhi::ITimerQuery *query) {
    reference(query);
    null_timer_query *timer = dynamic_cast<null_timer_query *>(query);
    if (timer != nullptr) {
        timer->begin();
    }
    mDevice.record([](null_device_stats &stats) {
        ++stats.timerQueriesBegun;
    });
}

void null_command_list::endTimerQuery(nvrhi::ITimerQuery *query) {
    null_timer_query *timer = dynamic_cast<null_timer_query *>(query);
    if (timer != nullptr) {
        timer->end();
    }
}

void null_command_list::beginMarker(const char *name) {
    mDevice.record_marker(name);
}

void null_command_list::endMarker() {
    mDevice.record([](null_device_stats &stats) {
        ++stats.markersEnded;
    });
}


/*===========================================================================*/
/*===========================================================================*/
// Resource state tracking

void null_command_list::setEnableAutomaticBarriers(bool) {}

void null_command_list::setResourceStatesForBindingSet(nvrhi::IBindingSet *bindingSet) {
    reference(bindingSet);
}

void null_command_list::setEnableUavBarriersForTexture(nvrhi::ITexture *, bool) {}

void null_command_list::setEnableUavBarriersForBuffer(nvrhi::IBuffer *, bool) {}

void null_command_list::beginTrackingTextureState(
  nvrhi::ITexture *texture,
  nvrhi::TextureSubresourceSet subresources,
  nvrhi::ResourceStates stateBits
) {
    track_texture(texture, subresources, stateBits);
}

void null_command_list::beginTrackingBufferState(nvrhi::IBuffer *buffer, nvrhi::ResourceStates stateBits) {
    track_buffer(buffer, stateBits);
}

void null_command_list::setTextureState(
  nvrhi::ITexture *texture,
  nvrhi::TextureSubresourceSet subresources,
  nvrhi::ResourceStates stateBits
) {
    reference(texture);
    if (permanent_texture_state(texture).has_value()) {
        return;
    }
    track_texture(texture, subresources, stateBits);
}

void null_command_list::setBufferState(nvrhi::IBuffer *buffer, nvrhi::ResourceStates stateBits) {
    reference(buffer);
    if (buffer == nullptr || permanent_buffer_state(buffer).has_value()) {
        return;
    }
    track_buffer(buffer, stateBits);
}

void null_command_list::setAccelStructState(nvrhi::rt::IAccelStruct *as, nvrhi::ResourceStates) {
    reference(as);
}

void null_command_list::setPermanentTextureState(nvrhi::ITexture *texture, nvrhi::ResourceStates stateBits) {
    reference(texture);
    null_texture *target = dynamic_cast<null_texture *>(texture);
    if (target == nullptr) {
        return;
    }
    target->set_permanent_state(stateBits);
    track_texture(texture, nvrhi::AllSubresources, stateBits);
}

void null_command_list::setPermanentBufferState(nvrhi::IBuffer *buffer, nvrhi::ResourceStates stateBits) {
    reference(buffer);
    null_buffer *target = dynamic_cast<null_buffer *>(buffer);
    if (target == nullptr) {
        return;
    }
    target->set_permanent_state(stateBits);
    track_buffer(buffer, stateBits);
}

void null_command_list::commitBarriers() {}

nvrhi::ResourceStates null_command_list::getTextureSubresourceState(
  nvrhi::ITexture *texture,
  nvrhi::ArraySlice arraySlice,
  nvrhi::MipLevel mipLevel
) {
    if (texture == nullptr) {
        return nvrhi::ResourceStates::Unknown;
    }
    const std::optional<nvrhi::ResourceStates> permanent = permanent_texture_state(texture);
    if (permanent.has_value()) {
        return *permanent;
    }
    const auto tracked = mTextureStates.find(subresource_key{texture, arraySlice, mipLevel});
    if (tracked != mTextureStates.end()) {
        return tracked->second;
    }
    const nvrhi::TextureDesc &desc = texture->getDesc();
    return desc.keepInitialState ? desc.initialState : nvrhi::ResourceStates::Unknown;
}

nvrhi::ResourceStates null_command_list::getBufferState(nvrhi::IBuffer *buffer) {
    if (buffer == nullptr) {
        return nvrhi::ResourceStates::Unknown;
    }
    const std::optional<nvrhi::ResourceStates> permanent = permanent_buffer_state(buffer);
    if (permanent.has_value()) {
        return *permanent;
    }
    const auto tracked = mBufferStates.find(buffer);
    if (tracked != mBufferStates.end()) {
        return tracked->second;
    }
    const nvrhi::BufferDesc &desc = buffer->getDesc();
    return desc.keepInitialState ? desc.initialState : nvrhi::ResourceStates::Unknown;
}

nvrhi::IDevice *null_command_list::getDevice() {
    return &mDevice;
}

const nvrhi::CommandListParameters &null_command_list::getDesc() {
    return mParameters;
}

nvrhi::CommandListMemoryUsage null_command_list::getInternalMemoryUsage() const {
    return nvrhi::CommandListMemoryUsage();
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

usize null_command_list::subresource_key_hash::operator()(const subresource_key &key) const noexcept {
    u64 hash = core::hash::FNV1A64_OFFSET_BASIS;
    const std::uintptr_t pointerBits = reinterpret_cast<std::uintptr_t>(key.texture);
    for (usize shift = 0; shift < sizeof(pointerBits) * 8; shift += 8) {
        hash = core::hash::fnv1a64_step(hash, static_cast<u8>(pointerBits >> shift));
    }
    for (usize shift = 0; shift < 32; shift += 8) {
        hash = core::hash::fnv1a64_step(hash, static_cast<u8>(key.arraySlice >> shift));
        hash = core::hash::fnv1a64_step(hash, static_cast<u8>(key.mipLevel >> shift));
    }
    return static_cast<usize>(hash);
}

void null_command_list::reference(nvrhi::IResource *resource) {
    if (resource == nullptr) {
        return;
    }
    mReferencedResources.emplace_back(resource);
}

void null_command_list::report_error(const char *text) {
    mDevice.messages().message(nvrhi::MessageSeverity::Error, text);
}

u64 null_command_list::store_buffer_write(nvrhi::IBuffer *b, const void *data, usize dataSize, u64 destOffsetBytes) {
    null_buffer *buffer = dynamic_cast<null_buffer *>(b);
    if (buffer == nullptr || data == nullptr || dataSize == 0) {
        return 0;
    }
    // Like nvrhi::validation (and D3D12), a write past the end stores nothing rather than a clamped part.
    if (!range_fits(destOffsetBytes, dataSize, buffer->getDesc().byteSize)) {
        report_error("writeBuffer: dataSize + destOffsetBytes exceeds the buffer size");
        return 0;
    }
    buffer->write(destOffsetBytes, std::span<const std::byte>(static_cast<const std::byte *>(data), dataSize));
    return dataSize;
}

std::optional<nvrhi::ResourceStates> null_command_list::permanent_texture_state(nvrhi::ITexture *texture) noexcept {
    const null_texture *target = dynamic_cast<const null_texture *>(texture);
    if (target == nullptr) {
        return std::nullopt;
    }
    return target->permanent_state();
}

std::optional<nvrhi::ResourceStates> null_command_list::permanent_buffer_state(nvrhi::IBuffer *buffer) noexcept {
    const null_buffer *target = dynamic_cast<const null_buffer *>(buffer);
    if (target == nullptr) {
        return std::nullopt;
    }
    return target->permanent_state();
}

void null_command_list::track_texture(
  nvrhi::ITexture *texture,
  nvrhi::TextureSubresourceSet subresources,
  nvrhi::ResourceStates state
) {
    if (texture == nullptr) {
        return;
    }
    const nvrhi::TextureSubresourceSet resolved = subresources.resolve(texture->getDesc(), false);
    for (u32 slice = 0; slice < resolved.numArraySlices; ++slice) {
        for (u32 mip = 0; mip < resolved.numMipLevels; ++mip) {
            const subresource_key key{texture, resolved.baseArraySlice + slice, resolved.baseMipLevel + mip};
            mTextureStates.try_emplace(key, state).first->second = state;
        }
    }
}

void null_command_list::track_buffer(nvrhi::IBuffer *buffer, nvrhi::ResourceStates state) {
    if (buffer == nullptr) {
        return;
    }
    mBufferStates.try_emplace(buffer, state).first->second = state;
}
