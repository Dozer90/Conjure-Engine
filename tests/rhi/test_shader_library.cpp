#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"
#include "rhi_test_support.h"

#include "conjure/rhi/device.h"
#include "conjure/rhi/log_category.h"
#include "conjure/rhi/shader_library.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

// shader_library (docs/design/02_renderer.md 3.12, 4.1, 10 "test_shader_library"). The index
// conjure_shaders built is read from CONJURE_TEST_SHADER_DIR (a compile definition set by
// tests/rhi/CMakeLists.txt); every other case writes its own index into a temp_directory.

#ifndef CONJURE_TEST_SHADER_DIR
    #define CONJURE_TEST_SHADER_DIR ""
#endif

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::testing;
using namespace conjure::testing::rhi_support;

namespace {
    constexpr std::string_view BLOB_A = "DXBC-blob-a";
    constexpr std::string_view BLOB_B = "DXBC-blob-b-longer";

    std::string entry_json(
      std::string_view name,
      std::string_view entry,
      std::string_view stage,
      std::string_view defines,
      std::string_view dxil,
      std::string_view spirv
    ) {
        std::string text = "{ \"name\": \"";
        text.append(name);
        text.append("\", \"entry\": \"");
        text.append(entry);
        text.append("\", \"stage\": \"");
        text.append(stage);
        text.append("\", \"defines\": \"");
        text.append(defines);
        text.append("\", \"dxil\": \"");
        text.append(dxil);
        text.append("\", \"spirv\": \"");
        text.append(spirv);
        text.append("\", \"sourceHash\": \"abc\" }");
        return text;
    }

    std::string index_json(std::string_view entries, int version = 1) {
        std::string text = "{ \"version\": ";
        text.append(std::to_string(version));
        text.append(", \"shaders\": [");
        text.append(entries);
        text.append("] }");
        return text;
    }

    void write_or_fail(const temp_directory &directory, std::string_view name, std::string_view content) {
        CONJURE_REQUIRE_OK(directory.write_text(name, content));
    }

    std::unique_ptr<shader_library> open_or_fail(device &target, const std::filesystem::path &directory) {
        core::result<std::unique_ptr<shader_library>> opened = shader_library::open(target, directory);
        CONJURE_REQUIRE_OK(opened);
        return std::move(opened).value();
    }

    std::string_view as_text(const shader_blob &blob) {
        return std::string_view(reinterpret_cast<const char *>(blob.bytes.data()), blob.bytes.size());
    }
}

TEST_SUITE("rhi/shader_library") {

    TEST_CASE("shader_stage - to_string and parse_shader_stage round-trip, with the dxc profile aliases") {
        CHECK(to_string(shader_stage::VERTEX) == "vertex");
        CHECK(to_string(shader_stage::PIXEL) == "pixel");
        CHECK(to_string(shader_stage::COMPUTE) == "compute");
        CHECK(to_string(shader_stage::COUNT) == "vertex");
        const std::array<shader_stage, 3> stages = {shader_stage::VERTEX, shader_stage::PIXEL, shader_stage::COMPUTE};
        for (const shader_stage stage : stages) {
            CAPTURE(to_string(stage));
            CHECK(parse_shader_stage(to_string(stage)).value() == stage);
        }
        CHECK(parse_shader_stage("VS").value() == shader_stage::VERTEX);
        CHECK(parse_shader_stage(" ps ").value() == shader_stage::PIXEL);
        CHECK(parse_shader_stage("Cs").value() == shader_stage::COMPUTE);
        CONJURE_CHECK_ERROR(parse_shader_stage("geometry"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(parse_shader_stage(""), core::error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("shader_library - opens the index conjure_shaders built and serves the fullscreen triangle") {
        log_capture logs = {};
        const std::filesystem::path directory = std::filesystem::path(CONJURE_TEST_SHADER_DIR);
        REQUIRE_FALSE(directory.empty());
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<shader_library> library = open_or_fail(*created, directory);

        CHECK(library->directory() == directory);
        CHECK(library->backend() == graphics_backend::NULL_DEVICE);
        // The built index also carries the renderer's pass permutations (engine/shaders/CMakeLists.txt),
        // a list no rhi-visible constant counts, so this case only asserts the floor: the fullscreen
        // triangle pair is what this module proves. Every other case writes its own index and is exact.
        CHECK(library->entry_count() >= 2);
        CHECK(library->loaded_blob_count() == 0);
        CHECK(library->contains({"fullscreen_triangle", "vs_main", ""}));
        CHECK(library->contains({"fullscreen_triangle", "ps_main", ""}));
        CHECK_FALSE(library->contains({"fullscreen_triangle", "cs_main", ""}));

        const core::result<shader_blob> vertex = library->get({"fullscreen_triangle", "vs_main", ""});
        CONJURE_REQUIRE_OK(vertex);
        CHECK(vertex.value().stage == shader_stage::VERTEX);
        CHECK_FALSE(vertex.value().bytes.empty());
        CHECK(library->loaded_blob_count() == 1);

        const core::result<shader_blob> pixel = library->get({"fullscreen_triangle", "ps_main", ""});
        CONJURE_REQUIRE_OK(pixel);
        CHECK(pixel.value().stage == shader_stage::PIXEL);
        CHECK_FALSE(pixel.value().bytes.empty());
        CHECK(pixel.value().bytes.data() != vertex.value().bytes.data());
        CHECK(library->loaded_blob_count() == 2);

        // A compiled container starts with its magic: "DXBC" for DXIL, 0x07230203 for SPIR-V.
        const std::string_view head = as_text(vertex.value()).substr(0, 4);
        const bool isDxil = head == "DXBC";
        const bool isSpirv = vertex.value().bytes.size() >= 4 && vertex.value().bytes[0] == std::byte{0x03} &&
          vertex.value().bytes[1] == std::byte{0x02} && vertex.value().bytes[2] == std::byte{0x23} &&
          vertex.value().bytes[3] == std::byte{0x07};
        CHECK((isDxil || isSpirv));
        CHECK(logs.contains(core::log_level::INFO, "opened shader library"));
    }

    TEST_CASE("shader_library - [gpu] the D3D12 device selects the DXIL column of the built index") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        const std::filesystem::path directory = std::filesystem::path(CONJURE_TEST_SHADER_DIR);
        REQUIRE_FALSE(directory.empty());
        const std::unique_ptr<device> created = make_software_device();
        const std::unique_ptr<shader_library> library = open_or_fail(*created, directory);

        CHECK(library->backend() == graphics_backend::D3D12);
        CHECK(library->contains({"fullscreen_triangle", "vs_main", ""}));
        CHECK(library->contains({"fullscreen_triangle", "ps_main", ""}));
        const core::result<shader_blob> pixel = library->get({"fullscreen_triangle", "ps_main", ""});
        CONJURE_REQUIRE_OK(pixel);
        CHECK(pixel.value().stage == shader_stage::PIXEL);
        // A DXIL container carries the "DXBC" fourcc; SPIR-V would start with 0x07230203.
        CHECK(as_text(pixel.value()).substr(0, 4) == "DXBC");
        CHECK(logs.contains(core::log_level::INFO, "opened shader library"));
    }

    TEST_CASE("shader_library - the second get of a blob is served from the cache") {
        log_capture logs = {};
        temp_directory directory = {};
        write_or_fail(directory, "a.dxil", BLOB_A);
        write_or_fail(directory, "index.json", index_json(entry_json("a", "vs_main", "vertex", "", "a.dxil", "")));
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<shader_library> library = open_or_fail(*created, directory.path());

        const core::result<shader_blob> first = library->get({"a", "vs_main", ""});
        CONJURE_REQUIRE_OK(first);
        CHECK(as_text(first.value()) == BLOB_A);
        const core::result<shader_blob> second = library->get({"a", "vs_main", ""});
        CONJURE_REQUIRE_OK(second);
        CHECK(second.value().bytes.data() == first.value().bytes.data());
        CHECK(library->loaded_blob_count() == 1);
    }

    TEST_CASE("shader_library - unknown name, entry or defines fail with NOT_FOUND") {
        log_capture logs = {};
        temp_directory directory = {};
        write_or_fail(directory, "a.dxil", BLOB_A);
        write_or_fail(
          directory,
          "index.json",
          index_json(entry_json("a", "ps_main", "pixel", "ALPHA_MASK=1", "a.dxil", ""))
        );
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<shader_library> library = open_or_fail(*created, directory.path());

        CONJURE_CHECK_OK(library->get({"a", "ps_main", "ALPHA_MASK=1"}));
        CONJURE_CHECK_ERROR(library->get({"b", "ps_main", "ALPHA_MASK=1"}), core::error_code::NOT_FOUND);
        CONJURE_CHECK_ERROR(library->get({"a", "vs_main", "ALPHA_MASK=1"}), core::error_code::NOT_FOUND);
        CONJURE_CHECK_ERROR(library->get({"a", "ps_main", ""}), core::error_code::NOT_FOUND);
        CONJURE_CHECK_ERROR(library->get({"a", "ps_main", "ALPHA_MASK=0"}), core::error_code::NOT_FOUND);
        CONJURE_CHECK_ERROR(library->get({"a", "ps_main", "ALPHA_MASK=1 "}), core::error_code::NOT_FOUND);
        CHECK_FALSE(library->contains({"a", "ps_main", ""}));
        const core::result<shader_blob> missing = library->get({"a", "ps_main", ""});
        CHECK(missing.get_error().message().find("not in the index") != std::string::npos);
    }

    TEST_CASE("shader_library - defines are matched as the axis string the index spells") {
        log_capture logs = {};
        temp_directory directory = {};
        write_or_fail(directory, "a.dxil", BLOB_A);
        write_or_fail(directory, "b.dxil", BLOB_B);
        const std::string entries = entry_json("f", "ps_main", "pixel", "ALPHA_MASK=0|FOO=2", "a.dxil", "") + "," +
          entry_json("f", "ps_main", "pixel", "ALPHA_MASK=1|FOO=2", "b.dxil", "");
        write_or_fail(directory, "index.json", index_json(entries));
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<shader_library> library = open_or_fail(*created, directory.path());
        CHECK(library->entry_count() == 2);
        CHECK(as_text(library->get({"f", "ps_main", "ALPHA_MASK=0|FOO=2"}).value()) == BLOB_A);
        CHECK(as_text(library->get({"f", "ps_main", "ALPHA_MASK=1|FOO=2"}).value()) == BLOB_B);
        CONJURE_CHECK_ERROR(library->get({"f", "ps_main", "FOO=2|ALPHA_MASK=1"}), core::error_code::NOT_FOUND);
    }

    TEST_CASE("shader_library - a missing index is NOT_FOUND") {
        log_capture logs = {};
        temp_directory directory = {};
        const std::unique_ptr<device> created = make_null_device();
        CONJURE_CHECK_ERROR(shader_library::open(*created, directory.path()), core::error_code::NOT_FOUND);
        CONJURE_CHECK_ERROR(shader_library::open(*created, directory.file("nowhere")), core::error_code::NOT_FOUND);
    }

    TEST_CASE("shader_library - a corrupt or structurally wrong index is PARSE_FAILURE") {
        log_capture logs = {};
        temp_directory directory = {};
        const std::unique_ptr<device> created = make_null_device();

        SUBCASE("invalid JSON") {
            write_or_fail(directory, "index.json", "{ \"version\": 1, \"shaders\": [ ");
            CONJURE_CHECK_ERROR(shader_library::open(*created, directory.path()), core::error_code::PARSE_FAILURE);
        }
        SUBCASE("root is not an object") {
            write_or_fail(directory, "index.json", "[1, 2, 3]");
            CONJURE_CHECK_ERROR(shader_library::open(*created, directory.path()), core::error_code::PARSE_FAILURE);
        }
        SUBCASE("unsupported version") {
            write_or_fail(directory, "index.json", index_json("", 2));
            const core::result<std::unique_ptr<shader_library>> opened = shader_library::open(
              *created,
              directory.path()
            );
            CONJURE_CHECK_ERROR(opened, core::error_code::PARSE_FAILURE);
            CHECK(opened.get_error().message().find("unsupported version 2") != std::string::npos);
        }
        SUBCASE("missing version") {
            write_or_fail(directory, "index.json", "{ \"shaders\": [] }");
            CONJURE_CHECK_ERROR(shader_library::open(*created, directory.path()), core::error_code::PARSE_FAILURE);
        }
        SUBCASE("shaders is not an array") {
            write_or_fail(directory, "index.json", "{ \"version\": 1, \"shaders\": {} }");
            CONJURE_CHECK_ERROR(shader_library::open(*created, directory.path()), core::error_code::PARSE_FAILURE);
        }
        SUBCASE("entry without an entry point") {
            write_or_fail(directory, "index.json", index_json("{ \"name\": \"a\", \"stage\": \"vertex\" }"));
            const core::result<std::unique_ptr<shader_library>> opened = shader_library::open(
              *created,
              directory.path()
            );
            CONJURE_CHECK_ERROR(opened, core::error_code::PARSE_FAILURE);
            CHECK(opened.get_error().message().find("entry 0") != std::string::npos);
        }
        SUBCASE("entry with an unknown stage") {
            write_or_fail(
              directory,
              "index.json",
              index_json(entry_json("a", "gs_main", "geometry", "", "a.dxil", ""))
            );
            const core::result<std::unique_ptr<shader_library>> opened = shader_library::open(
              *created,
              directory.path()
            );
            CONJURE_CHECK_ERROR(opened, core::error_code::PARSE_FAILURE);
            CHECK(opened.get_error().message().find("unknown shader stage 'geometry'") != std::string::npos);
        }
        SUBCASE("duplicate entry") {
            const std::string entries = entry_json("a", "vs_main", "vertex", "", "a.dxil", "") + "," +
              entry_json("a", "vs_main", "vertex", "", "b.dxil", "");
            write_or_fail(directory, "index.json", index_json(entries));
            const core::result<std::unique_ptr<shader_library>> opened = shader_library::open(
              *created,
              directory.path()
            );
            CONJURE_CHECK_ERROR(opened, core::error_code::PARSE_FAILURE);
            CHECK(opened.get_error().message().find("duplicate entry a/vs_main") != std::string::npos);
        }
    }

    TEST_CASE("shader_library - an empty index opens with no entries") {
        log_capture logs = {};
        temp_directory directory = {};
        write_or_fail(directory, "index.json", index_json(""));
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<shader_library> library = open_or_fail(*created, directory.path());
        CHECK(library->entry_count() == 0);
        CONJURE_CHECK_ERROR(library->get({"a", "vs_main", ""}), core::error_code::NOT_FOUND);
    }

    TEST_CASE("shader_library - the null device takes whichever blob column exists") {
        log_capture logs = {};
        temp_directory directory = {};
        write_or_fail(directory, "a.dxil", BLOB_A);
        write_or_fail(directory, "b.spv", BLOB_B);
        const std::string entries = entry_json("dxilOnly", "vs_main", "vertex", "", "a.dxil", "") + "," +
          entry_json("spirvOnly", "vs_main", "vertex", "", "", "b.spv") + "," +
          entry_json("both", "vs_main", "vertex", "", "a.dxil", "b.spv") + "," +
          entry_json("neither", "vs_main", "vertex", "", "", "");
        write_or_fail(directory, "index.json", index_json(entries));
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<shader_library> library = open_or_fail(*created, directory.path());

        CHECK(library->entry_count() == 4);
        CHECK(as_text(library->get({"dxilOnly", "vs_main", ""}).value()) == BLOB_A);
        CHECK(as_text(library->get({"spirvOnly", "vs_main", ""}).value()) == BLOB_B);
        CHECK(as_text(library->get({"both", "vs_main", ""}).value()) == BLOB_A);
        CHECK_FALSE(library->contains({"neither", "vs_main", ""}));
        const core::result<shader_blob> neither = library->get({"neither", "vs_main", ""});
        CONJURE_CHECK_ERROR(neither, core::error_code::NOT_FOUND);
        CHECK(neither.get_error().message().find("has no blob for the null backend") != std::string::npos);
    }

    TEST_CASE("shader_library - a missing or empty blob file is IO_FAILURE") {
        log_capture logs = {};
        temp_directory directory = {};
        write_or_fail(directory, "empty.dxil", "");
        const std::string entries = entry_json("missing", "vs_main", "vertex", "", "missing.dxil", "") + "," +
          entry_json("empty", "vs_main", "vertex", "", "empty.dxil", "");
        write_or_fail(directory, "index.json", index_json(entries));
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<shader_library> library = open_or_fail(*created, directory.path());

        CHECK(library->contains({"missing", "vs_main", ""}));
        CONJURE_CHECK_ERROR(library->get({"missing", "vs_main", ""}), core::error_code::IO_FAILURE);
        const core::result<shader_blob> empty = library->get({"empty", "vs_main", ""});
        CONJURE_CHECK_ERROR(empty, core::error_code::IO_FAILURE);
        CHECK(empty.get_error().message().find("is empty") != std::string::npos);
        CHECK(library->loaded_blob_count() == 0);
    }

    TEST_CASE("shader_library - reload re-reads the index and drops the cached blobs") {
        log_capture logs = {};
        temp_directory directory = {};
        write_or_fail(directory, "a.dxil", BLOB_A);
        write_or_fail(directory, "index.json", index_json(entry_json("a", "vs_main", "vertex", "", "a.dxil", "")));
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<shader_library> library = open_or_fail(*created, directory.path());
        CHECK(as_text(library->get({"a", "vs_main", ""}).value()) == BLOB_A);
        CHECK(library->loaded_blob_count() == 1);

        write_or_fail(directory, "a.dxil", BLOB_B);
        write_or_fail(directory, "b.dxil", BLOB_A);
        const std::string entries = entry_json("a", "vs_main", "vertex", "", "a.dxil", "") + "," +
          entry_json("b", "ps_main", "pixel", "", "b.dxil", "");
        write_or_fail(directory, "index.json", index_json(entries));

        CONJURE_REQUIRE_OK(library->reload());
        // Exactly the two entries of the rewritten index: a reload replaces the table, never merges it.
        CHECK(library->entry_count() == 2);
        CHECK(library->loaded_blob_count() == 0);
        CHECK(as_text(library->get({"a", "vs_main", ""}).value()) == BLOB_B);
        CHECK(library->get({"b", "ps_main", ""}).value().stage == shader_stage::PIXEL);
        CHECK(library->loaded_blob_count() == 2);
        CHECK(logs.contains(core::log_level::INFO, "reloaded shader library"));
    }

    TEST_CASE("shader_library - a failed reload keeps the previous index in effect") {
        log_capture logs = {};
        temp_directory directory = {};
        write_or_fail(directory, "a.dxil", BLOB_A);
        write_or_fail(directory, "index.json", index_json(entry_json("a", "vs_main", "vertex", "", "a.dxil", "")));
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<shader_library> library = open_or_fail(*created, directory.path());
        CONJURE_REQUIRE_OK(library->get({"a", "vs_main", ""}));

        write_or_fail(directory, "index.json", "not json at all");
        CONJURE_CHECK_ERROR(library->reload(), core::error_code::PARSE_FAILURE);
        CHECK(library->entry_count() == 1);
        CHECK(library->loaded_blob_count() == 1);
        CHECK(as_text(library->get({"a", "vs_main", ""}).value()) == BLOB_A);

        CONJURE_REQUIRE_OK(directory.write_text("index.json", ""));
        std::filesystem::remove(directory.file("index.json"));
        CONJURE_CHECK_ERROR(library->reload(), core::error_code::NOT_FOUND);
        CHECK(library->entry_count() == 1);
    }

    TEST_CASE("shader_library - open and destroy leak nothing") {
        log_capture logs = {};
        temp_directory directory = {};
        write_or_fail(directory, "a.dxil", BLOB_A);
        write_or_fail(directory, "index.json", index_json(entry_json("a", "vs_main", "vertex", "", "a.dxil", "")));
        const std::unique_ptr<device> created = make_null_device();
        // Captured log lines are allocations the guard would see, so the category is muted inside it.
        const core::log_level previousLevel = LOG_RHI.level();
        LOG_RHI.set_level(core::log_level::OFF);
        memory_guard guard = {};
        {
            const std::unique_ptr<shader_library> library = open_or_fail(*created, directory.path());
            CONJURE_CHECK_OK(library->get({"a", "vs_main", ""}));
        }
        guard.check();
        LOG_RHI.set_level(previousLevel);
    }
}
