#include "conjure/testing/doctest.h"

#include "scripting_test_support.h"

#include "conjure/scripting/native_api.h"
#include "conjure/scripting/native_api_hash.h"

#include "conjure/core/hash.h"
#include "conjure/core/json/document.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;

namespace {

    // The lines of CONJURE_NATIVE_API_FOR_EACH_RECORD_LINE in header order.
    [[nodiscard]] std::vector<std::string_view> record_lines() {
        std::vector<std::string_view> lines = {};
#define SCRIPTING_TEST_RECORD_LINE(line) lines.emplace_back(line);
        CONJURE_NATIVE_API_FOR_EACH_RECORD_LINE(SCRIPTING_TEST_RECORD_LINE)
#undef SCRIPTING_TEST_RECORD_LINE
        return lines;
    }

    // FNV-1a 64 over the entry lines of native_entry_descriptors() only, as the hash was before sweep F445.
    [[nodiscard]] u64 entry_hash() {
        u64 running = core::hash::FNV1A64_OFFSET_BASIS;
        for (const native_entry_descriptor &descriptor : native_entry_descriptors()) {
            const std::string line = std::string(descriptor.group) + "_" + std::string(descriptor.name) + " " +
              std::string(descriptor.signature) + "\n";
            running = core::hash::fnv1a64(line, running);
        }
        return running;
    }

    // The entry hash with 'lines' folded after it, one "<line>\n" each: the generator's algorithm.
    [[nodiscard]] u64 layout_hash_with(std::span<const std::string_view> lines) {
        u64 running = entry_hash();
        for (const std::string_view line : lines) {
            running = core::hash::fnv1a64(line, running);
            running = core::hash::fnv1a64("\n", running);
        }
        return running;
    }

    // Index of 'line' in 'lines' at or after 'from'; lines.size() when absent.
    [[nodiscard]] usize index_of(std::span<const std::string_view> lines, std::string_view line, usize from = 0) {
        for (usize index = from; index < lines.size(); ++index) {
            if (lines[index] == line) {
                return index;
            }
        }
        return lines.size();
    }
}

TEST_SUITE("scripting/native_api_hash") {

    TEST_CASE("native_api_hash - the generated constants describe the table") {
        CHECK(CONJURE_NATIVE_API_ENTRY_COUNT == 127);
        CHECK(CONJURE_NATIVE_API_CORE_ENTRY_COUNT == 20);
        CHECK(NATIVE_API_ENTRY_COUNT == CONJURE_NATIVE_API_ENTRY_COUNT);
        CHECK(NATIVE_API_CORE_ENTRY_COUNT == CONJURE_NATIVE_API_CORE_ENTRY_COUNT);
        CHECK(static_cast<u64>(CONJURE_NATIVE_API_HASH) != 0);
        CHECK(CONJURE_NATIVE_API_RECORD_LINE_COUNT > 0);
        // Version 2 is the first whose hash covers the records and the managed table (sweep F445); a later
        // version keeps that coverage, so only the lower bound is checked.
        CHECK(CONJURE_NATIVE_API_VERSION >= 2);
    }

    TEST_CASE("native_api_hash - the FOR_EACH_ENTRY macro expands once per entry in table order") {
        u32 count = 0;
        bool firstIsLogWrite = false;
#define SCRIPTING_TEST_COUNT_ENTRY(group, name, signature)                                                  \
        if (count == 0) {                                                                                   \
            firstIsLogWrite = std::string_view(#group "_" #name) == "log_write";                            \
        }                                                                                                   \
        ++count;
        CONJURE_NATIVE_API_FOR_EACH_ENTRY(SCRIPTING_TEST_COUNT_ENTRY)
#undef SCRIPTING_TEST_COUNT_ENTRY
        CHECK(count == CONJURE_NATIVE_API_ENTRY_COUNT);
        CHECK(firstIsLogWrite);
    }

    TEST_CASE("native_api_hash - the FOR_EACH_RECORD_LINE macro lists every record line in header order") {
        const std::vector<std::string_view> lines = record_lines();
        REQUIRE(lines.size() == CONJURE_NATIVE_API_RECORD_LINE_COUNT);
        CHECK(lines.front() == "struct conjure_vec2 {");
        CHECK(lines.back() == "};");

        SUBCASE("a record is its declaration, its members in order and a closing line") {
            const usize vec3 = index_of(lines, "struct conjure_vec3 {");
            REQUIRE(vec3 + 4 < lines.size());
            CHECK(lines[vec3 + 1] == "float x;");
            CHECK(lines[vec3 + 2] == "float y;");
            CHECK(lines[vec3 + 3] == "float z;");
            CHECK(lines[vec3 + 4] == "};");
        }
        SUBCASE("the alignment of a record is part of its declaration") {
            CHECK(index_of(lines, "struct alignas(64) conjure_frame_block {") < lines.size());
            CHECK(index_of(lines, "struct conjure_frame_block {") == lines.size());
        }
        SUBCASE("padding members are hashed in their place") {
            const usize shape = index_of(lines, "struct conjure_shape_query {");
            REQUIRE(shape + 3 < lines.size());
            CHECK(lines[shape + 2] == "uint8_t shape;");
            CHECK(lines[shape + 3] == "uint8_t pad[3];");
            const usize frame = index_of(lines, "struct alignas(64) conjure_frame_block {");
            const usize frameEnd = index_of(lines, "};", frame);
            REQUIRE(frameEnd < lines.size());
            CHECK(lines[frameEnd - 1] == "uint8_t padding[8];");
        }
        SUBCASE("an enumeration lists its enumerators in order") {
            const usize forceMode = index_of(lines, "enum class conjure_force_mode : uint8_t {");
            REQUIRE(forceMode + 5 < lines.size());
            CHECK(lines[forceMode + 1] == "FORCE,");
            CHECK(lines[forceMode + 2] == "IMPULSE,");
            CHECK(lines[forceMode + 3] == "VELOCITY_CHANGE,");
            CHECK(lines[forceMode + 4] == "ACCELERATION,");
            CHECK(lines[forceMode + 5] == "};");
        }
        SUBCASE("the native table contributes its header, not its entries") {
            const usize native = index_of(lines, "struct conjure_native_api {");
            REQUIRE(native + 5 < lines.size());
            CHECK(lines[native + 1] == "uint32_t size;");
            CHECK(lines[native + 2] == "uint32_t version;");
            CHECK(lines[native + 3] == "uint64_t layout_hash;");
            CHECK(lines[native + 4] == "const conjure_frame_block *frame;");
            CHECK(lines[native + 5] == "};");
        }
        SUBCASE("no line carries a default member initialiser") {
            for (const std::string_view line : lines) {
                CAPTURE(line);
                CHECK(line.find('=') == std::string_view::npos);
            }
        }
    }

    TEST_CASE("native_api_hash - the manifest lists the hashed records with their members") {
        const std::filesystem::path manifestPath = scripting_test::manifest_path();
        REQUIRE_FALSE(manifestPath.empty());
        const core::result<core::json::document> manifest = core::json::document::parse_file(manifestPath);
        REQUIRE_MESSAGE(manifest.has_value(), testing::describe_error(manifest));
        const core::json::value records = manifest.value().root()["records"];
        REQUIRE(records.is_array());

        // declaration, members, "};" per record, in order, reproduce the generated lines exactly.
        std::vector<std::string> fromManifest = {};
        for (usize index = 0; index < records.size(); ++index) {
            CAPTURE(index);
            const core::json::value item = records[index];
            REQUIRE(item["members"].is_array());
            CHECK(std::string_view(item["declaration"].as_string()).find(item["name"].as_string()) !=
                  std::string_view::npos);
            fromManifest.emplace_back(item["declaration"].as_string());
            const core::json::value members = item["members"];
            for (usize member = 0; member < members.size(); ++member) {
                fromManifest.emplace_back(members[member].as_string());
            }
            fromManifest.emplace_back("};");
        }
        const std::vector<std::string_view> lines = record_lines();
        REQUIRE(fromManifest.size() == lines.size());
        for (usize index = 0; index < lines.size(); ++index) {
            CAPTURE(index);
            CHECK(fromManifest[index] == lines[index]);
        }
        CHECK(records[records.size() - 1]["name"].as_string() == "conjure_managed_api");
    }

    TEST_CASE("regression - code sweep F445 the layout hash covers the ABI records and the managed callback table") {
        const std::vector<std::string_view> lines = record_lines();

        // Before F445 the hash covered the entries only, so a record or managed-table change kept it.
        CHECK(entry_hash() != static_cast<u64>(CONJURE_NATIVE_API_HASH));
        CHECK(layout_hash_with(lines) == static_cast<u64>(CONJURE_NATIVE_API_HASH));
        CHECK(compute_native_api_hash() == static_cast<u64>(CONJURE_NATIVE_API_HASH));

        const usize managed = index_of(lines, "struct conjure_managed_api {");
        REQUIRE(managed < lines.size());
        const usize runPhase = index_of(lines, "int32_t (*run_phase)(uint8_t phase, float deltaTime);", managed);
        REQUIRE(runPhase < lines.size());
        CHECK(index_of(lines, "int32_t (*shutdown)(void);", managed) < lines.size());

        SUBCASE("a managed callback whose signature changed hashes differently") {
            std::vector<std::string_view> changed = lines;
            changed[runPhase] = "int32_t (*run_phase)(uint8_t phase, double deltaTime);";
            CHECK(layout_hash_with(changed) != static_cast<u64>(CONJURE_NATIVE_API_HASH));
        }
        SUBCASE("two record members of the same size swapped hash differently") {
            const usize hit = index_of(lines, "struct conjure_raycast_hit {");
            REQUIRE(hit + 3 < lines.size());
            REQUIRE(lines[hit + 2] == "conjure_vec3 point;");
            REQUIRE(lines[hit + 3] == "conjure_vec3 normal;");
            std::vector<std::string_view> changed = lines;
            std::swap(changed[hit + 2], changed[hit + 3]);
            CHECK(layout_hash_with(changed) != static_cast<u64>(CONJURE_NATIVE_API_HASH));
        }
        SUBCASE("a padding member resized hashes differently") {
            const usize info = index_of(lines, "struct conjure_asset_info {");
            const usize pad = index_of(lines, "uint8_t pad[3];", info);
            REQUIRE(pad < lines.size());
            std::vector<std::string_view> changed = lines;
            changed[pad] = "uint8_t pad[7];";
            CHECK(layout_hash_with(changed) != static_cast<u64>(CONJURE_NATIVE_API_HASH));
        }
        SUBCASE("an enumerator reordered hashes differently") {
            const usize shape = index_of(lines, "enum class conjure_cast_shape : uint8_t {");
            REQUIRE(shape + 2 < lines.size());
            std::vector<std::string_view> changed = lines;
            std::swap(changed[shape + 1], changed[shape + 2]);
            CHECK(layout_hash_with(changed) != static_cast<u64>(CONJURE_NATIVE_API_HASH));
        }
    }
}
