#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "conjure/testing/test_environment.h"
#include "rhi_test_support.h"

#include "conjure/rhi/device.h"
#include "conjure/rhi/null_device_stats.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/log.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// The null backend as an nvrhi::IDevice (docs/design/02_renderer.md R17, 7; 08_testing.md 8.4;
// 10 "test_null_device"). The null device is reachable only through device::native_device(), an
// NVRHI interface, so this file includes the NVRHI header: it is the API under test.

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::testing;
using namespace conjure::testing::rhi_support;

namespace {
    constexpr std::array<std::byte, 4> FAKE_BYTECODE = {
        std::byte{0x44},
        std::byte{0x58},
        std::byte{0x42},
        std::byte{0x43},
    };

    nvrhi::TextureDesc colour_texture_desc(u32 width, u32 height) {
        nvrhi::TextureDesc desc = {};
        desc.width = width;
        desc.height = height;
        desc.format = nvrhi::Format::RGBA8_UNORM;
        desc.isRenderTarget = true;
        desc.initialState = nvrhi::ResourceStates::ShaderResource;
        desc.keepInitialState = true;
        desc.debugName = "colour";
        return desc;
    }

    nvrhi::BufferDesc vertex_buffer_desc(u64 bytes) {
        nvrhi::BufferDesc desc = {};
        desc.byteSize = bytes;
        desc.isVertexBuffer = true;
        desc.initialState = nvrhi::ResourceStates::VertexBuffer;
        desc.keepInitialState = true;
        desc.debugName = "vertices";
        return desc;
    }

    nvrhi::ShaderHandle make_shader(nvrhi::IDevice &native, nvrhi::ShaderType type) {
        nvrhi::ShaderDesc desc = {};
        desc.shaderType = type;
        desc.entryName = "main";
        return native.createShader(desc, FAKE_BYTECODE.data(), FAKE_BYTECODE.size());
    }

    null_device_stats stats_of(const device &created) {
        const std::optional<null_device_stats> stats = created.null_stats();
        REQUIRE(stats.has_value());
        return *stats;
    }
}

TEST_SUITE("rhi/null_device") {

    TEST_CASE("null_device - reports D3D12 semantics, a message callback and no Aftermath") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        CHECK(native.getGraphicsAPI() == nvrhi::GraphicsAPI::D3D12);
        CHECK(native.getMessageCallback() != nullptr);
        CHECK_FALSE(native.isAftermathEnabled());
        const nvrhi::ObjectType queueType = nvrhi::ObjectTypes::D3D12_CommandQueue;
        CHECK(native.getNativeQueue(queueType, nvrhi::CommandQueue::Graphics).pointer == nullptr);
        const nvrhi::coopvec::DataType half = nvrhi::coopvec::DataType::Float16;
        CHECK(native.getCoopVecMatrixSize(half, nvrhi::coopvec::MatrixLayout::RowMajor, 4, 4) == 0);
        CHECK_FALSE(native.queryCoopVecMatMulFormatSupport(nvrhi::coopvec::MatMulFormatCombo()).supported);
        CHECK_FALSE(
          native.queryCoopVecTrainingFormatSupport(nvrhi::coopvec::DataType::Float32).bufferTrainingSupported
        );
        CHECK(native.queryCoopVecFeatures().matMulFormats.empty());
    }

    TEST_CASE("null_device - feature support names the queues and refuses GPU-only features") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        CHECK(native.queryFeatureSupport(nvrhi::Feature::ComputeQueue));
        CHECK(native.queryFeatureSupport(nvrhi::Feature::CopyQueue));
        CHECK(native.queryFeatureSupport(nvrhi::Feature::ConstantBufferRanges));
        CHECK(native.queryFeatureSupport(nvrhi::Feature::VirtualResources));
        CHECK_FALSE(native.queryFeatureSupport(nvrhi::Feature::Meshlets));
        CHECK_FALSE(native.queryFeatureSupport(nvrhi::Feature::RayTracingPipeline));
        CHECK_FALSE(native.queryFeatureSupport(nvrhi::Feature::VariableRateShading));
        CHECK_FALSE(native.queryFeatureSupport(nvrhi::Feature::EnhancedBarriers));
        nvrhi::WaveLaneCountMinMaxFeatureInfo waveInfo = {};
        CHECK(native.queryFeatureSupport(nvrhi::Feature::WaveLaneCountMinMax, &waveInfo, sizeof(waveInfo)));
        CHECK(waveInfo.minWaveLaneCount == 32);
        CHECK(waveInfo.maxWaveLaneCount == 32);
    }

    TEST_CASE("null_device - format support follows the format kind") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        const nvrhi::FormatSupport colour = native.queryFormatSupport(nvrhi::Format::RGBA8_UNORM);
        CHECK((colour & nvrhi::FormatSupport::RenderTarget) != 0);
        CHECK((colour & nvrhi::FormatSupport::Blendable) != 0);
        CHECK((colour & nvrhi::FormatSupport::ShaderUavStore) != 0);
        CHECK((colour & nvrhi::FormatSupport::DepthStencil) == 0);
        CHECK((colour & nvrhi::FormatSupport::IndexBuffer) == 0);

        const nvrhi::FormatSupport depth = native.queryFormatSupport(nvrhi::Format::D32);
        CHECK((depth & nvrhi::FormatSupport::DepthStencil) != 0);
        CHECK((depth & nvrhi::FormatSupport::RenderTarget) == 0);
        CHECK((depth & nvrhi::FormatSupport::ShaderSample) != 0);

        const nvrhi::FormatSupport srgb = native.queryFormatSupport(nvrhi::Format::SRGBA8_UNORM);
        CHECK((srgb & nvrhi::FormatSupport::RenderTarget) != 0);
        CHECK((srgb & nvrhi::FormatSupport::ShaderUavStore) == 0);

        const nvrhi::FormatSupport compressed = native.queryFormatSupport(nvrhi::Format::BC7_UNORM_SRGB);
        CHECK((compressed & nvrhi::FormatSupport::Texture) != 0);
        CHECK((compressed & nvrhi::FormatSupport::Buffer) == 0);
        CHECK((compressed & nvrhi::FormatSupport::Blendable) == 0);

        const nvrhi::FormatSupport indices = native.queryFormatSupport(nvrhi::Format::R32_UINT);
        CHECK((indices & nvrhi::FormatSupport::IndexBuffer) != 0);
        CHECK((indices & nvrhi::FormatSupport::ShaderAtomic) != 0);
        CHECK((indices & nvrhi::FormatSupport::Blendable) == 0);

        CHECK(native.queryFormatSupport(nvrhi::Format::UNKNOWN) == nvrhi::FormatSupport::None);
        CHECK(native.queryFormatSupport(nvrhi::Format::COUNT) == nvrhi::FormatSupport::None);
    }

    TEST_CASE("null_device - every resource type is creatable and keeps its descriptor") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();

        nvrhi::HeapDesc heapDesc = {};
        heapDesc.capacity = 1024;
        heapDesc.type = nvrhi::HeapType::DeviceLocal;
        const nvrhi::HeapHandle heap = native.createHeap(heapDesc);
        REQUIRE(heap != nullptr);
        CHECK(heap->getDesc().capacity == 1024);

        const nvrhi::TextureHandle texture = native.createTexture(colour_texture_desc(64, 32));
        REQUIRE(texture != nullptr);
        CHECK(texture->getDesc().width == 64);
        CHECK(texture->getDesc().debugName == "colour");
        CHECK(texture->getNativeView(nvrhi::ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor).pointer == nullptr);
        CHECK(native.bindTextureMemory(texture, heap, 0));
        CHECK(native.getTextureMemoryRequirements(texture).size == 64 * 32 * 4);

        const nvrhi::StagingTextureHandle staging = native.createStagingTexture(
          colour_texture_desc(8, 8),
          nvrhi::CpuAccessMode::Read
        );
        REQUIRE(staging != nullptr);
        CHECK(staging->getDesc().height == 8);

        const nvrhi::SamplerFeedbackTextureHandle feedback = native.createSamplerFeedbackTexture(
          texture,
          nvrhi::SamplerFeedbackTextureDesc()
        );
        REQUIRE(feedback != nullptr);
        CHECK(feedback->getPairedTexture() == texture);

        const nvrhi::BufferHandle buffer = native.createBuffer(vertex_buffer_desc(256));
        REQUIRE(buffer != nullptr);
        CHECK(buffer->getDesc().byteSize == 256);
        CHECK(buffer->getGpuVirtualAddress() != 0);
        CHECK(native.bindBufferMemory(buffer, heap, 0));
        CHECK(native.getBufferMemoryRequirements(buffer).size == 256);
        const nvrhi::BufferHandle other = native.createBuffer(vertex_buffer_desc(16));
        REQUIRE(other != nullptr);
        CHECK(other->getGpuVirtualAddress() != buffer->getGpuVirtualAddress());

        const nvrhi::ShaderHandle vertexShader = make_shader(native, nvrhi::ShaderType::Vertex);
        REQUIRE(vertexShader != nullptr);
        CHECK(vertexShader->getDesc().shaderType == nvrhi::ShaderType::Vertex);
        const void *bytecode = nullptr;
        size_t bytecodeSize = 0;
        vertexShader->getBytecode(&bytecode, &bytecodeSize);
        REQUIRE(bytecodeSize == FAKE_BYTECODE.size());
        CHECK(std::memcmp(bytecode, FAKE_BYTECODE.data(), bytecodeSize) == 0);

        const nvrhi::ShaderSpecialization constant = nvrhi::ShaderSpecialization::UInt32(0, 7);
        const nvrhi::ShaderHandle specialised = native.createShaderSpecialization(vertexShader, &constant, 1);
        REQUIRE(specialised != nullptr);
        CHECK(specialised->getDesc().shaderType == nvrhi::ShaderType::Vertex);

        const nvrhi::ShaderLibraryHandle library = native.createShaderLibrary(
          FAKE_BYTECODE.data(),
          FAKE_BYTECODE.size()
        );
        REQUIRE(library != nullptr);
        const nvrhi::ShaderHandle fromLibrary = library->getShader("rgen", nvrhi::ShaderType::RayGeneration);
        REQUIRE(fromLibrary != nullptr);
        CHECK(fromLibrary->getDesc().entryName == "rgen");

        nvrhi::SamplerDesc samplerDesc = {};
        samplerDesc.maxAnisotropy = 8.0f;
        const nvrhi::SamplerHandle sampler = native.createSampler(samplerDesc);
        REQUIRE(sampler != nullptr);
        CHECK(sampler->getDesc().maxAnisotropy == 8.0f);

        nvrhi::VertexAttributeDesc attribute = {};
        attribute.name = "POSITION";
        attribute.format = nvrhi::Format::RGB32_FLOAT;
        attribute.elementStride = 12;
        const nvrhi::InputLayoutHandle inputLayout = native.createInputLayout(&attribute, 1, vertexShader);
        REQUIRE(inputLayout != nullptr);
        CHECK(inputLayout->getNumAttributes() == 1);
        REQUIRE(inputLayout->getAttributeDesc(0) != nullptr);
        CHECK(inputLayout->getAttributeDesc(0)->name == "POSITION");
        CHECK(inputLayout->getAttributeDesc(1) == nullptr);

        nvrhi::FramebufferDesc framebufferDesc = {};
        framebufferDesc.addColorAttachment(texture);
        const nvrhi::FramebufferHandle framebuffer = native.createFramebuffer(framebufferDesc);
        REQUIRE(framebuffer != nullptr);
        CHECK(framebuffer->getFramebufferInfo().width == 64);
        CHECK(framebuffer->getFramebufferInfo().height == 32);
        REQUIRE(framebuffer->getFramebufferInfo().colorFormats.size() == 1);
        CHECK(framebuffer->getFramebufferInfo().colorFormats[0] == nvrhi::Format::RGBA8_UNORM);

        nvrhi::BindingLayoutDesc layoutDesc = {};
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.registerSpace = 1;
        layoutDesc.registerSpaceIsDescriptorSet = true;
        layoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
        const nvrhi::BindingLayoutHandle layout = native.createBindingLayout(layoutDesc);
        REQUIRE(layout != nullptr);
        REQUIRE(layout->getDesc() != nullptr);
        CHECK(layout->getDesc()->registerSpace == 1);
        CHECK(layout->getBindlessDesc() == nullptr);

        nvrhi::BindlessLayoutDesc bindlessDesc = {};
        bindlessDesc.maxCapacity = 64;
        bindlessDesc.addRegisterSpace(nvrhi::BindingLayoutItem::Texture_SRV(2));
        const nvrhi::BindingLayoutHandle bindless = native.createBindlessLayout(bindlessDesc);
        REQUIRE(bindless != nullptr);
        CHECK(bindless->getDesc() == nullptr);
        REQUIRE(bindless->getBindlessDesc() != nullptr);
        CHECK(bindless->getBindlessDesc()->maxCapacity == 64);

        nvrhi::BindingSetDesc setDesc = {};
        setDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, buffer));
        setDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, texture));
        const nvrhi::BindingSetHandle bindingSet = native.createBindingSet(setDesc, layout);
        REQUIRE(bindingSet != nullptr);
        CHECK(bindingSet->getLayout() == layout);
        REQUIRE(bindingSet->getDesc() != nullptr);
        CHECK(bindingSet->getDesc()->bindings.size() == 2);

        const nvrhi::DescriptorTableHandle table = native.createDescriptorTable(bindless);
        REQUIRE(table != nullptr);
        CHECK(table->getDesc() == nullptr);
        CHECK(table->getCapacity() == 0);
        native.resizeDescriptorTable(table, 8, true);
        CHECK(table->getCapacity() == 8);
        CHECK(native.writeDescriptorTable(table, nvrhi::BindingSetItem::Texture_SRV(3, texture)));
        CHECK_FALSE(native.writeDescriptorTable(table, nvrhi::BindingSetItem::Texture_SRV(8, texture)));
        native.resizeDescriptorTable(table, 2, false);
        CHECK(table->getCapacity() == 2);
        CHECK(table->getFirstDescriptorIndexInHeap() == 0);

        nvrhi::GraphicsPipelineDesc pipelineDesc = {};
        pipelineDesc.setVertexShader(vertexShader);
        pipelineDesc.setPixelShader(make_shader(native, nvrhi::ShaderType::Pixel));
        pipelineDesc.setInputLayout(inputLayout);
        pipelineDesc.addBindingLayout(layout);
        const nvrhi::GraphicsPipelineHandle pipeline = native.createGraphicsPipeline(
          pipelineDesc,
          framebuffer->getFramebufferInfo()
        );
        REQUIRE(pipeline != nullptr);
        CHECK(pipeline->getDesc().VS == vertexShader);
        CHECK(pipeline->getFramebufferInfo() == framebuffer->getFramebufferInfo());

        nvrhi::ComputePipelineDesc computeDesc = {};
        computeDesc.setComputeShader(make_shader(native, nvrhi::ShaderType::Compute));
        const nvrhi::ComputePipelineHandle compute = native.createComputePipeline(computeDesc);
        REQUIRE(compute != nullptr);
        CHECK(compute->getDesc().CS != nullptr);

        nvrhi::MeshletPipelineDesc meshletDesc = {};
        meshletDesc.setMeshShader(make_shader(native, nvrhi::ShaderType::Mesh));
        const nvrhi::MeshletPipelineHandle meshlet = native.createMeshletPipeline(
          meshletDesc,
          framebuffer->getFramebufferInfo()
        );
        REQUIRE(meshlet != nullptr);
        CHECK(meshlet->getDesc().MS != nullptr);

        const nvrhi::EventQueryHandle eventQuery = native.createEventQuery();
        const nvrhi::TimerQueryHandle timerQuery = native.createTimerQuery();
        CHECK(eventQuery != nullptr);
        CHECK(timerQuery != nullptr);

        nvrhi::rt::AccelStructDesc accelDesc = {};
        accelDesc.setTopLevelMaxInstances(4);
        const nvrhi::rt::AccelStructHandle accel = native.createAccelStruct(accelDesc);
        REQUIRE(accel != nullptr);
        CHECK(accel->getDesc().isTopLevel);
        CHECK_FALSE(accel->isCompacted());
        CHECK(accel->getDeviceAddress() != 0);
        CHECK(native.getAccelStructMemoryRequirements(accel).size > 0);
        CHECK(native.bindAccelStructMemory(accel, heap, 0));
        nvrhi::rt::AccelStructPrebuildInfo prebuild = {};
        CHECK_FALSE(native.queryTopLevelAccelStructPrebuildInfo(accelDesc, 4, prebuild));

        const nvrhi::rt::OpacityMicromapHandle micromap = native.createOpacityMicromap(
          nvrhi::rt::OpacityMicromapDesc()
        );
        REQUIRE(micromap != nullptr);
        CHECK_FALSE(micromap->isCompacted());

        nvrhi::rt::PipelineDesc rayDesc = {};
        const nvrhi::rt::PipelineHandle rayPipeline = native.createRayTracingPipeline(rayDesc);
        REQUIRE(rayPipeline != nullptr);
        const nvrhi::rt::ShaderTableHandle shaderTable = rayPipeline->createShaderTable();
        REQUIRE(shaderTable != nullptr);
        CHECK(shaderTable->getPipeline() == rayPipeline);
        shaderTable->setRayGenerationShader("rgen");
        CHECK(shaderTable->addMissShader("miss") == 0);
        CHECK(shaderTable->addHitGroup("hit") == 0);
        CHECK(shaderTable->addHitGroup("hit2") == 1);
        CHECK(shaderTable->addCallableShader("call") == 0);
        CHECK(shaderTable->getNumEntries() == 5);
        shaderTable->clearHitShaders();
        CHECK(shaderTable->getNumEntries() == 3);
        shaderTable->clearMissShaders();
        shaderTable->clearCallableShaders();
        CHECK(shaderTable->getNumEntries() == 1);

        const nvrhi::CommandListLifetimeTrackerHandle tracker = native.createCommandListLifetimeTracker(
          nvrhi::CommandQueue::Compute
        );
        REQUIRE(tracker != nullptr);
        tracker->runGarbageCollection();

        const nvrhi::CommandListHandle commandList = native.createCommandList();
        REQUIRE(commandList != nullptr);
        CHECK(commandList->getDevice() == &native);
        CHECK(commandList->getDesc().queueType == nvrhi::CommandQueue::Graphics);
        CHECK(commandList->getInternalMemoryUsage().uploadBytes == 0);

        const null_device_stats stats = stats_of(*created);
        CHECK(stats.heapsCreated == 1);
        CHECK(stats.texturesCreated == 2);
        CHECK(stats.stagingTexturesCreated == 1);
        CHECK(stats.buffersCreated == 2);
        CHECK(stats.shadersCreated == 6);
        CHECK(stats.samplersCreated == 1);
        CHECK(stats.inputLayoutsCreated == 1);
        CHECK(stats.framebuffersCreated == 1);
        CHECK(stats.bindingLayoutsCreated == 2);
        CHECK(stats.bindingSetsCreated == 1);
        CHECK(stats.descriptorTablesCreated == 1);
        CHECK(stats.graphicsPipelinesCreated == 1);
        CHECK(stats.computePipelinesCreated == 1);
        CHECK(stats.meshletPipelinesCreated == 1);
        CHECK(stats.eventQueriesCreated == 1);
        CHECK(stats.timerQueriesCreated == 1);
        CHECK(stats.commandListsCreated == 1);
    }

    TEST_CASE("null_device - invalid descriptors return null handles and report an error") {
        log_capture logs = {};
        logs.allow_errors();
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();

        CHECK(native.createTexture(colour_texture_desc(0, 8)) == nullptr);
        CHECK(native.createStagingTexture(colour_texture_desc(8, 0), nvrhi::CpuAccessMode::Read) == nullptr);
        CHECK(native.createBuffer(vertex_buffer_desc(0)) == nullptr);
        nvrhi::BufferDesc volatileDesc = vertex_buffer_desc(64);
        volatileDesc.isVolatile = true;
        volatileDesc.maxVersions = 0;
        CHECK(native.createBuffer(volatileDesc) == nullptr);
        CHECK(native.createShader(nvrhi::ShaderDesc(), nullptr, 0) == nullptr);
        CHECK(native.createShaderLibrary(FAKE_BYTECODE.data(), 0) == nullptr);
        CHECK(native.createShaderSpecialization(nullptr, nullptr, 0) == nullptr);
        CHECK(native.createFramebuffer(nvrhi::FramebufferDesc()) == nullptr);
        CHECK(native.createBindingSet(nvrhi::BindingSetDesc(), nullptr) == nullptr);
        CHECK(native.createGraphicsPipeline(nvrhi::GraphicsPipelineDesc(), nvrhi::FramebufferInfo()) == nullptr);
        CHECK(native.createComputePipeline(nvrhi::ComputePipelineDesc()) == nullptr);
        CHECK(native.createMeshletPipeline(nvrhi::MeshletPipelineDesc(), nvrhi::FramebufferInfo()) == nullptr);
        CHECK(native.createSamplerFeedbackTexture(nullptr, nvrhi::SamplerFeedbackTextureDesc()) == nullptr);
        CHECK(native.createInputLayout(nullptr, 2, nullptr) == nullptr);

        CHECK(logs.count(core::log_level::ERR) == 14);
        CHECK(logs.contains(core::log_level::ERR, "createTexture"));
        CHECK(logs.contains(core::log_level::ERR, "createShader: the shader bytecode is empty"));
        CHECK(logs.contains(core::log_level::ERR, "createGraphicsPipeline: a vertex shader is required"));
        CHECK(stats_of(*created).texturesCreated == 0);
        CHECK(stats_of(*created).buffersCreated == 0);
        CHECK(stats_of(*created).shadersCreated == 0);
    }

    TEST_CASE("null_device - 100 execute and garbage-collect cycles return increasing fences") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        const nvrhi::CommandListHandle commandList = native.createCommandList();
        REQUIRE(commandList != nullptr);

        u64 previousFence = 0;
        for (int cycle = 0; cycle < 100; ++cycle) {
            CAPTURE(cycle);
            commandList->open();
            commandList->beginMarker("frame");
            commandList->endMarker();
            commandList->close();
            const u64 fence = native.executeCommandList(commandList);
            CHECK(fence > previousFence);
            previousFence = fence;
            native.runGarbageCollection();
        }
        CHECK(native.waitForIdle());
        CHECK_FALSE(created->is_lost());

        const null_device_stats stats = stats_of(*created);
        CHECK(stats.commandListsOpened == 100);
        CHECK(stats.commandListsClosed == 100);
        CHECK(stats.commandListsSubmitted == 100);
        CHECK(stats.garbageCollections == 100);
        CHECK(stats.waitsForIdle == 1);
        CHECK(stats.markersBegun == 100);
        CHECK(stats.markersEnded == 100);
        CHECK(stats.markerNames.size() == 100);
    }

    TEST_CASE("null_device - executing an open command list is an error and is not counted") {
        log_capture logs = {};
        logs.allow_errors();
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        const nvrhi::CommandListHandle commandList = native.createCommandList();
        commandList->open();
        native.executeCommandList(commandList);
        CHECK(stats_of(*created).commandListsSubmitted == 0);
        CHECK(logs.contains(core::log_level::ERR, "still open"));
        commandList->close();
        nvrhi::ICommandList *lists[2] = {commandList, nullptr};
        native.executeCommandLists(lists, 2);
        CHECK(stats_of(*created).commandListsSubmitted == 1);
    }

    TEST_CASE("null_device - the command list records draws, dispatches, clears, copies and markers") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        const nvrhi::TextureHandle texture = native.createTexture(colour_texture_desc(16, 16));
        const nvrhi::BufferHandle buffer = native.createBuffer(vertex_buffer_desc(64));
        const nvrhi::StagingTextureHandle staging = native.createStagingTexture(
          colour_texture_desc(16, 16),
          nvrhi::CpuAccessMode::Read
        );
        const nvrhi::CommandListHandle commandList = native.createCommandList();
        REQUIRE(texture != nullptr);
        REQUIRE(buffer != nullptr);
        REQUIRE(staging != nullptr);
        REQUIRE(commandList != nullptr);

        const std::array<u32, 4> payload = {1, 2, 3, 4};
        commandList->open();
        commandList->beginMarker("depth_prepass");
        commandList->setGraphicsState(nvrhi::GraphicsState());
        commandList->setPushConstants(payload.data(), 16);
        nvrhi::DrawArguments draw = {};
        draw.vertexCount = 3;
        draw.instanceCount = 5;
        commandList->draw(draw);
        draw.instanceCount = 2;
        commandList->drawIndexed(draw);
        commandList->drawIndirect(0, 3);
        commandList->drawIndexedIndirect(0, 2);
        commandList->drawIndexedIndirectCount(0, 0, 4);
        commandList->endMarker();
        commandList->beginMarker("light_cull");
        commandList->setComputeState(nvrhi::ComputeState());
        commandList->dispatch(8, 8, 1);
        commandList->dispatchIndirect(0);
        commandList->setMeshletState(nvrhi::MeshletState());
        commandList->dispatchMesh(1, 1, 1);
        commandList->dispatchMeshIndirect(0, 1);
        commandList->dispatchMeshIndirectCount(0, 0, 1);
        commandList->endMarker();
        commandList->clearTextureFloat(texture, nvrhi::AllSubresources, nvrhi::Color(0.0f));
        commandList->clearTextureUInt(texture, nvrhi::AllSubresources, 0);
        commandList->clearDepthStencilTexture(texture, nvrhi::AllSubresources, true, 0.0f, false, 0);
        commandList->clearBufferUInt(buffer, 0);
        commandList->writeBuffer(buffer, payload.data(), sizeof(payload), 0);
        commandList->writeTexture(texture, 0, 0, payload.data(), 16, 0);
        commandList->copyBuffer(buffer, 16, buffer, 0, 16);
        commandList->copyTexture(texture, nvrhi::TextureSlice(), texture, nvrhi::TextureSlice());
        commandList->copyTexture(staging, nvrhi::TextureSlice(), texture, nvrhi::TextureSlice());
        commandList->copyTexture(texture, nvrhi::TextureSlice(), staging, nvrhi::TextureSlice());
        commandList->resolveTexture(texture, nvrhi::AllSubresources, texture, nvrhi::AllSubresources);
        commandList->close();
        native.executeCommandList(commandList);

        const null_device_stats stats = stats_of(*created);
        CHECK(stats.draws == 2);
        CHECK(stats.instancesDrawn == 7);
        CHECK(stats.drawsIndirect == 9);
        CHECK(stats.dispatches == 1);
        CHECK(stats.dispatchesIndirect == 1);
        CHECK(stats.meshDispatches == 3);
        CHECK(stats.graphicsStateChanges == 2);
        CHECK(stats.computeStateChanges == 1);
        CHECK(stats.pushConstantWrites == 1);
        CHECK(stats.textureClears == 2);
        CHECK(stats.depthClears == 1);
        CHECK(stats.bufferClears == 1);
        CHECK(stats.bufferWrites == 1);
        CHECK(stats.bufferWriteBytes == 16);
        CHECK(stats.textureWrites == 1);
        CHECK(stats.bufferCopies == 1);
        CHECK(stats.textureCopies == 4);
        CHECK(stats.markersBegun == 2);
        CHECK(stats.markersEnded == 2);
        REQUIRE(stats.markerNames.size() == 2);
        CHECK(stats.markerNames[0] == "depth_prepass");
        CHECK(stats.markerNames[1] == "light_cull");
        CHECK(stats.commandListsSubmitted == 1);
    }

    TEST_CASE("null_device - buffer writes and copies land in CPU storage readable through mapBuffer") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        const nvrhi::BufferHandle buffer = native.createBuffer(vertex_buffer_desc(32));
        const nvrhi::CommandListHandle commandList = native.createCommandList();
        REQUIRE(buffer != nullptr);

        const std::array<u32, 4> payload = {10, 20, 30, 40};
        commandList->open();
        commandList->writeBuffer(buffer, payload.data(), sizeof(payload), 0);
        commandList->copyBuffer(buffer, 16, buffer, 0, 16);
        // Out-of-range writes are dropped, never written past the end.
        commandList->writeBuffer(buffer, payload.data(), sizeof(payload), 24);
        commandList->writeBuffer(buffer, payload.data(), sizeof(payload), 1000);
        commandList->close();
        native.executeCommandList(commandList);

        const u32 *mapped = static_cast<const u32 *>(native.mapBuffer(buffer, nvrhi::CpuAccessMode::Read));
        REQUIRE(mapped != nullptr);
        CHECK(mapped[0] == 10);
        CHECK(mapped[3] == 40);
        CHECK(mapped[4] == 10);
        CHECK(mapped[5] == 20);
        CHECK(mapped[6] == 10);
        CHECK(mapped[7] == 20);
        native.unmapBuffer(buffer);

        // A write through the mapping persists across map/unmap cycles.
        u32 *writable = static_cast<u32 *>(native.mapBuffer(buffer, nvrhi::CpuAccessMode::Write));
        REQUIRE(writable != nullptr);
        writable[0] = 99;
        native.unmapBuffer(buffer);
        CHECK(static_cast<const u32 *>(native.mapBuffer(buffer, nvrhi::CpuAccessMode::Read))[0] == 99);
        native.unmapBuffer(buffer);
    }

    TEST_CASE("null_device - staging textures map to zero-filled memory with a tight row pitch") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        const nvrhi::StagingTextureHandle staging = native.createStagingTexture(
          colour_texture_desc(7, 3),
          nvrhi::CpuAccessMode::Read
        );
        REQUIRE(staging != nullptr);

        size_t rowPitch = 0;
        const std::byte *pixels = static_cast<const std::byte *>(
          native.mapStagingTexture(staging, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch)
        );
        REQUIRE(pixels != nullptr);
        CHECK(rowPitch == 7 * 4);
        bool allZero = true;
        for (size_t index = 0; index < rowPitch * 3; ++index) {
            allZero = allZero && pixels[index] == std::byte{0};
        }
        CHECK(allZero);
        native.unmapStagingTexture(staging);

        // Mapping a texture that was never created as staging is refused.
        CHECK(
          native.mapStagingTexture(nullptr, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch) == nullptr
        );
        CHECK(native.mapBuffer(nullptr, nvrhi::CpuAccessMode::Read) == nullptr);
    }

    TEST_CASE("null_device - texture memory requirements sum the mip chain in blocks") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        nvrhi::TextureDesc desc = colour_texture_desc(8, 8);
        desc.mipLevels = 4;
        desc.arraySize = 2;
        const nvrhi::TextureHandle texture = native.createTexture(desc);
        REQUIRE(texture != nullptr);
        // (64 + 16 + 4 + 1) texels * 4 bytes * 2 slices.
        CHECK(native.getTextureMemoryRequirements(texture).size == 85 * 4 * 2);

        nvrhi::TextureDesc compressed = colour_texture_desc(6, 6);
        compressed.format = nvrhi::Format::BC7_UNORM_SRGB;
        compressed.isRenderTarget = false;
        const nvrhi::TextureHandle block = native.createTexture(compressed);
        REQUIRE(block != nullptr);
        // 6x6 texels are 2x2 blocks of 16 bytes.
        CHECK(native.getTextureMemoryRequirements(block).size == 4 * 16);
        CHECK(native.getTextureMemoryRequirements(nullptr).size == 0);

        uint32_t tiles = 7;
        native.getTextureTiling(texture, &tiles, nullptr, nullptr, nullptr, nullptr);
        CHECK(tiles == 0);
    }

    TEST_CASE("null_device - resource state tracking answers like a real backend") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        nvrhi::TextureDesc trackedDesc = colour_texture_desc(4, 4);
        trackedDesc.mipLevels = 2;
        const nvrhi::TextureHandle tracked = native.createTexture(trackedDesc);
        nvrhi::TextureDesc untrackedDesc = colour_texture_desc(4, 4);
        untrackedDesc.keepInitialState = false;
        const nvrhi::TextureHandle untracked = native.createTexture(untrackedDesc);
        const nvrhi::BufferHandle buffer = native.createBuffer(vertex_buffer_desc(16));
        const nvrhi::CommandListHandle commandList = native.createCommandList();
        REQUIRE(tracked != nullptr);
        REQUIRE(untracked != nullptr);
        REQUIRE(buffer != nullptr);

        commandList->open();
        CHECK(commandList->getTextureSubresourceState(tracked, 0, 0) == nvrhi::ResourceStates::ShaderResource);
        CHECK(commandList->getTextureSubresourceState(untracked, 0, 0) == nvrhi::ResourceStates::Unknown);
        CHECK(commandList->getTextureSubresourceState(nullptr, 0, 0) == nvrhi::ResourceStates::Unknown);
        CHECK(commandList->getBufferState(buffer) == nvrhi::ResourceStates::VertexBuffer);
        CHECK(commandList->getBufferState(nullptr) == nvrhi::ResourceStates::Unknown);

        commandList->setTextureState(
          tracked,
          nvrhi::TextureSubresourceSet(1, 1, 0, 1),
          nvrhi::ResourceStates::RenderTarget
        );
        CHECK(commandList->getTextureSubresourceState(tracked, 0, 1) == nvrhi::ResourceStates::RenderTarget);
        CHECK(commandList->getTextureSubresourceState(tracked, 0, 0) == nvrhi::ResourceStates::ShaderResource);

        commandList->beginTrackingTextureState(untracked, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        CHECK(commandList->getTextureSubresourceState(untracked, 0, 0) == nvrhi::ResourceStates::CopyDest);
        commandList->setBufferState(buffer, nvrhi::ResourceStates::CopySource);
        CHECK(commandList->getBufferState(buffer) == nvrhi::ResourceStates::CopySource);

        commandList->setPermanentTextureState(untracked, nvrhi::ResourceStates::ShaderResource);
        commandList->setTextureState(untracked, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
        CHECK(commandList->getTextureSubresourceState(untracked, 0, 0) == nvrhi::ResourceStates::ShaderResource);
        commandList->setPermanentBufferState(buffer, nvrhi::ResourceStates::IndexBuffer);
        commandList->setBufferState(buffer, nvrhi::ResourceStates::CopyDest);
        CHECK(commandList->getBufferState(buffer) == nvrhi::ResourceStates::IndexBuffer);
        commandList->commitBarriers();
        commandList->setEnableAutomaticBarriers(false);
        commandList->close();

        // Per-list tracking resets on open; permanent states survive.
        commandList->open();
        CHECK(commandList->getTextureSubresourceState(tracked, 0, 1) == nvrhi::ResourceStates::ShaderResource);
        CHECK(commandList->getTextureSubresourceState(untracked, 0, 0) == nvrhi::ResourceStates::ShaderResource);
        CHECK(commandList->getBufferState(buffer) == nvrhi::ResourceStates::IndexBuffer);
        commandList->close();
    }

    TEST_CASE("regression - review F1 a permanent state does not outlive its texture or buffer") {
        // Permanent states used to be kept per command list in maps keyed by the raw resource pointer
        // and were never erased, so a resource created at a freed resource's address inherited the dead
        // one's permanent state. Whether or not the allocator reuses the address, the fresh resource
        // must answer with its own initial state.
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        const nvrhi::CommandListHandle commandList = native.createCommandList();
        REQUIRE(commandList != nullptr);

        nvrhi::TextureHandle first = native.createTexture(colour_texture_desc(4, 4));
        nvrhi::BufferHandle firstBuffer = native.createBuffer(vertex_buffer_desc(16));
        REQUIRE(first != nullptr);
        REQUIRE(firstBuffer != nullptr);
        commandList->open();
        commandList->setPermanentTextureState(first, nvrhi::ResourceStates::RenderTarget);
        commandList->setPermanentBufferState(firstBuffer, nvrhi::ResourceStates::IndexBuffer);
        commandList->close();
        native.executeCommandList(commandList);
        CHECK(commandList->getTextureSubresourceState(first, 0, 0) == nvrhi::ResourceStates::RenderTarget);

        // Drop every reference: the handles here and the ones the command list keeps until it reopens.
        first = nullptr;
        firstBuffer = nullptr;
        commandList->open();
        commandList->close();

        const nvrhi::TextureHandle second = native.createTexture(colour_texture_desc(4, 4));
        nvrhi::BufferDesc bufferDesc = vertex_buffer_desc(16);
        bufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
        const nvrhi::BufferHandle secondBuffer = native.createBuffer(bufferDesc);
        REQUIRE(second != nullptr);
        REQUIRE(secondBuffer != nullptr);
        commandList->open();
        CHECK(commandList->getTextureSubresourceState(second, 0, 0) == nvrhi::ResourceStates::ShaderResource);
        CHECK(commandList->getBufferState(secondBuffer) == nvrhi::ResourceStates::CopyDest);
        // A fresh resource is not pinned: setTextureState / setBufferState apply to it.
        commandList->setTextureState(second, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
        commandList->setBufferState(secondBuffer, nvrhi::ResourceStates::VertexBuffer);
        CHECK(commandList->getTextureSubresourceState(second, 0, 0) == nvrhi::ResourceStates::CopySource);
        CHECK(commandList->getBufferState(secondBuffer) == nvrhi::ResourceStates::VertexBuffer);
        commandList->close();
    }

    TEST_CASE("regression - review F1 a permanent state is a property of the resource seen by every command list") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        const nvrhi::TextureHandle texture = native.createTexture(colour_texture_desc(4, 4));
        const nvrhi::BufferHandle buffer = native.createBuffer(vertex_buffer_desc(16));
        const nvrhi::CommandListHandle writer = native.createCommandList();
        const nvrhi::CommandListHandle reader = native.createCommandList();
        REQUIRE(texture != nullptr);
        REQUIRE(buffer != nullptr);

        writer->open();
        writer->setPermanentTextureState(texture, nvrhi::ResourceStates::RenderTarget);
        writer->setPermanentBufferState(buffer, nvrhi::ResourceStates::IndexBuffer);
        writer->close();

        reader->open();
        CHECK(reader->getTextureSubresourceState(texture, 0, 0) == nvrhi::ResourceStates::RenderTarget);
        CHECK(reader->getBufferState(buffer) == nvrhi::ResourceStates::IndexBuffer);
        // The other list cannot move a pinned resource either.
        reader->setTextureState(texture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        reader->setBufferState(buffer, nvrhi::ResourceStates::CopyDest);
        CHECK(reader->getTextureSubresourceState(texture, 0, 0) == nvrhi::ResourceStates::RenderTarget);
        CHECK(reader->getBufferState(buffer) == nvrhi::ResourceStates::IndexBuffer);
        reader->close();

        // A list created after the state was pinned sees it as well.
        const nvrhi::CommandListHandle late = native.createCommandList();
        late->open();
        CHECK(late->getTextureSubresourceState(texture, 0, 0) == nvrhi::ResourceStates::RenderTarget);
        CHECK(late->getBufferState(buffer) == nvrhi::ResourceStates::IndexBuffer);
        late->close();
    }

    TEST_CASE("null_device - event and timer queries complete when set and reset when asked") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        const nvrhi::EventQueryHandle event = native.createEventQuery();
        const nvrhi::TimerQueryHandle timer = native.createTimerQuery();
        const nvrhi::CommandListHandle commandList = native.createCommandList();
        REQUIRE(event != nullptr);
        REQUIRE(timer != nullptr);

        CHECK_FALSE(native.pollEventQuery(event));
        native.setEventQuery(event, nvrhi::CommandQueue::Graphics);
        CHECK(native.pollEventQuery(event));
        native.waitEventQuery(event);
        native.resetEventQuery(event);
        CHECK_FALSE(native.pollEventQuery(event));
        CHECK_FALSE(native.pollEventQuery(nullptr));

        CHECK_FALSE(native.pollTimerQuery(timer));
        commandList->open();
        commandList->beginTimerQuery(timer);
        CHECK_FALSE(native.pollTimerQuery(timer));
        commandList->endTimerQuery(timer);
        commandList->close();
        CHECK(native.pollTimerQuery(timer));
        CHECK(native.getTimerQueryTime(timer) == 0.0f);
        native.resetTimerQuery(timer);
        CHECK_FALSE(native.pollTimerQuery(timer));
        CHECK(stats_of(*created).timerQueriesBegun == 1);
    }

    TEST_CASE("null_device - framebuffers keep their attachments alive") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        nvrhi::FramebufferHandle framebuffer = nullptr;
        nvrhi::ITexture *rawTexture = nullptr;
        {
            const nvrhi::TextureHandle texture = native.createTexture(colour_texture_desc(2, 2));
            REQUIRE(texture != nullptr);
            rawTexture = texture;
            nvrhi::FramebufferDesc desc = {};
            desc.addColorAttachment(texture);
            framebuffer = native.createFramebuffer(desc);
        }
        REQUIRE(framebuffer != nullptr);
        CHECK(rawTexture->GetRefCount() == 1);
        CHECK(framebuffer->getDesc().colorAttachments[0].texture == rawTexture);
        framebuffer = nullptr;
    }

    TEST_CASE("null_device - resources are released with their last handle") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        memory_guard guard = {};
        {
            const nvrhi::TextureHandle texture = native.createTexture(colour_texture_desc(4, 4));
            const nvrhi::BufferHandle buffer = native.createBuffer(vertex_buffer_desc(64));
            const nvrhi::CommandListHandle commandList = native.createCommandList();
            REQUIRE(texture != nullptr);
            REQUIRE(buffer != nullptr);
            commandList->open();
            commandList->writeBuffer(buffer, "abcd", 4, 0);
            commandList->setTextureState(texture, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
            commandList->close();
            native.executeCommandList(commandList);
            // The command list references the texture and buffer until it is reopened or released.
            CHECK(texture->GetRefCount() == 2);
            CHECK(buffer->GetRefCount() == 2);
        }
        guard.check();
    }

    TEST_CASE("null_device - [parallel] statistics are safe to record from several command lists at once") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IDevice &native = *created->native_device();
        constexpr usize LIST_COUNT = 4;
        constexpr usize MARKERS_PER_LIST = 1000;

        std::vector<nvrhi::CommandListHandle> lists = {};
        for (usize index = 0; index < LIST_COUNT; ++index) {
            lists.emplace_back(native.createCommandList());
        }

        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc jobsDesc = {};
        jobsDesc.workerCount = environment().jobThreadCount;
        CONJURE_REQUIRE_OK(jobs.start(jobsDesc));
        jobs.parallel_for(LIST_COUNT, [&lists](usize index) {
            nvrhi::ICommandList &list = *lists[index];
            list.open();
            for (usize marker = 0; marker < MARKERS_PER_LIST; ++marker) {
                list.beginMarker("pass");
                list.draw(nvrhi::DrawArguments());
                list.endMarker();
            }
            list.close();
        });
        jobs.stop();

        const null_device_stats stats = stats_of(*created);
        CHECK(stats.markersBegun == LIST_COUNT * MARKERS_PER_LIST);
        CHECK(stats.markersEnded == LIST_COUNT * MARKERS_PER_LIST);
        CHECK(stats.draws == LIST_COUNT * MARKERS_PER_LIST);
        CHECK(stats.markerNames.size() == LIST_COUNT * MARKERS_PER_LIST);
    }
}
