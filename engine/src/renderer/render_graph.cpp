#include "renderer/render_graph.h"
#include "renderer/timer_ring.h"
#include "conjure/renderer/log_category.h"

#include "rhi/texture_format_mapping.h"
#include "conjure/core/assert.h"

#include <algorithm>
#include <span>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr u32 MAX_VALIDATION_ERRORS_LOGGED = 8;
}

/*===========================================================================*/
/*===========================================================================*/
// render_pass

render_pass::render_pass() {}

render_pass::~render_pass() {}

/*===========================================================================*/
/*===========================================================================*/
// render_graph

render_graph::render_graph() {}

render_graph::~render_graph() {}

void render_graph::pass_entry::clear() noexcept {
    pass = nullptr;
    reads.clear();
    writes.clear();
    colourTargets.clear();
    depthTarget = graph_attachment{};
    hasDepthTarget = false;
    sideEffect = false;
    live = false;
    invalid = false;
}

void render_graph::reset() {
    mResources.clear();
    mPassCount = 0;
    mExecutedNames.clear();
    mCompiled = false;
}

graph_resource render_graph::import_texture(nvrhi::ITexture *texture, const char *name) {
    resource_entry entry = {};
    entry.kind = resource_kind::IMPORTED_TEXTURE;
    entry.texture = texture;
    entry.name = name;
    mResources.push_back(entry);
    return static_cast<graph_resource>(mResources.size() - 1);
}

graph_resource render_graph::import_buffer(nvrhi::IBuffer *buffer, const char *name) {
    resource_entry entry = {};
    entry.kind = resource_kind::IMPORTED_BUFFER;
    entry.buffer = buffer;
    entry.name = name;
    mResources.push_back(entry);
    return static_cast<graph_resource>(mResources.size() - 1);
}

void render_graph::add_pass(render_pass &pass) {
    // Entries outlive reset() so their declaration vectors keep their capacity across frames.
    if (mPasses.size() <= mPassCount) {
        mPasses.emplace_back();
    }
    pass_entry &entry = mPasses[mPassCount];
    entry.clear();
    entry.pass = &pass;
    ++mPassCount;
}

void render_graph::report_validation(const char *passName, const char *problem) {
    CONJURE_ASSERT(false, "render_graph: pass '{}': {}", passName, problem);
    report_skipped(passName, problem);
}

void render_graph::report_skipped(const char *passName, const char *problem) {
    if (mValidationErrorsLogged < MAX_VALIDATION_ERRORS_LOGGED) {
        ++mValidationErrorsLogged;
        CONJURE_LOG_ERROR(LOG_RENDERER, "render_graph: pass '{}' skipped: {}", passName, problem);
    }
}

void render_graph::compile(transient_pool &pool, u64 frameIndex) {
    // 1. Setup: every pass declares its resources.
    for (usize index = 0; index < mPassCount; ++index) {
        pass_builder builder(*this, index);
        mPasses[index].pass->setup(builder);
    }
    const std::span<pass_entry> passes(mPasses.data(), mPassCount);

    // 2. Validation: unknown resources and reads of transient resources nobody wrote earlier.
    std::vector<bool> &written = mWritten;
    written.assign(mResources.size(), false);
    for (usize index = 0; index < mResources.size(); ++index) {
        written[index] = mResources[index].kind == resource_kind::IMPORTED_TEXTURE ||
          mResources[index].kind == resource_kind::IMPORTED_BUFFER;
    }
    for (pass_entry &entry : passes) {
        // A pass may read what it writes itself (read-modify-write), so its own writes count first.
        for (const graph_resource resource : entry.writes) {
            if (valid_resource(resource)) {
                written[resource] = true;
            }
        }
        for (const graph_resource resource : entry.reads) {
            if (!valid_resource(resource)) {
                entry.invalid = true;
                report_validation(entry.pass->name(), "reads an unknown resource");
                break;
            }
            if (!written[resource]) {
                entry.invalid = true;
                report_validation(entry.pass->name(), "reads a resource before any pass wrote it");
                break;
            }
        }
        for (const graph_resource resource : entry.writes) {
            if (!valid_resource(resource)) {
                entry.invalid = true;
                report_validation(entry.pass->name(), "writes an unknown resource");
                break;
            }
        }
        if (entry.invalid) {
            for (const graph_resource resource : entry.writes) {
                if (valid_resource(resource)) {
                    written[resource] = false;
                }
            }
        }
    }

    // 3. Culling, backwards: a pass lives when it has a side effect, writes an imported resource, or
    //    writes a resource a later live pass reads.
    std::vector<bool> &needed = mNeeded;
    needed.assign(mResources.size(), false);
    for (usize reverse = mPassCount; reverse > 0; --reverse) {
        pass_entry &entry = mPasses[reverse - 1];
        if (entry.invalid) {
            entry.live = false;
            continue;
        }
        bool live = entry.sideEffect;
        for (const graph_resource resource : entry.writes) {
            const resource_kind kind = mResources[resource].kind;
            if (kind == resource_kind::IMPORTED_TEXTURE || kind == resource_kind::IMPORTED_BUFFER || needed[resource]) {
                live = true;
            }
        }
        entry.live = live;
        if (!live) {
            continue;
        }
        for (const graph_resource resource : entry.reads) {
            needed[resource] = true;
        }
    }

    // 4. Lifetimes over live passes and allocation of the live transients (no aliasing).
    for (resource_entry &resource : mResources) {
        resource.used = false;
        resource.firstPass = 0;
        resource.lastPass = 0;
    }
    for (usize index = 0; index < mPassCount; ++index) {
        const pass_entry &entry = mPasses[index];
        if (!entry.live) {
            continue;
        }
        const auto touch = [this, index](graph_resource resource) {
            resource_entry &record = mResources[resource];
            if (!record.used) {
                record.used = true;
                record.firstPass = index;
            }
            record.lastPass = index;
        };
        for (const graph_resource resource : entry.reads) {
            touch(resource);
        }
        for (const graph_resource resource : entry.writes) {
            touch(resource);
        }
    }
    for (resource_entry &resource : mResources) {
        if (!resource.used) {
            continue;
        }
        if (resource.kind == resource_kind::TRANSIENT_TEXTURE) {
            resource.texture = pool.acquire(resource.textureDesc, frameIndex);
        } else if (resource.kind == resource_kind::TRANSIENT_BUFFER) {
            resource.buffer = pool.acquire_buffer(resource.bufferDesc, frameIndex);
        }
    }
    // A pass whose transient resource the device refused cannot run. That is a runtime failure
    // (out of memory), not a declaration error (design 6): ERROR once and skip, no assert.
    for (pass_entry &entry : passes) {
        if (!entry.live) {
            continue;
        }
        const auto missing = [this](graph_resource resource) {
            const resource_entry &record = mResources[resource];
            return record.texture == nullptr && record.buffer == nullptr;
        };
        if (std::any_of(entry.reads.begin(), entry.reads.end(), missing) ||
          std::any_of(entry.writes.begin(), entry.writes.end(), missing)) {
            entry.live = false;
            entry.invalid = true;
            report_skipped(entry.pass->name(), "a resource could not be allocated");
        }
    }
    mCompiled = true;
}

void render_graph::execute(
  nvrhi::ICommandList &commandList,
  framebuffer_cache &framebuffers,
  const gpu::view_constants &viewConstants,
  render_stats &stats,
  timer_ring *timers
) {
    CONJURE_ASSERT(mCompiled, "render_graph: execute before compile");
    mExecutedNames.clear();
    for (usize index = 0; index < mPassCount; ++index) {
        pass_entry &entry = mPasses[index];
        if (!entry.live) {
            ++stats.passesCulled;
            continue;
        }
        const char *name = entry.pass->name();
        commandList.beginMarker(name);
        if (timers != nullptr) {
            timers->begin(commandList, name);
        }
        pass_context context(*this, commandList, framebuffers, viewConstants, stats, index);
        entry.pass->execute(context);
        if (timers != nullptr) {
            timers->end(commandList);
        }
        commandList.endMarker();
        ++stats.passesExecuted;
        mExecutedNames.push_back(name);
        if (stats.passNames.size() < stats.passNames.INLINE_CAPACITY) {
            stats.passNames.push_back(name);
        }
    }
}

bool render_graph::is_pass_live(usize index) const noexcept {
    return index < mPassCount && mPasses[index].live;
}

bool render_graph::lifetime(graph_resource resource, usize &firstPass, usize &lastPass) const noexcept {
    firstPass = 0;
    lastPass = 0;
    if (!valid_resource(resource) || !mResources[resource].used) {
        return false;
    }
    firstPass = mResources[resource].firstPass;
    lastPass = mResources[resource].lastPass;
    return true;
}

bool render_graph::is_imported(graph_resource resource) const noexcept {
    if (!valid_resource(resource)) {
        return false;
    }
    const resource_kind kind = mResources[resource].kind;
    return kind == resource_kind::IMPORTED_TEXTURE || kind == resource_kind::IMPORTED_BUFFER;
}

nvrhi::ITexture *render_graph::texture(graph_resource resource) const noexcept {
    return valid_resource(resource) ? mResources[resource].texture : nullptr;
}

nvrhi::IBuffer *render_graph::buffer(graph_resource resource) const noexcept {
    return valid_resource(resource) ? mResources[resource].buffer : nullptr;
}

u32 render_graph::live_transient_count() const noexcept {
    u32 count = 0;
    for (const resource_entry &resource : mResources) {
        if (resource.used && resource.kind == resource_kind::TRANSIENT_TEXTURE && resource.texture != nullptr) {
            ++count;
        }
    }
    return count;
}

/*===========================================================================*/
/*===========================================================================*/
// pass_builder

pass_builder::pass_builder(render_graph &graph, usize passIndex) :
    mGraph(graph),
    mPassIndex(passIndex) {}

pass_builder::~pass_builder() {}

graph_resource pass_builder::create_texture(const graph_texture_desc &desc) {
    render_graph::resource_entry entry = {};
    entry.kind = render_graph::resource_kind::TRANSIENT_TEXTURE;
    entry.textureDesc = desc;
    entry.name = desc.debugName;
    mGraph.mResources.push_back(entry);
    return static_cast<graph_resource>(mGraph.mResources.size() - 1);
}

graph_resource pass_builder::create_buffer(const graph_buffer_desc &desc) {
    render_graph::resource_entry entry = {};
    entry.kind = render_graph::resource_kind::TRANSIENT_BUFFER;
    entry.bufferDesc = desc;
    entry.name = desc.debugName;
    mGraph.mResources.push_back(entry);
    return static_cast<graph_resource>(mGraph.mResources.size() - 1);
}

void pass_builder::read(graph_resource resource) {
    mGraph.mPasses[mPassIndex].reads.push_back(resource);
}

void pass_builder::write(graph_resource resource) {
    mGraph.mPasses[mPassIndex].writes.push_back(resource);
}

void pass_builder::colour_target(graph_resource resource, u32 slot, u32 arraySlice, u32 mipLevel) {
    render_graph::pass_entry &entry = mGraph.mPasses[mPassIndex];
    if (entry.colourTargets.size() <= slot) {
        entry.colourTargets.resize(static_cast<usize>(slot) + 1);
    }
    entry.colourTargets[slot].resource = resource;
    entry.colourTargets[slot].arraySlice = arraySlice;
    entry.colourTargets[slot].mipLevel = mipLevel;
    entry.writes.push_back(resource);
}

void pass_builder::depth_target(graph_resource resource, u32 arraySlice) {
    render_graph::pass_entry &entry = mGraph.mPasses[mPassIndex];
    entry.depthTarget.resource = resource;
    entry.depthTarget.arraySlice = arraySlice;
    entry.hasDepthTarget = true;
    entry.writes.push_back(resource);
}

void pass_builder::side_effect() {
    mGraph.mPasses[mPassIndex].sideEffect = true;
}

/*===========================================================================*/
/*===========================================================================*/
// pass_context

pass_context::pass_context(
  render_graph &graph,
  nvrhi::ICommandList &commandList,
  framebuffer_cache &framebuffers,
  const gpu::view_constants &viewConstants,
  render_stats &stats,
  usize passIndex
) :
    mGraph(graph),
    mCommandList(commandList),
    mFramebuffers(framebuffers),
    mViewConstants(viewConstants),
    mStats(stats),
    mPassIndex(passIndex) {}

pass_context::~pass_context() {}

nvrhi::IFramebuffer *pass_context::framebuffer() {
    if (mFramebuffer != nullptr) {
        return mFramebuffer;
    }
    const render_graph::pass_entry &entry = mGraph.mPasses[mPassIndex];
    nvrhi::FramebufferDesc desc = {};
    for (const graph_attachment &attachment : entry.colourTargets) {
        nvrhi::ITexture *texture = mGraph.texture(attachment.resource);
        if (texture == nullptr) {
            continue;
        }
        nvrhi::FramebufferAttachment colour = {};
        colour.texture = texture;
        colour.subresources = nvrhi::TextureSubresourceSet(attachment.mipLevel, 1, attachment.arraySlice, 1);
        desc.addColorAttachment(colour);
    }
    if (entry.hasDepthTarget) {
        nvrhi::ITexture *texture = mGraph.texture(entry.depthTarget.resource);
        if (texture != nullptr) {
            nvrhi::FramebufferAttachment depth = {};
            depth.texture = texture;
            depth.subresources = nvrhi::TextureSubresourceSet(0, 1, entry.depthTarget.arraySlice, 1);
            desc.setDepthAttachment(depth);
        }
    }
    if (desc.colorAttachments.empty() && !desc.depthAttachment.valid()) {
        return nullptr;
    }
    mFramebuffer = mFramebuffers.get(desc);
    return mFramebuffer;
}
