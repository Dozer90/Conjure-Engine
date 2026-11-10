#pragma once

#include "renderer/framebuffer_cache.h"
#include "renderer/gpu_types.h"
#include "renderer/transient_pool.h"

#include "conjure/renderer/renderer.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <span>
#include <string>
#include <vector>

// The render graph (docs/design/02_renderer.md R3, 2.7): rebuilt every frame, declaration order is
// execution order, a pass whose writes nobody reads (and that writes no imported resource and has no
// side effect) is culled, transient textures come from the pool, no aliasing, NVRHI's automatic
// barriers stay on. Passes are non-owning: the renderer keeps them alive across frames and adds them
// to the graph each frame.

namespace conjure::renderer {

    using graph_resource = u16;
    inline constexpr graph_resource INVALID_GRAPH_RESOURCE = 0xFFFFu;

    class pass_builder;
    class pass_context;
    class timer_ring;

    class render_pass {
        public:
            render_pass();
            virtual ~render_pass();

            render_pass(const render_pass &) = delete;
            render_pass &operator=(const render_pass &) = delete;

        public:
            [[nodiscard]] virtual const char *name() const noexcept = 0;
            virtual void setup(pass_builder &builder) = 0;
            virtual void execute(pass_context &context) = 0;
    };

    /// <summary>
    /// A colour or depth attachment a pass declared.
    /// </summary>
    struct graph_attachment {
        graph_resource resource = INVALID_GRAPH_RESOURCE;
        u32 arraySlice = 0;
        u32 mipLevel = 0;
    };

    class render_graph {
        private:
            enum class resource_kind : u8 {
                TRANSIENT_TEXTURE,
                TRANSIENT_BUFFER,
                IMPORTED_TEXTURE,
                IMPORTED_BUFFER
            };

            struct resource_entry {
                resource_kind kind = resource_kind::TRANSIENT_TEXTURE;
                graph_texture_desc textureDesc = {};
                graph_buffer_desc bufferDesc = {};
                nvrhi::ITexture *texture = nullptr;
                nvrhi::IBuffer *buffer = nullptr;
                const char *name = "";
                usize firstPass = 0;
                usize lastPass = 0;
                bool used = false;
            };

            struct pass_entry {
                render_pass *pass = nullptr;
                std::vector<graph_resource> reads = {};
                std::vector<graph_resource> writes = {};
                std::vector<graph_attachment> colourTargets = {};
                graph_attachment depthTarget = {};
                bool hasDepthTarget = false;
                bool sideEffect = false;
                bool live = false;
                bool invalid = false;       // failed validation; skipped with an ERROR
            };

            std::vector<resource_entry> mResources = {};
            std::vector<pass_entry> mPasses = {};
            std::vector<const char *> mExecutedNames = {};
            bool mCompiled = false;
            u32 mValidationErrorsLogged = 0;

            friend class pass_builder;
            friend class pass_context;

        public:
            render_graph();
            ~render_graph();

            render_graph(const render_graph &) = delete;
            render_graph &operator=(const render_graph &) = delete;

        public:
            /// <summary>
            /// Forgets every pass and resource; capacity is kept.
            /// </summary>
            void reset();

            /// <summary>
            /// Registers an external texture (back buffer, render target, persistent shadow map).
            /// </summary>
            [[nodiscard]] graph_resource import_texture(nvrhi::ITexture *texture, const char *name);

            /// <summary>
            /// Registers an external buffer (persistent light list, exposure state, ...).
            /// </summary>
            [[nodiscard]] graph_resource import_buffer(nvrhi::IBuffer *buffer, const char *name);

            /// <summary>
            /// Appends a pass; declaration order is execution order.
            /// </summary>
            void add_pass(render_pass &pass);

            /// <summary>
            /// Runs every pass's setup, validates the declarations (a read of a transient resource
            /// nobody wrote before, or of an unknown resource, invalidates the pass: asserted in Debug,
            /// an ERROR once and the pass skipped otherwise; a pass may read what it writes itself),
            /// culls dead passes, computes lifetimes and acquires the live transient resources from
            /// the pool. A transient the device refuses skips its passes with an ERROR, never an
            /// assert (a runtime failure, not a declaration error).
            /// </summary>
            void compile(transient_pool &pool, u64 frameIndex);

            /// <summary>
            /// Executes the live passes in order, each wrapped in a debug marker (and a timer query when
            /// a ring is given), and updates the stats (passesExecuted, passesCulled, passNames).
            /// </summary>
            void execute(
              nvrhi::ICommandList &commandList,
              framebuffer_cache &framebuffers,
              const gpu::view_constants &viewConstants,
              render_stats &stats,
              timer_ring *timers
            );

            [[nodiscard]] usize pass_count() const noexcept {
                return mPasses.size();
            }

            [[nodiscard]] usize resource_count() const noexcept {
                return mResources.size();
            }

            /// <summary>
            /// After compile: whether the pass at this index survived culling and validation.
            /// </summary>
            [[nodiscard]] bool is_pass_live(usize index) const noexcept;

            /// <summary>
            /// After compile: first and last live pass index using the resource; {0, 0} and false when
            /// no live pass uses it.
            /// </summary>
            [[nodiscard]] bool lifetime(graph_resource resource, usize &firstPass, usize &lastPass) const noexcept;

            [[nodiscard]] bool is_imported(graph_resource resource) const noexcept;

            /// <summary>
            /// After compile: the texture behind a resource (nullptr for buffers, unused or unknown ones).
            /// </summary>
            [[nodiscard]] nvrhi::ITexture *texture(graph_resource resource) const noexcept;

            [[nodiscard]] nvrhi::IBuffer *buffer(graph_resource resource) const noexcept;

            /// <summary>
            /// Names of the passes the last execute ran, in order.
            /// </summary>
            [[nodiscard]] std::span<const char *const> executed_names() const noexcept {
                return mExecutedNames;
            }

            /// <summary>
            /// Number of live transient textures acquired by the last compile.
            /// </summary>
            [[nodiscard]] u32 live_transient_count() const noexcept;

        private:
            [[nodiscard]] bool valid_resource(graph_resource resource) const noexcept {
                return resource < mResources.size();
            }
            void report_validation(const char *passName, const char *problem);
            void report_skipped(const char *passName, const char *problem);
    };

    class pass_builder {
        private:
            render_graph &mGraph;       // set by the only constructor, valid during setup
            usize mPassIndex = 0;

        public:
            pass_builder(render_graph &graph, usize passIndex);
            ~pass_builder();

            pass_builder(const pass_builder &) = delete;
            pass_builder &operator=(const pass_builder &) = delete;

        public:
            [[nodiscard]] graph_resource create_texture(const graph_texture_desc &desc);
            [[nodiscard]] graph_resource create_buffer(const graph_buffer_desc &desc);
            void read(graph_resource resource);
            void write(graph_resource resource);
            void colour_target(graph_resource resource, u32 slot, u32 arraySlice = 0, u32 mipLevel = 0);
            void depth_target(graph_resource resource, u32 arraySlice = 0);
            void side_effect();
    };

    class pass_context {
        private:
            render_graph &mGraph;                   // set by the only constructor, valid during execute
            nvrhi::ICommandList &mCommandList;
            framebuffer_cache &mFramebuffers;
            const gpu::view_constants &mViewConstants;
            render_stats &mStats;
            usize mPassIndex = 0;
            nvrhi::IFramebuffer *mFramebuffer = nullptr;

        public:
            pass_context(
              render_graph &graph,
              nvrhi::ICommandList &commandList,
              framebuffer_cache &framebuffers,
              const gpu::view_constants &viewConstants,
              render_stats &stats,
              usize passIndex
            );
            ~pass_context();

            pass_context(const pass_context &) = delete;
            pass_context &operator=(const pass_context &) = delete;

        public:
            [[nodiscard]] nvrhi::ICommandList &command_list() const noexcept {
                return mCommandList;
            }

            [[nodiscard]] nvrhi::ITexture *texture(graph_resource resource) const noexcept {
                return mGraph.texture(resource);
            }

            [[nodiscard]] nvrhi::IBuffer *buffer(graph_resource resource) const noexcept {
                return mGraph.buffer(resource);
            }

            /// <summary>
            /// The framebuffer of the targets declared in setup, from the cache; nullptr without targets.
            /// </summary>
            [[nodiscard]] nvrhi::IFramebuffer *framebuffer();

            [[nodiscard]] const gpu::view_constants &view_constants() const noexcept {
                return mViewConstants;
            }

            [[nodiscard]] render_stats &stats() const noexcept {
                return mStats;
            }
    };
}
