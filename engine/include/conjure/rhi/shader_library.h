#pragma once

#include "conjure/rhi/backend.h"
#include "conjure/rhi/device.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>

// Compiled shader blobs (docs/design/02_renderer.md 3.12, 4.1). conjure_add_shaders() writes
// <output>/index.json listing every permutation as { name, entry, stage, defines, dxil, spirv,
// sourceHash }; shader_library opens that index, selects the blob column for the device's backend
// (DXIL on D3D12, SPIR-V on Vulkan, whichever exists on the null device) and loads blobs lazily.
//
// Threading: render thread only; a shader_blob span is valid until the next reload().

namespace conjure::rhi {

    enum class shader_stage : u8 {
        VERTEX = 0,
        PIXEL,
        COMPUTE,
        COUNT
    };

    /// <summary>
    /// The index.json spelling of a stage: "vertex", "pixel" or "compute".
    /// </summary>
    [[nodiscard]] std::string_view to_string(shader_stage stage) noexcept;

    /// <summary>
    /// Parses "vertex" / "pixel" / "compute" (also the dxc profiles "vs", "ps", "cs"), case-insensitively.
    /// Fails with core::error_code::INVALID_ARGUMENT otherwise.
    /// </summary>
    [[nodiscard]] core::result<shader_stage> parse_shader_stage(std::string_view name) noexcept;

    /// <summary>
    /// Identifies one permutation: the source file stem, the entry point and the define string in
    /// the axis order declared in CMake ("ALPHA_MASK=1", "A=1|B=0", "" for none).
    /// </summary>
    struct shader_key {
        std::string_view name = "";
        std::string_view entry = "";
        std::string_view defines = "";
    };

    struct shader_blob {
        std::span<const std::byte> bytes = {};
        shader_stage stage = shader_stage::VERTEX;
    };

    class shader_library {
        private:
            struct impl;
            struct create_tag {};

            std::unique_ptr<impl> mImpl = nullptr;

        public:
            explicit shader_library(create_tag);
            ~shader_library();

            shader_library(const shader_library &) = delete;
            shader_library &operator=(const shader_library &) = delete;

        public:
            /// <summary>
            /// Opens <shaderDirectory>/index.json and selects the blob set for the device's backend.
            /// Fails with core::error_code::NOT_FOUND when the index is missing, IO_FAILURE when it cannot
            /// be read and PARSE_FAILURE when it is not valid JSON or not a valid index (the message
            /// names the entry).
            /// </summary>
            [[nodiscard]] static core::result<std::unique_ptr<shader_library>> open(
              device &device,
              const std::filesystem::path &shaderDirectory
            );

            /// <summary>
            /// Returns the compiled blob, loading it lazily. Fails with core::error_code::NOT_FOUND when no
            /// entry matches the key or the entry has no blob for this backend, and IO_FAILURE when the
            /// blob file cannot be read or is empty. A returned blob is never empty.
            /// </summary>
            [[nodiscard]] core::result<shader_blob> get(const shader_key &key);

            /// <summary>
            /// True when an entry with a blob for this backend matches the key (no file access).
            /// </summary>
            [[nodiscard]] bool contains(const shader_key &key) const noexcept;

            /// <summary>
            /// Re-reads index.json and drops every cached blob (development hot reload). On failure the
            /// previous index stays in effect and the error is returned.
            /// </summary>
            [[nodiscard]] core::result<void> reload();

            /// <summary>
            /// Number of entries in the index, including entries without a blob for this backend.
            /// </summary>
            [[nodiscard]] usize entry_count() const noexcept;

            /// <summary>
            /// Number of blobs currently loaded in memory.
            /// </summary>
            [[nodiscard]] usize loaded_blob_count() const noexcept;

            /// <summary>
            /// The backend whose blob column was selected.
            /// </summary>
            [[nodiscard]] graphics_backend backend() const noexcept;

            /// <summary>
            /// The directory index.json was opened from.
            /// </summary>
            [[nodiscard]] const std::filesystem::path &directory() const noexcept;
    };
}
