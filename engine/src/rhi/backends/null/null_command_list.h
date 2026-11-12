#pragma once

#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <optional>
#include <vector>

// The null backend's command list (docs/design/02_renderer.md R17): records counters and debug
// marker names into the owning null_device's stats, applies buffer writes and copies to the CPU
// storage of null buffers (an out-of-range one reports an Error and stores nothing), and tracks the
// states set explicitly (beginTracking*State, set*State, setPermanent*State) so
// getTextureSubresourceState / getBufferState report them. The implicit transitions of automatic
// barriers (setGraphicsState, clears, copies, writes) are not modelled, and an untracked resource
// reports its initialState when keepInitialState is set, Unknown otherwise. Per-list tracking lives
// here and resets on open(); permanent states live on the null_texture / null_buffer themselves
// (null_resources.h). Nothing is executed. The per-list containers (referenced resources, state
// trackers) keep their storage across open() so a command list that records the same frame again
// allocates nothing (the renderer's allocation gates measure through this device).

namespace conjure::rhi::null_backend {

    class null_device;

    class null_command_list final : public nvrhi::RefCounter<nvrhi::ICommandList> {
        private:
            struct subresource_key {
                nvrhi::ITexture *texture = nullptr;
                u32 arraySlice = 0;
                u32 mipLevel = 0;

                [[nodiscard]] bool operator==(const subresource_key &other) const noexcept = default;
            };

            struct subresource_key_hash {
                [[nodiscard]] usize operator()(const subresource_key &key) const noexcept;
            };

            null_device &mDevice;   // set by the only constructor; the device owns and outlives its command lists
            nvrhi::CommandListParameters mParameters = {};
            bool mOpen = false;
            std::vector<nvrhi::ResourceHandle> mReferencedResources = {};   // liveness until the next open()
            core::flat_hash_map<subresource_key, nvrhi::ResourceStates, subresource_key_hash> mTextureStates = {};
            core::flat_hash_map<nvrhi::IBuffer *, nvrhi::ResourceStates> mBufferStates = {};

        public:
            null_command_list(null_device &device, const nvrhi::CommandListParameters &parameters);
            ~null_command_list() override;

        public:
            [[nodiscard]] bool is_open() const noexcept {
                return mOpen;
            }

            // nvrhi::ICommandList
            void open() override;
            void close() override;
            void clearState() override;
            void clearTextureFloat(
              nvrhi::ITexture *t,
              nvrhi::TextureSubresourceSet subresources,
              const nvrhi::Color &clearColor
            ) override;
            void clearDepthStencilTexture(
              nvrhi::ITexture *t,
              nvrhi::TextureSubresourceSet subresources,
              bool clearDepth,
              float depth,
              bool clearStencil,
              uint8_t stencil
            ) override;
            void clearTextureUInt(
              nvrhi::ITexture *t,
              nvrhi::TextureSubresourceSet subresources,
              uint32_t clearColor
            ) override;
            void copyTexture(
              nvrhi::ITexture *dest,
              const nvrhi::TextureSlice &destSlice,
              nvrhi::ITexture *src,
              const nvrhi::TextureSlice &srcSlice
            ) override;
            void copyTexture(
              nvrhi::IStagingTexture *dest,
              const nvrhi::TextureSlice &destSlice,
              nvrhi::ITexture *src,
              const nvrhi::TextureSlice &srcSlice
            ) override;
            void copyTexture(
              nvrhi::ITexture *dest,
              const nvrhi::TextureSlice &destSlice,
              nvrhi::IStagingTexture *src,
              const nvrhi::TextureSlice &srcSlice
            ) override;
            void writeTexture(
              nvrhi::ITexture *dest,
              uint32_t arraySlice,
              uint32_t mipLevel,
              const void *data,
              size_t rowPitch,
              size_t depthPitch
            ) override;
            void resolveTexture(
              nvrhi::ITexture *dest,
              const nvrhi::TextureSubresourceSet &dstSubresources,
              nvrhi::ITexture *src,
              const nvrhi::TextureSubresourceSet &srcSubresources
            ) override;
            void writeBuffer(nvrhi::IBuffer *b, const void *data, size_t dataSize, uint64_t destOffsetBytes) override;
            void clearBufferUInt(nvrhi::IBuffer *b, uint32_t clearValue) override;
            void copyBuffer(
              nvrhi::IBuffer *dest,
              uint64_t destOffsetBytes,
              nvrhi::IBuffer *src,
              uint64_t srcOffsetBytes,
              uint64_t dataSizeBytes
            ) override;
            void clearSamplerFeedbackTexture(nvrhi::ISamplerFeedbackTexture *texture) override;
            void decodeSamplerFeedbackTexture(
              nvrhi::IBuffer *buffer,
              nvrhi::ISamplerFeedbackTexture *texture,
              nvrhi::Format format
            ) override;
            void setSamplerFeedbackTextureState(
              nvrhi::ISamplerFeedbackTexture *texture,
              nvrhi::ResourceStates stateBits
            ) override;
            void setPushConstants(const void *data, size_t byteSize) override;
            void setGraphicsState(const nvrhi::GraphicsState &state) override;
            void draw(const nvrhi::DrawArguments &args) override;
            void drawIndexed(const nvrhi::DrawArguments &args) override;
            void drawIndirect(uint32_t offsetBytes, uint32_t drawCount) override;
            void drawIndexedIndirect(uint32_t offsetBytes, uint32_t drawCount) override;
            void drawIndexedIndirectCount(
              uint32_t paramOffsetBytes,
              uint32_t countOffsetBytes,
              uint32_t maxDrawCount
            ) override;
            void setComputeState(const nvrhi::ComputeState &state) override;
            void dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;
            void dispatchIndirect(uint32_t offsetBytes) override;
            void setMeshletState(const nvrhi::MeshletState &state) override;
            void dispatchMesh(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;
            void dispatchMeshIndirect(uint32_t offsetBytes, uint32_t maxDrawCount) override;
            void dispatchMeshIndirectCount(
              uint32_t paramOffsetBytes,
              uint32_t countOffsetBytes,
              uint32_t maxDrawCount
            ) override;
            void setRayTracingState(const nvrhi::rt::State &state) override;
            void dispatchRays(const nvrhi::rt::DispatchRaysArguments &args) override;
            void buildOpacityMicromap(
              nvrhi::rt::IOpacityMicromap *omm,
              const nvrhi::rt::OpacityMicromapDesc &desc
            ) override;
            void buildBottomLevelAccelStruct(
              nvrhi::rt::IAccelStruct *as,
              const nvrhi::rt::GeometryDesc *pGeometries,
              size_t numGeometries,
              nvrhi::rt::AccelStructBuildFlags buildFlags
            ) override;
            void compactBottomLevelAccelStructs() override;
            void copyRaytracingAccelerationStructure(
              nvrhi::rt::IAccelStruct *destination,
              nvrhi::rt::IAccelStruct *source
            ) override;
            void buildTopLevelAccelStruct(
              nvrhi::rt::IAccelStruct *as,
              const nvrhi::rt::InstanceDesc *pInstances,
              size_t numInstances,
              nvrhi::rt::AccelStructBuildFlags buildFlags
            ) override;
            void executeMultiIndirectClusterOperation(const nvrhi::rt::cluster::OperationDesc &desc) override;
            void buildTopLevelAccelStructFromBuffer(
              nvrhi::rt::IAccelStruct *as,
              nvrhi::IBuffer *instanceBuffer,
              uint64_t instanceBufferOffset,
              size_t numInstances,
              nvrhi::rt::AccelStructBuildFlags buildFlags
            ) override;
            void convertCoopVecMatrices(
              const nvrhi::coopvec::ConvertMatrixLayoutDesc *convertDescs,
              size_t numDescs
            ) override;
            void beginTimerQuery(nvrhi::ITimerQuery *query) override;
            void endTimerQuery(nvrhi::ITimerQuery *query) override;
            void beginMarker(const char *name) override;
            void endMarker() override;
            void setEnableAutomaticBarriers(bool enable) override;
            void setResourceStatesForBindingSet(nvrhi::IBindingSet *bindingSet) override;
            void setEnableUavBarriersForTexture(nvrhi::ITexture *texture, bool enableBarriers) override;
            void setEnableUavBarriersForBuffer(nvrhi::IBuffer *buffer, bool enableBarriers) override;
            void beginTrackingTextureState(
              nvrhi::ITexture *texture,
              nvrhi::TextureSubresourceSet subresources,
              nvrhi::ResourceStates stateBits
            ) override;
            void beginTrackingBufferState(nvrhi::IBuffer *buffer, nvrhi::ResourceStates stateBits) override;
            void setTextureState(
              nvrhi::ITexture *texture,
              nvrhi::TextureSubresourceSet subresources,
              nvrhi::ResourceStates stateBits
            ) override;
            void setBufferState(nvrhi::IBuffer *buffer, nvrhi::ResourceStates stateBits) override;
            void setAccelStructState(nvrhi::rt::IAccelStruct *as, nvrhi::ResourceStates stateBits) override;
            void setPermanentTextureState(nvrhi::ITexture *texture, nvrhi::ResourceStates stateBits) override;
            void setPermanentBufferState(nvrhi::IBuffer *buffer, nvrhi::ResourceStates stateBits) override;
            void commitBarriers() override;
            nvrhi::ResourceStates getTextureSubresourceState(
              nvrhi::ITexture *texture,
              nvrhi::ArraySlice arraySlice,
              nvrhi::MipLevel mipLevel
            ) override;
            nvrhi::ResourceStates getBufferState(nvrhi::IBuffer *buffer) override;
            nvrhi::IDevice *getDevice() override;
            const nvrhi::CommandListParameters &getDesc() override;
            [[nodiscard]] nvrhi::CommandListMemoryUsage getInternalMemoryUsage() const override;

        private:
            void reference(nvrhi::IResource *resource);
            void report_error(const char *text);

            // Applies a writeBuffer to the null buffer's storage and returns the bytes stored: 0 for a
            // null buffer or empty data, and 0 with an Error for a range past the end of the buffer.
            [[nodiscard]] u64 store_buffer_write(
              nvrhi::IBuffer *b,
              const void *data,
              usize dataSize,
              u64 destOffsetBytes
            );
            [[nodiscard]] static std::optional<nvrhi::ResourceStates> permanent_texture_state(
              nvrhi::ITexture *texture
            ) noexcept;
            [[nodiscard]] static std::optional<nvrhi::ResourceStates> permanent_buffer_state(
              nvrhi::IBuffer *buffer
            ) noexcept;
            void track_texture(
              nvrhi::ITexture *texture,
              nvrhi::TextureSubresourceSet subresources,
              nvrhi::ResourceStates state
            );
            void track_buffer(nvrhi::IBuffer *buffer, nvrhi::ResourceStates state);
    };
}
