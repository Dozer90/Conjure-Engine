#pragma once

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <string_view>

// Errors of the renderer module (docs/design/02_renderer.md 6). Device loss is reported with
// rhi::rhi_error::DEVICE_LOST; parse and lookup failures of importers use the generic core codes
// (PARSE_FAILURE, NOT_FOUND, UNSUPPORTED, INVALID_ARGUMENT). The codes below are the conditions only
// this module produces.

namespace conjure::renderer {

    enum class renderer_error : u16 {
        SHADER_NOT_FOUND = 1,           // a shader permutation the renderer needs is missing from the library
        PIPELINE_CREATION_FAILED,       // the device refused a pipeline, binding layout or input layout
        RESOURCE_CREATION_FAILED,       // the device refused a texture, buffer or sampler
        DECODE_FAILED,                  // an image or HDR file did not decode
        INVALID_ASSET,                  // a cooked blob failed validation
        INVALID_SETTINGS,               // a rendering settings value (project.json "rendering.defaults") is invalid
        COUNT
    };

    /// <summary>
    /// The enumerator name of a renderer error code ("SHADER_NOT_FOUND"), or "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view renderer_error_name(renderer_error code) noexcept;
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::renderer::renderer_error, "renderer", conjure::renderer::renderer_error_name);
