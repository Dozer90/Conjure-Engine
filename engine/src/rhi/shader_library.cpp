#include "conjure/rhi/shader_library.h"

#include "conjure/rhi/log_category.h"

#include "conjure/core/error.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"

#include <cstddef>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::rhi;

/*===========================================================================*/
/*===========================================================================*/
// Stage names

std::string_view conjure::rhi::to_string(shader_stage stage) noexcept {
    switch (stage) {
        case shader_stage::VERTEX: {
            return "vertex";
        }
        case shader_stage::PIXEL: {
            return "pixel";
        }
        case shader_stage::COMPUTE: {
            return "compute";
        }
        case shader_stage::COUNT: {
            break;
        }
    }
    return "vertex";
}

core::result<shader_stage> conjure::rhi::parse_shader_stage(std::string_view name) noexcept {
    const std::string_view trimmed = core::str::trim(name);
    if (core::str::iequals(trimmed, "vertex") || core::str::iequals(trimmed, "vs")) {
        return shader_stage::VERTEX;
    }
    if (core::str::iequals(trimmed, "pixel") || core::str::iequals(trimmed, "ps")) {
        return shader_stage::PIXEL;
    }
    if (core::str::iequals(trimmed, "compute") || core::str::iequals(trimmed, "cs")) {
        return shader_stage::COMPUTE;
    }
    return core::make_error(core::error_code::INVALID_ARGUMENT, "unknown shader stage '{}'", name);
}


/*===========================================================================*/
/*===========================================================================*/
// impl

namespace {
    constexpr std::string_view INDEX_FILE_NAME = "index.json";
    constexpr i64 SUPPORTED_INDEX_VERSION = 1;

    struct index_entry;
    using entry_table = std::unordered_map<std::string, index_entry>;

    struct index_entry {
        std::string name = {};
        std::string entry = {};
        std::string defines = {};
        shader_stage stage = shader_stage::VERTEX;
        std::string blobPath = {};          // relative to the shader directory; empty when this backend has no blob
        std::string sourceHash = {};
        std::vector<std::byte> blob = {};   // loaded lazily by get()
    };

    /// <summary>
    /// "name|entry|defines" with a separator no permutation axis can contain ('\n'), so two keys
    /// never collide after concatenation.
    /// </summary>
    std::string make_lookup_key(std::string_view name, std::string_view entry, std::string_view defines) {
        std::string key = {};
        key.reserve(name.size() + entry.size() + defines.size() + 2);
        key.append(name);
        key.push_back('\n');
        key.append(entry);
        key.push_back('\n');
        key.append(defines);
        return key;
    }

    /// <summary>
    /// The index column holding this backend's blobs. The null device prefers DXIL and falls back
    /// to SPIR-V: it only checks that a blob exists, never its encoding.
    /// </summary>
    std::string select_blob_path(const core::json::value &entry, graphics_backend backend) {
        const std::string_view dxil = entry["dxil"].as_string();
        const std::string_view spirv = entry["spirv"].as_string();
        switch (backend) {
            case graphics_backend::D3D12: {
                return std::string(dxil);
            }
            case graphics_backend::VULKAN: {
                return std::string(spirv);
            }
            case graphics_backend::NULL_DEVICE:
            case graphics_backend::COUNT: {
                break;
            }
        }
        return std::string(dxil.empty() ? spirv : dxil);
    }

    core::result<entry_table> parse_index(
      const std::filesystem::path &indexPath,
      graphics_backend backend
    ) {
        std::error_code ignored = {};
        if (!std::filesystem::is_regular_file(indexPath, ignored)) {
            return core::make_error(
              core::error_code::NOT_FOUND,
              "shader index '{}' does not exist",
              indexPath.string()
            );
        }

        CONJURE_TRY_ASSIGN(const core::json::document document, core::json::document::parse_file(indexPath));
        const core::json::value root = document.root();
        if (!root.is_object()) {
            return core::make_error(
              core::error_code::PARSE_FAILURE,
              "shader index '{}': root is not an object",
              indexPath.string()
            );
        }
        const i64 version = root["version"].as_i64(-1);
        if (version != SUPPORTED_INDEX_VERSION) {
            return core::make_error(
              core::error_code::PARSE_FAILURE,
              "shader index '{}': unsupported version {} (expected {})",
              indexPath.string(),
              version,
              SUPPORTED_INDEX_VERSION
            );
        }
        const core::json::value shaders = root["shaders"];
        if (!shaders.is_array()) {
            return core::make_error(
              core::error_code::PARSE_FAILURE,
              "shader index '{}': 'shaders' is not an array",
              indexPath.string()
            );
        }

        entry_table entries = {};
        entries.reserve(shaders.size());
        usize position = 0;
        for (const core::json::value item : shaders) {
            if (!item.is_object() || !item["name"].is_string() || !item["entry"].is_string() ||
                !item["stage"].is_string()) {
                return core::make_error(
                  core::error_code::PARSE_FAILURE,
                  "shader index '{}': entry {} needs string fields name, entry and stage",
                  indexPath.string(),
                  position
                );
            }
            index_entry entry = {};
            entry.name = std::string(item["name"].as_string());
            entry.entry = std::string(item["entry"].as_string());
            entry.defines = std::string(item["defines"].as_string());
            entry.sourceHash = std::string(item["sourceHash"].as_string());
            entry.blobPath = select_blob_path(item, backend);
            const core::result<shader_stage> stage = parse_shader_stage(item["stage"].as_string());
            if (!stage.has_value()) {
                return core::make_error(
                  core::error_code::PARSE_FAILURE,
                  "shader index '{}': entry {} ({}/{}): {}",
                  indexPath.string(),
                  position,
                  entry.name,
                  entry.entry,
                  stage.get_error().message()
                );
            }
            entry.stage = stage.value();

            std::string key = make_lookup_key(entry.name, entry.entry, entry.defines);
            if (entries.contains(key)) {
                return core::make_error(
                  core::error_code::PARSE_FAILURE,
                  "shader index '{}': duplicate entry {}/{} [{}]",
                  indexPath.string(),
                  entry.name,
                  entry.entry,
                  entry.defines
                );
            }
            entries.emplace(std::move(key), std::move(entry));
            ++position;
        }
        return entries;
    }

    core::result<std::vector<std::byte>> read_blob(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            return core::make_error(core::error_code::IO_FAILURE, "cannot open shader blob '{}'", path.string());
        }
        std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        if (stream.bad()) {
            return core::make_error(core::error_code::IO_FAILURE, "cannot read shader blob '{}'", path.string());
        }
        if (bytes.empty()) {
            return core::make_error(core::error_code::IO_FAILURE, "shader blob '{}' is empty", path.string());
        }
        std::vector<std::byte> blob(bytes.size());
        for (usize index = 0; index < bytes.size(); ++index) {
            blob[index] = static_cast<std::byte>(bytes[index]);
        }
        return blob;
    }
}

struct shader_library::impl {
    graphics_backend backend = graphics_backend::NULL_DEVICE;
    std::filesystem::path directory = {};
    entry_table entries = {};
    usize loadedBlobs = 0;
};


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

shader_library::shader_library(create_tag) :
    mImpl(std::make_unique<impl>()) {}

shader_library::~shader_library() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<std::unique_ptr<shader_library>> shader_library::open(
  device &device,
  const std::filesystem::path &shaderDirectory
) {
    std::unique_ptr<shader_library> library = std::make_unique<shader_library>(create_tag{});
    library->mImpl->backend = device.backend();
    library->mImpl->directory = shaderDirectory;
    CONJURE_TRY_ASSIGN(library->mImpl->entries, parse_index(shaderDirectory / INDEX_FILE_NAME, device.backend()));

    CONJURE_LOG_INFO(
      LOG_RHI,
      "opened shader library '{}' with {} entries for the {} backend",
      shaderDirectory.string(),
      library->mImpl->entries.size(),
      to_string(device.backend())
    );
    return library;
}

core::result<shader_blob> shader_library::get(const shader_key &key) {
    const auto found = mImpl->entries.find(make_lookup_key(key.name, key.entry, key.defines));
    if (found == mImpl->entries.end()) {
        return core::make_error(
          core::error_code::NOT_FOUND,
          "shader {}/{} [{}] is not in the index",
          key.name,
          key.entry,
          key.defines
        );
    }
    index_entry &entry = found->second;
    if (entry.blobPath.empty()) {
        return core::make_error(
          core::error_code::NOT_FOUND,
          "shader {}/{} [{}] has no blob for the {} backend",
          key.name,
          key.entry,
          key.defines,
          to_string(mImpl->backend)
        );
    }
    if (entry.blob.empty()) {
        CONJURE_TRY_ASSIGN(entry.blob, read_blob(mImpl->directory / entry.blobPath));
        ++mImpl->loadedBlobs;
        CONJURE_LOG_TRACE(LOG_RHI, "loaded shader blob '{}' ({} bytes)", entry.blobPath, entry.blob.size());
    }
    shader_blob blob = {};
    blob.bytes = entry.blob;
    blob.stage = entry.stage;
    return blob;
}

bool shader_library::contains(const shader_key &key) const noexcept {
    const auto found = mImpl->entries.find(make_lookup_key(key.name, key.entry, key.defines));
    return found != mImpl->entries.end() && !found->second.blobPath.empty();
}

core::result<void> shader_library::reload() {
    CONJURE_TRY_ASSIGN(
      entry_table entries,
      parse_index(mImpl->directory / INDEX_FILE_NAME, mImpl->backend)
    );
    mImpl->entries = std::move(entries);
    mImpl->loadedBlobs = 0;
    CONJURE_LOG_INFO(
      LOG_RHI,
      "reloaded shader library '{}' ({} entries)",
      mImpl->directory.string(), mImpl->entries.size()
    );
    return core::result<void>::success();
}

usize shader_library::entry_count() const noexcept {
    return mImpl->entries.size();
}

usize shader_library::loaded_blob_count() const noexcept {
    return mImpl->loadedBlobs;
}

graphics_backend shader_library::backend() const noexcept {
    return mImpl->backend;
}

const std::filesystem::path &shader_library::directory() const noexcept {
    return mImpl->directory;
}
