#pragma once

#include "rhi/device_backend.h"

#include "conjure/rhi/backend.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/null_device_stats.h"

#include "conjure/core/types.h"

#include <nvrhi/common/aftermath.h>
#include <nvrhi/nvrhi.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

// The null backend (docs/design/02_renderer.md R17, 7; 08_testing.md 8.4): a complete
// nvrhi::IDevice that stores descriptors, returns valid handles and counts what command lists
// record. The whole renderer runs on it in CTest. It reports GraphicsAPI::D3D12 so code that
// branches on the API takes the zero-based SV_InstanceID convention the design assumes.
//
// Threading: resource creation and recording are render-thread operations like on a real
// backend; the statistics are guarded by a mutex so they may be read from any thread.
//
// Allocation: the device is the instrument of the renderer's allocation gates, so recording never
// allocates in steady state. Marker names are the only growing record; they are stored only once a
// consumer called reset_stats() (device::reset_null_stats()) and at most MAX_RECORDED_MARKER_NAMES
// per reset, so a run that never asks for them stores nothing.

namespace conjure::rhi {
    class message_callback;
}

namespace conjure::rhi::null_backend {

    class null_device final : public nvrhi::RefCounter<nvrhi::IDevice> {
        private:
            message_callback &mMessages;   // set by the only constructor; device::impl owns it and outlives the backend
            mutable std::mutex mStatsMutex = {};
            null_device_stats mStats = {};
            bool mRecordMarkerNames = false;    // guarded by mStatsMutex; armed by the first reset_stats()
            std::atomic<u64> mNextFenceValue = 1;
            std::atomic<u64> mNextVirtualAddress = 0x10000;    // fake GPU addresses, 64 KiB apart, never 0
            std::atomic<null_fault> mPendingFault = null_fault::NONE;
            nvrhi::AftermathCrashDumpHelper mAftermathHelper = {};

        public:
            explicit null_device(message_callback &messages);
            ~null_device() override;

        public:
            /// <summary>
            /// Applies 'fn(null_device_stats &)' under the statistics lock.
            /// </summary>
            template <typename Fn>
            void record(Fn &&fn) {
                const std::scoped_lock lock(mStatsMutex);
                std::forward<Fn>(fn)(mStats);
            }

            /// <summary>
            /// Counts a beginMarker and stores its name when a consumer asked for names (reset_stats()
            /// has been called) and fewer than MAX_RECORDED_MARKER_NAMES are stored; otherwise the name
            /// is counted in markerNamesDropped.
            /// </summary>
            void record_marker(const char *name);

            /// <summary>
            /// Snapshot of the statistics.
            /// </summary>
            [[nodiscard]] null_device_stats stats() const;

            /// <summary>
            /// Resets every counter and starts (or restarts) storing marker names.
            /// </summary>
            void reset_stats();

            /// <summary>
            /// Arms one failure (device::inject_null_fault); the next call that matches it reports the
            /// failure and disarms. NONE disarms.
            /// </summary>
            void inject_fault(null_fault fault) noexcept;

            /// <summary>
            /// True, once, when 'fault' is the armed failure; disarms it.
            /// </summary>
            [[nodiscard]] bool consume_fault(null_fault fault) noexcept;

            /// <summary>
            /// The message callback of the owning device, for backend objects that report device loss.
            /// </summary>
            [[nodiscard]] message_callback &messages() const noexcept {
                return mMessages;
            }

            // nvrhi::IDevice
            nvrhi::HeapHandle createHeap(const nvrhi::HeapDesc &d) override;
            bool queryTopLevelAccelStructPrebuildInfo(
              const nvrhi::rt::AccelStructDesc &desc,
              uint32_t instanceCount,
              nvrhi::rt::AccelStructPrebuildInfo &outInfo
            ) override;
            nvrhi::TextureHandle createTexture(const nvrhi::TextureDesc &d) override;
            nvrhi::MemoryRequirements getTextureMemoryRequirements(nvrhi::ITexture *texture) override;
            bool bindTextureMemory(nvrhi::ITexture *texture, nvrhi::IHeap *heap, uint64_t offset) override;
            nvrhi::TextureHandle createHandleForNativeTexture(
              nvrhi::ObjectType objectType,
              nvrhi::Object texture,
              const nvrhi::TextureDesc &desc
            ) override;
            nvrhi::StagingTextureHandle createStagingTexture(
              const nvrhi::TextureDesc &d,
              nvrhi::CpuAccessMode cpuAccess
            ) override;
            void *mapStagingTexture(
              nvrhi::IStagingTexture *tex,
              const nvrhi::TextureSlice &slice,
              nvrhi::CpuAccessMode cpuAccess,
              size_t *outRowPitch
            ) override;
            void unmapStagingTexture(nvrhi::IStagingTexture *tex) override;
            void getTextureTiling(
              nvrhi::ITexture *texture,
              uint32_t *numTiles,
              nvrhi::PackedMipDesc *desc,
              nvrhi::TileShape *tileShape,
              uint32_t *subresourceTilingsNum,
              nvrhi::SubresourceTiling *subresourceTilings
            ) override;
            void updateTextureTileMappings(
              nvrhi::ITexture *texture,
              const nvrhi::TextureTilesMapping *tileMappings,
              uint32_t numTileMappings,
              nvrhi::CommandQueue executionQueue
            ) override;
            nvrhi::SamplerFeedbackTextureHandle createSamplerFeedbackTexture(
              nvrhi::ITexture *pairedTexture,
              const nvrhi::SamplerFeedbackTextureDesc &desc
            ) override;
            nvrhi::SamplerFeedbackTextureHandle createSamplerFeedbackForNativeTexture(
              nvrhi::ObjectType objectType,
              nvrhi::Object texture,
              nvrhi::ITexture *pairedTexture
            ) override;
            nvrhi::BufferHandle createBuffer(const nvrhi::BufferDesc &d) override;
            void *mapBuffer(nvrhi::IBuffer *buffer, nvrhi::CpuAccessMode cpuAccess) override;
            void unmapBuffer(nvrhi::IBuffer *buffer) override;
            nvrhi::MemoryRequirements getBufferMemoryRequirements(nvrhi::IBuffer *buffer) override;
            bool bindBufferMemory(nvrhi::IBuffer *buffer, nvrhi::IHeap *heap, uint64_t offset) override;
            nvrhi::BufferHandle createHandleForNativeBuffer(
              nvrhi::ObjectType objectType,
              nvrhi::Object buffer,
              const nvrhi::BufferDesc &desc
            ) override;
            nvrhi::ShaderHandle createShader(
              const nvrhi::ShaderDesc &d,
              const void *binary,
              size_t binarySize
            ) override;
            nvrhi::ShaderHandle createShaderSpecialization(
              nvrhi::IShader *baseShader,
              const nvrhi::ShaderSpecialization *constants,
              uint32_t numConstants
            ) override;
            nvrhi::ShaderLibraryHandle createShaderLibrary(const void *binary, size_t binarySize) override;
            nvrhi::SamplerHandle createSampler(const nvrhi::SamplerDesc &d) override;
            nvrhi::InputLayoutHandle createInputLayout(
              const nvrhi::VertexAttributeDesc *d,
              uint32_t attributeCount,
              nvrhi::IShader *vertexShader
            ) override;
            nvrhi::EventQueryHandle createEventQuery() override;
            void setEventQuery(nvrhi::IEventQuery *query, nvrhi::CommandQueue queue) override;
            bool pollEventQuery(nvrhi::IEventQuery *query) override;
            void waitEventQuery(nvrhi::IEventQuery *query) override;
            void resetEventQuery(nvrhi::IEventQuery *query) override;
            nvrhi::TimerQueryHandle createTimerQuery() override;
            bool pollTimerQuery(nvrhi::ITimerQuery *query) override;
            float getTimerQueryTime(nvrhi::ITimerQuery *query) override;
            void resetTimerQuery(nvrhi::ITimerQuery *query) override;
            nvrhi::GraphicsAPI getGraphicsAPI() override;
            nvrhi::FramebufferHandle createFramebuffer(const nvrhi::FramebufferDesc &desc) override;
            nvrhi::GraphicsPipelineHandle createGraphicsPipeline(
              const nvrhi::GraphicsPipelineDesc &desc,
              const nvrhi::FramebufferInfo &fbinfo
            ) override;
            nvrhi::GraphicsPipelineHandle createGraphicsPipeline(
              const nvrhi::GraphicsPipelineDesc &desc,
              nvrhi::IFramebuffer *fb
            ) override;
            nvrhi::ComputePipelineHandle createComputePipeline(const nvrhi::ComputePipelineDesc &desc) override;
            nvrhi::MeshletPipelineHandle createMeshletPipeline(
              const nvrhi::MeshletPipelineDesc &desc,
              const nvrhi::FramebufferInfo &fbinfo
            ) override;
            nvrhi::MeshletPipelineHandle createMeshletPipeline(
              const nvrhi::MeshletPipelineDesc &desc,
              nvrhi::IFramebuffer *fb
            ) override;
            nvrhi::rt::PipelineHandle createRayTracingPipeline(const nvrhi::rt::PipelineDesc &desc) override;
            nvrhi::BindingLayoutHandle createBindingLayout(const nvrhi::BindingLayoutDesc &desc) override;
            nvrhi::BindingLayoutHandle createBindlessLayout(const nvrhi::BindlessLayoutDesc &desc) override;
            nvrhi::BindingSetHandle createBindingSet(
              const nvrhi::BindingSetDesc &desc,
              nvrhi::IBindingLayout *layout
            ) override;
            nvrhi::DescriptorTableHandle createDescriptorTable(nvrhi::IBindingLayout *layout) override;
            void resizeDescriptorTable(
              nvrhi::IDescriptorTable *descriptorTable,
              uint32_t newSize,
              bool keepContents
            ) override;
            bool writeDescriptorTable(
              nvrhi::IDescriptorTable *descriptorTable,
              const nvrhi::BindingSetItem &item
            ) override;
            nvrhi::rt::OpacityMicromapHandle createOpacityMicromap(const nvrhi::rt::OpacityMicromapDesc &desc) override;
            nvrhi::rt::AccelStructHandle createAccelStruct(const nvrhi::rt::AccelStructDesc &desc) override;
            nvrhi::MemoryRequirements getAccelStructMemoryRequirements(nvrhi::rt::IAccelStruct *as) override;
            nvrhi::rt::cluster::OperationSizeInfo getClusterOperationSizeInfo(
              const nvrhi::rt::cluster::OperationParams &params
            ) override;
            bool bindAccelStructMemory(nvrhi::rt::IAccelStruct *as, nvrhi::IHeap *heap, uint64_t offset) override;
            nvrhi::CommandListHandle createCommandList(const nvrhi::CommandListParameters &params) override;
            uint64_t executeCommandLists(
              nvrhi::ICommandList *const *pCommandLists,
              size_t numCommandLists,
              nvrhi::CommandQueue executionQueue
            ) override;
            void queueWaitForCommandList(
              nvrhi::CommandQueue waitQueue,
              nvrhi::CommandQueue executionQueue,
              uint64_t instance
            ) override;
            bool waitForIdle() override;
            nvrhi::CommandListLifetimeTrackerHandle createCommandListLifetimeTracker(
              nvrhi::CommandQueue executionQueue
            ) override;
            void runGarbageCollection() override;
            bool queryFeatureSupport(nvrhi::Feature feature, void *pInfo, size_t infoSize) override;
            nvrhi::FormatSupport queryFormatSupport(nvrhi::Format format) override;
            nvrhi::coopvec::DeviceFeatures queryCoopVecFeatures() override;
            nvrhi::coopvec::MatMulFormatSupport queryCoopVecMatMulFormatSupport(
              const nvrhi::coopvec::MatMulFormatCombo &combination
            ) override;
            nvrhi::coopvec::TrainingFormatSupport queryCoopVecTrainingFormatSupport(
              nvrhi::coopvec::DataType componentType
            ) override;
            size_t getCoopVecMatrixSize(
              nvrhi::coopvec::DataType type,
              nvrhi::coopvec::MatrixLayout layout,
              int rows,
              int columns
            ) override;
            nvrhi::Object getNativeQueue(nvrhi::ObjectType objectType, nvrhi::CommandQueue queue) override;
            nvrhi::IMessageCallback *getMessageCallback() override;
            bool isAftermathEnabled() override;
            nvrhi::AftermathCrashDumpHelper &getAftermathCrashDumpHelper() override;

        private:
            [[nodiscard]] u64 next_virtual_address(u64 bytes) noexcept;
            void report_error(const char *text);
    };

    /// <summary>
    /// The device_backend of the null device: owns the null_device handle and creates null swapchains.
    /// </summary>
    class null_device_backend final : public device_backend {
        private:
            device_capabilities mCapabilities = {};
            nvrhi::RefCountPtr<null_device> mDevice = nullptr;

        public:
            null_device_backend(const device_desc &desc, message_callback &messages);
            ~null_device_backend() override;

        public:
            [[nodiscard]] nvrhi::IDevice *nvrhi_device() const noexcept override;
            [[nodiscard]] const device_capabilities &capabilities() const noexcept override;
            [[nodiscard]] core::result<std::unique_ptr<swapchain_backend>> create_swapchain(
              const native_window &window,
              const swapchain_desc &desc
            ) override;
            [[nodiscard]] bool poll_device_removed() noexcept override;
            [[nodiscard]] null_device *as_null() noexcept override;
    };

    /// <summary>
    /// Creates the null backend; never fails.
    /// </summary>
    [[nodiscard]] core::result<std::unique_ptr<device_backend>> create_null_backend(
      const device_desc &desc,
      message_callback &messages
    );
}
