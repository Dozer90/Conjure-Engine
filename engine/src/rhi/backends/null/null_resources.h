#pragma once

#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

// Resources of the null backend (docs/design/02_renderer.md R17, 7). Every class stores the
// descriptor it was created from and returns it through the NVRHI getters; nothing talks to a
// GPU. Buffers and staging textures own zero-filled CPU storage so map/unmap and readback work.

namespace conjure::rhi::null_backend {

    /// <summary>
    /// Creates an NVRHI resource with a reference count of one. NVRHI resources are intrusively
    /// counted and RefCounter::Release deletes the object itself, so the allocation has to be a plain
    /// new to match; this is the single place in the module that performs one.
    /// </summary>
    template <typename Resource, typename... Args>
    [[nodiscard]] nvrhi::RefCountPtr<Resource> make_resource(Args &&...args) {
        return nvrhi::RefCountPtr<Resource>::Create(new Resource(std::forward<Args>(args)...));
    }

    /*-----------------------------------------------------------------------*/
    // Memory and textures

    class null_heap final : public nvrhi::RefCounter<nvrhi::IHeap> {
        private:
            nvrhi::HeapDesc mDesc = {};

        public:
            explicit null_heap(nvrhi::HeapDesc desc);
            ~null_heap() override;

        public:
            const nvrhi::HeapDesc &getDesc() override;
    };

    class null_texture final : public nvrhi::RefCounter<nvrhi::ITexture> {
        private:
            nvrhi::TextureDesc mDesc = {};
            std::optional<nvrhi::ResourceStates> mPermanentState = std::nullopt;

        public:
            explicit null_texture(nvrhi::TextureDesc desc);
            ~null_texture() override;

        public:
            [[nodiscard]] const nvrhi::TextureDesc &getDesc() const override;
            nvrhi::Object getNativeView(
              nvrhi::ObjectType objectType,
              nvrhi::Format format,
              nvrhi::TextureSubresourceSet subresources,
              nvrhi::TextureDimension dimension,
              bool isReadOnlyDSV,
              std::optional<nvrhi::ComponentMapping> overrideComponentMapping
            ) override;

            /// <summary>
            /// The state setPermanentTextureState pinned, or nullopt. Like on D3D12 and Vulkan the
            /// permanent state is a property of the resource, visible to every command list and gone
            /// with the texture, never a per-list record keyed by an address that may be reused.
            /// </summary>
            [[nodiscard]] std::optional<nvrhi::ResourceStates> permanent_state() const noexcept {
                return mPermanentState;
            }
            void set_permanent_state(nvrhi::ResourceStates state) noexcept {
                mPermanentState = state;
            }
    };

    class null_staging_texture final : public nvrhi::RefCounter<nvrhi::IStagingTexture> {
        private:
            nvrhi::TextureDesc mDesc = {};
            nvrhi::CpuAccessMode mCpuAccess = nvrhi::CpuAccessMode::None;
            std::vector<std::byte> mStorage = {};
            bool mMapped = false;

        public:
            null_staging_texture(nvrhi::TextureDesc desc, nvrhi::CpuAccessMode cpuAccess);
            ~null_staging_texture() override;

        public:
            [[nodiscard]] const nvrhi::TextureDesc &getDesc() const override;

            /// <summary>
            /// Zero-filled memory for one slice of the texture; the row pitch is the tightly packed one.
            /// </summary>
            [[nodiscard]] void *map(const nvrhi::TextureSlice &slice, usize *outRowPitch);
            void unmap() noexcept;
            [[nodiscard]] bool is_mapped() const noexcept {
                return mMapped;
            }
            [[nodiscard]] nvrhi::CpuAccessMode cpu_access() const noexcept {
                return mCpuAccess;
            }
    };

    class null_sampler_feedback_texture final : public nvrhi::RefCounter<nvrhi::ISamplerFeedbackTexture> {
        private:
            nvrhi::SamplerFeedbackTextureDesc mDesc = {};
            nvrhi::TextureHandle mPairedTexture = nullptr;

        public:
            null_sampler_feedback_texture(nvrhi::SamplerFeedbackTextureDesc desc, nvrhi::ITexture *pairedTexture);
            ~null_sampler_feedback_texture() override;

        public:
            [[nodiscard]] const nvrhi::SamplerFeedbackTextureDesc &getDesc() const override;
            nvrhi::TextureHandle getPairedTexture() override;
    };

    /*-----------------------------------------------------------------------*/
    // Buffers, shaders, samplers, input layouts

    class null_buffer final : public nvrhi::RefCounter<nvrhi::IBuffer> {
        private:
            nvrhi::BufferDesc mDesc = {};
            nvrhi::GpuVirtualAddress mAddress = 0;
            std::vector<std::byte> mStorage = {};
            std::optional<nvrhi::ResourceStates> mPermanentState = std::nullopt;
            bool mMapped = false;

        public:
            null_buffer(nvrhi::BufferDesc desc, nvrhi::GpuVirtualAddress address);
            ~null_buffer() override;

        public:
            [[nodiscard]] const nvrhi::BufferDesc &getDesc() const override;
            [[nodiscard]] nvrhi::GpuVirtualAddress getGpuVirtualAddress() const override;

            /// <summary>
            /// CPU storage of byteSize bytes, zero-filled on first use, so writes persist across maps.
            /// </summary>
            [[nodiscard]] void *map();
            void unmap() noexcept;
            [[nodiscard]] bool is_mapped() const noexcept {
                return mMapped;
            }

            /// <summary>
            /// Copies data into the CPU storage (writeBuffer, copyBuffer). Callers range-check first and
            /// report an out-of-range or overlapping request; out-of-range bytes are dropped here and
            /// data may alias the storage, both only defensively.
            /// </summary>
            void write(u64 offset, std::span<const std::byte> data);
            [[nodiscard]] std::span<const std::byte> storage();

            /// <summary>
            /// The state setPermanentBufferState pinned, or nullopt (see null_texture::permanent_state).
            /// </summary>
            [[nodiscard]] std::optional<nvrhi::ResourceStates> permanent_state() const noexcept {
                return mPermanentState;
            }
            void set_permanent_state(nvrhi::ResourceStates state) noexcept {
                mPermanentState = state;
            }
    };

    class null_shader final : public nvrhi::RefCounter<nvrhi::IShader> {
        private:
            nvrhi::ShaderDesc mDesc = {};
            std::vector<std::byte> mBytecode = {};

        public:
            null_shader(nvrhi::ShaderDesc desc, std::span<const std::byte> bytecode);
            ~null_shader() override;

        public:
            [[nodiscard]] const nvrhi::ShaderDesc &getDesc() const override;
            void getBytecode(const void **ppBytecode, size_t *pSize) const override;
    };

    class null_shader_library final : public nvrhi::RefCounter<nvrhi::IShaderLibrary> {
        private:
            std::vector<std::byte> mBytecode = {};

        public:
            explicit null_shader_library(std::span<const std::byte> bytecode);
            ~null_shader_library() override;

        public:
            void getBytecode(const void **ppBytecode, size_t *pSize) const override;
            nvrhi::ShaderHandle getShader(const char *entryName, nvrhi::ShaderType shaderType) override;
    };

    class null_sampler final : public nvrhi::RefCounter<nvrhi::ISampler> {
        private:
            nvrhi::SamplerDesc mDesc = {};

        public:
            explicit null_sampler(nvrhi::SamplerDesc desc);
            ~null_sampler() override;

        public:
            [[nodiscard]] const nvrhi::SamplerDesc &getDesc() const override;
    };

    class null_input_layout final : public nvrhi::RefCounter<nvrhi::IInputLayout> {
        private:
            std::vector<nvrhi::VertexAttributeDesc> mAttributes = {};

        public:
            explicit null_input_layout(std::span<const nvrhi::VertexAttributeDesc> attributes);
            ~null_input_layout() override;

        public:
            [[nodiscard]] uint32_t getNumAttributes() const override;
            [[nodiscard]] const nvrhi::VertexAttributeDesc *getAttributeDesc(uint32_t index) const override;
    };

    /*-----------------------------------------------------------------------*/
    // Queries

    class null_event_query final : public nvrhi::RefCounter<nvrhi::IEventQuery> {
        private:
            bool mSignalled = false;

        public:
            null_event_query();
            ~null_event_query() override;

        public:
            void set() noexcept {
                mSignalled = true;
            }
            void reset() noexcept {
                mSignalled = false;
            }
            [[nodiscard]] bool is_signalled() const noexcept {
                return mSignalled;
            }
    };

    class null_timer_query final : public nvrhi::RefCounter<nvrhi::ITimerQuery> {
        private:
            bool mStarted = false;
            bool mEnded = false;

        public:
            null_timer_query();
            ~null_timer_query() override;

        public:
            void begin() noexcept {
                mStarted = true;
                mEnded = false;
            }
            void end() noexcept {
                mEnded = true;
            }
            void reset() noexcept {
                mStarted = false;
                mEnded = false;
            }
            [[nodiscard]] bool is_started() const noexcept {
                return mStarted;
            }
            [[nodiscard]] bool is_ended() const noexcept {
                return mEnded;
            }
    };

    /*-----------------------------------------------------------------------*/
    // Framebuffers, bindings, pipelines

    class null_framebuffer final : public nvrhi::RefCounter<nvrhi::IFramebuffer> {
        private:
            nvrhi::FramebufferDesc mDesc = {};
            nvrhi::FramebufferInfoEx mInfo = {};
            std::vector<nvrhi::TextureHandle> mAttachments = {};   // keeps the attached textures alive

        public:
            explicit null_framebuffer(const nvrhi::FramebufferDesc &desc);
            ~null_framebuffer() override;

        public:
            [[nodiscard]] const nvrhi::FramebufferDesc &getDesc() const override;
            [[nodiscard]] const nvrhi::FramebufferInfoEx &getFramebufferInfo() const override;
    };

    class null_binding_layout final : public nvrhi::RefCounter<nvrhi::IBindingLayout> {
        private:
            std::optional<nvrhi::BindingLayoutDesc> mDesc = std::nullopt;
            std::optional<nvrhi::BindlessLayoutDesc> mBindlessDesc = std::nullopt;

        public:
            explicit null_binding_layout(nvrhi::BindingLayoutDesc desc);
            explicit null_binding_layout(nvrhi::BindlessLayoutDesc desc);
            ~null_binding_layout() override;

        public:
            [[nodiscard]] const nvrhi::BindingLayoutDesc *getDesc() const override;
            [[nodiscard]] const nvrhi::BindlessLayoutDesc *getBindlessDesc() const override;
    };

    class null_binding_set final : public nvrhi::RefCounter<nvrhi::IBindingSet> {
        private:
            nvrhi::BindingSetDesc mDesc = {};
            nvrhi::BindingLayoutHandle mLayout = nullptr;
            std::vector<nvrhi::ResourceHandle> mResources = {};   // liveness tracking of the bound resources

        public:
            null_binding_set(nvrhi::BindingSetDesc desc, nvrhi::IBindingLayout *layout);
            ~null_binding_set() override;

        public:
            [[nodiscard]] const nvrhi::BindingSetDesc *getDesc() const override;
            [[nodiscard]] nvrhi::IBindingLayout *getLayout() const override;
    };

    class null_descriptor_table final : public nvrhi::RefCounter<nvrhi::IDescriptorTable> {
        private:
            nvrhi::BindingLayoutHandle mLayout = nullptr;
            std::vector<nvrhi::BindingSetItem> mItems = {};

        public:
            explicit null_descriptor_table(nvrhi::IBindingLayout *layout);
            ~null_descriptor_table() override;

        public:
            [[nodiscard]] const nvrhi::BindingSetDesc *getDesc() const override;
            [[nodiscard]] nvrhi::IBindingLayout *getLayout() const override;
            [[nodiscard]] uint32_t getCapacity() const override;
            [[nodiscard]] uint32_t getFirstDescriptorIndexInHeap() const override;

            void resize(uint32_t newSize, bool keepContents);
            [[nodiscard]] bool write(const nvrhi::BindingSetItem &item);
    };

    class null_graphics_pipeline final : public nvrhi::RefCounter<nvrhi::IGraphicsPipeline> {
        private:
            nvrhi::GraphicsPipelineDesc mDesc = {};
            nvrhi::FramebufferInfo mFramebufferInfo = {};

        public:
            null_graphics_pipeline(nvrhi::GraphicsPipelineDesc desc, nvrhi::FramebufferInfo framebufferInfo);
            ~null_graphics_pipeline() override;

        public:
            [[nodiscard]] const nvrhi::GraphicsPipelineDesc &getDesc() const override;
            [[nodiscard]] const nvrhi::FramebufferInfo &getFramebufferInfo() const override;
    };

    class null_compute_pipeline final : public nvrhi::RefCounter<nvrhi::IComputePipeline> {
        private:
            nvrhi::ComputePipelineDesc mDesc = {};

        public:
            explicit null_compute_pipeline(nvrhi::ComputePipelineDesc desc);
            ~null_compute_pipeline() override;

        public:
            [[nodiscard]] const nvrhi::ComputePipelineDesc &getDesc() const override;
    };

    class null_meshlet_pipeline final : public nvrhi::RefCounter<nvrhi::IMeshletPipeline> {
        private:
            nvrhi::MeshletPipelineDesc mDesc = {};
            nvrhi::FramebufferInfo mFramebufferInfo = {};

        public:
            null_meshlet_pipeline(nvrhi::MeshletPipelineDesc desc, nvrhi::FramebufferInfo framebufferInfo);
            ~null_meshlet_pipeline() override;

        public:
            [[nodiscard]] const nvrhi::MeshletPipelineDesc &getDesc() const override;
            [[nodiscard]] const nvrhi::FramebufferInfo &getFramebufferInfo() const override;
    };

    /*-----------------------------------------------------------------------*/
    // Ray tracing (never built on the null device; the objects exist so handles stay valid)

    class null_accel_struct final : public nvrhi::RefCounter<nvrhi::rt::IAccelStruct> {
        private:
            nvrhi::rt::AccelStructDesc mDesc = {};
            u64 mDeviceAddress = 0;

        public:
            null_accel_struct(nvrhi::rt::AccelStructDesc desc, u64 deviceAddress);
            ~null_accel_struct() override;

        public:
            [[nodiscard]] const nvrhi::rt::AccelStructDesc &getDesc() const override;
            [[nodiscard]] bool isCompacted() const override;
            [[nodiscard]] uint64_t getDeviceAddress() const override;
    };

    class null_opacity_micromap final : public nvrhi::RefCounter<nvrhi::rt::IOpacityMicromap> {
        private:
            nvrhi::rt::OpacityMicromapDesc mDesc = {};
            u64 mDeviceAddress = 0;

        public:
            null_opacity_micromap(nvrhi::rt::OpacityMicromapDesc desc, u64 deviceAddress);
            ~null_opacity_micromap() override;

        public:
            [[nodiscard]] const nvrhi::rt::OpacityMicromapDesc &getDesc() const override;
            [[nodiscard]] bool isCompacted() const override;
            [[nodiscard]] uint64_t getDeviceAddress() const override;
    };

    class null_ray_tracing_pipeline;

    class null_shader_table final : public nvrhi::RefCounter<nvrhi::rt::IShaderTable> {
        private:
            nvrhi::rt::ShaderTableDesc mDesc = {};
            nvrhi::RefCountPtr<null_ray_tracing_pipeline> mPipeline = nullptr;
            bool mHasRayGeneration = false;
            u32 mMissShaders = 0;
            u32 mHitGroups = 0;
            u32 mCallableShaders = 0;

        public:
            null_shader_table(nvrhi::rt::ShaderTableDesc desc, null_ray_tracing_pipeline *pipeline);
            ~null_shader_table() override;

        public:
            const nvrhi::rt::ShaderTableDesc &getDesc() const override;
            uint32_t getNumEntries() const override;
            nvrhi::rt::IPipeline *getPipeline() const override;
            void setRayGenerationShader(const char *exportName, nvrhi::IBindingSet *bindings) override;
            int addMissShader(const char *exportName, nvrhi::IBindingSet *bindings) override;
            int addHitGroup(const char *exportName, nvrhi::IBindingSet *bindings) override;
            int addCallableShader(const char *exportName, nvrhi::IBindingSet *bindings) override;
            void clearMissShaders() override;
            void clearHitShaders() override;
            void clearCallableShaders() override;
    };

    class null_ray_tracing_pipeline final : public nvrhi::RefCounter<nvrhi::rt::IPipeline> {
        private:
            nvrhi::rt::PipelineDesc mDesc = {};

        public:
            explicit null_ray_tracing_pipeline(nvrhi::rt::PipelineDesc desc);
            ~null_ray_tracing_pipeline() override;

        public:
            [[nodiscard]] const nvrhi::rt::PipelineDesc &getDesc() const override;
            nvrhi::rt::ShaderTableHandle createShaderTable(const nvrhi::rt::ShaderTableDesc &desc) override;
    };

    /*-----------------------------------------------------------------------*/
    // Lifetime tracking

    class null_lifetime_tracker final : public nvrhi::RefCounter<nvrhi::ICommandListLifetimeTracker> {
        private:
            nvrhi::CommandQueue mQueue = nvrhi::CommandQueue::Graphics;
            u32 mCollections = 0;

        public:
            explicit null_lifetime_tracker(nvrhi::CommandQueue queue);
            ~null_lifetime_tracker() override;

        public:
            void runGarbageCollection() override;
            [[nodiscard]] u32 collection_count() const noexcept {
                return mCollections;
            }
            [[nodiscard]] nvrhi::CommandQueue queue() const noexcept {
                return mQueue;
            }
    };
}
