#pragma once

#include "conjure/core/error.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// Whole-file reads for the importers' standalone (non-pipeline) entry points: texture_asset::load_from_file
// and gltf_importer::import read source files directly instead of through the asset pipeline's VFS.

namespace conjure::renderer::detail {

    /// <summary>
    /// Reads a file into bytes. Fails with NOT_FOUND when the path is not a regular file and IO_FAILURE
    /// when it cannot be opened or read.
    /// </summary>
    [[nodiscard]] inline core::result<std::vector<std::byte>> read_file_bytes(const std::filesystem::path &path) {
        std::error_code errorCode = {};
        if (!std::filesystem::is_regular_file(path, errorCode)) {
            return core::make_error(core::error_code::NOT_FOUND, "{} not found", path.string());
        }
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            return core::make_error(core::error_code::IO_FAILURE, "cannot open {}", path.string());
        }
        const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        if (stream.bad()) {
            return core::make_error(core::error_code::IO_FAILURE, "cannot read {}", path.string());
        }
        std::vector<std::byte> bytes(text.size());
        for (usize index = 0; index < text.size(); ++index) {
            bytes[index] = static_cast<std::byte>(text[index]);
        }
        return bytes;
    }
}
