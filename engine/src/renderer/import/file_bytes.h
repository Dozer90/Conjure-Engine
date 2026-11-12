#pragma once

#include "conjure/core/error.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <vector>

// Whole-file reads for the importers' standalone (non-pipeline) entry points: texture_asset::load_from_file
// and gltf_importer::import read source files directly instead of through the asset pipeline's VFS.

namespace conjure::renderer::detail {

    /// <summary>
    /// The path as UTF-8 text for names and messages. On Windows path::string() converts through the
    /// ANSI code page and throws for a character outside it; u8string() never depends on the code page.
    /// </summary>
    [[nodiscard]] inline std::string path_utf8(const std::filesystem::path &path) {
        const std::u8string text = path.u8string();
        return std::string(reinterpret_cast<const char *>(text.data()), text.size());
    }

    /// <summary>
    /// Reads a file into bytes with one read of its size. Fails with NOT_FOUND when the path is not a
    /// regular file and IO_FAILURE when it cannot be opened or read completely.
    /// </summary>
    [[nodiscard]] inline core::result<std::vector<std::byte>> read_file_bytes(const std::filesystem::path &path) {
        std::error_code errorCode = {};
        if (!std::filesystem::is_regular_file(path, errorCode)) {
            return core::make_error(core::error_code::NOT_FOUND, "{} not found", path_utf8(path));
        }
        const std::uintmax_t size = std::filesystem::file_size(path, errorCode);
        if (errorCode) {
            return core::make_error(core::error_code::IO_FAILURE, "cannot size {}", path_utf8(path));
        }
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            return core::make_error(core::error_code::IO_FAILURE, "cannot open {}", path_utf8(path));
        }
        std::vector<std::byte> bytes(static_cast<usize>(size));
        if (bytes.empty()) {
            return bytes;
        }
        stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
            return core::make_error(core::error_code::IO_FAILURE, "cannot read {}", path_utf8(path));
        }
        return bytes;
    }
}
