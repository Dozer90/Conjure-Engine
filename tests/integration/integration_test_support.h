#pragma once

// Shared helpers of the integration tests: a minimal glTF generator (one textured cube, the buffer and
// the PNG embedded as data URIs, so no binary asset is needed), the editor-side asset pipeline over a
// memory file system (importers, database, cache, manager) and the shader directory conjure_shaders
// wrote (CONJURE_TEST_SHADER_DIR).

#include "conjure/testing/doctest.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace integration_test {

    /// <summary>
    /// Where conjure_shaders wrote index.json.
    /// </summary>
    [[nodiscard]] std::filesystem::path shader_directory();

    /// <summary>
    /// Standard base64 (RFC 4648, padded).
    /// </summary>
    [[nodiscard]] std::string base64_encode(std::span<const std::byte> bytes);

    /// <summary>
    /// The bytes of a string.
    /// </summary>
    [[nodiscard]] std::span<const std::byte> bytes_of(std::string_view text) noexcept;

    /// <summary>
    /// A .gltf text with one 24-vertex cube (positions, normals, uv0, 36 indices), one metallic-roughness
    /// material with a 2 x 2 base colour PNG, and two nodes: "root" at (1, 2, 3) and its child "cube" that
    /// carries the mesh. Buffer and image are data URIs.
    /// </summary>
    [[nodiscard]] std::string textured_cube_gltf();

    /// <summary>
    /// The editor-side pipeline: a memory file system with an "assets" mount, the importer registry, the
    /// database (cache under .conjure/cache) and a manager reading the cache. Jobs run inline. Types and
    /// importers are registered by the test before scan().
    /// </summary>
    class asset_pipeline {
        public:
            std::shared_ptr<conjure::core::vfs::memory_file_system> fileSystem =
              std::make_shared<conjure::core::vfs::memory_file_system>();
            conjure::core::jobs::job_system jobs = {};
            conjure::assets::importer_registry importers = {};
            std::unique_ptr<conjure::assets::asset_manager> manager = nullptr;
            std::unique_ptr<conjure::assets::asset_database> database = nullptr;

        public:
            asset_pipeline();
            ~asset_pipeline();

            asset_pipeline(const asset_pipeline &) = delete;
            asset_pipeline &operator=(const asset_pipeline &) = delete;

        public:
            /// <summary>
            /// Opens the database and points the manager at its cache. Call after every type and importer
            /// is registered.
            /// </summary>
            void open();

            /// <summary>
            /// Writes a source under assets/ and returns its asset path ("assets://<relative>").
            /// </summary>
            std::string write_source(std::string_view relativePath, std::string_view text);

            /// <summary>
            /// scan() + import_stale(); fails the case on error.
            /// </summary>
            void scan_and_import();

            /// <summary>
            /// The uuid of a source by relative path (requires a scan). Fails the case when unknown.
            /// </summary>
            [[nodiscard]] conjure::assets::asset_id id_of(std::string_view relativePath) const;
    };
}
