#include "conjure/renderer/renderer_error.h"

using namespace conjure;
using namespace conjure::renderer;

std::string_view conjure::renderer::renderer_error_name(renderer_error code) noexcept {
    switch (code) {
        case renderer_error::SHADER_NOT_FOUND: {
            return "SHADER_NOT_FOUND";
        }
        case renderer_error::PIPELINE_CREATION_FAILED: {
            return "PIPELINE_CREATION_FAILED";
        }
        case renderer_error::RESOURCE_CREATION_FAILED: {
            return "RESOURCE_CREATION_FAILED";
        }
        case renderer_error::DECODE_FAILED: {
            return "DECODE_FAILED";
        }
        case renderer_error::INVALID_ASSET: {
            return "INVALID_ASSET";
        }
        case renderer_error::COUNT: {
            return "";
        }
    }
    return "";
}
