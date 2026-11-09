#include "conjure/testing/doctest.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_pack.h"
#include "conjure/core/json/document.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    // On-disk offsets (docs/design/05_assets.md 2.10): magic, three u32 and four u64 put contentHash
    // at 48 in the header; the first TOC entry starts at 64 with blobOffset at +32 and blobSize at +40.
    constexpr usize CONTENT_HASH_OFFSET = 48;
    constexpr usize FIRST_BLOB_OFFSET = 64 + 32;
    constexpr usize FIRST_BLOB_SIZE = 64 + 40;

    void poke_u64(std::vector<std::byte> &bytes, usize offset, u64 value) {
        for (usize index = 0; index < 8; ++index) {
            bytes[offset + index] = static_cast<std::byte>((value >> (8 * index)) & 0xFF);
        }
    }

    /// <summary>
    /// Re-signs a patched pack so only the table validation can reject it.
    /// </summary>
    void resign(std::vector<std::byte> &bytes) {
        const std::span<const std::byte> body(bytes.data() + pack_header::SIZE, bytes.size() - pack_header::SIZE);
        poke_u64(bytes, CONTENT_HASH_OFFSET, hash_bytes(body));
    }

    pack_writer make_writer(u32 count, bool duplicatePayloads = false) {
        pack_writer writer = {};
        // Added in reverse so sorting is observable.
        for (u32 index = count; index > 0; --index) {
            const std::string content = duplicatePayloads ? "same" : "content " + std::to_string(index);
            std::vector<asset_id> dependencies = {};
            if (index > 1) {
                dependencies.push_back(test_id(index - 1));
            }
            CONJURE_REQUIRE_OK(
              writer.add(
                make_test_blob(test_id(index), content, dependencies),
                "assets://item" + std::to_string(index) + ".ta"
              )
            );
        }
        return writer;
    }
}

TEST_SUITE("assets/asset_pack") {

    TEST_CASE("pack_writer - add verifies the payload hash and rejects duplicate uuids") {
        pack_writer writer = {};
        CONJURE_REQUIRE_OK(writer.add(make_test_blob(test_id(1), "a"), "assets://a.ta"));
        CONJURE_CHECK_ERROR(
          writer.add(make_test_blob(test_id(1), "b"), "assets://b.ta"),
          core::error_code::ALREADY_EXISTS
        );
        cooked_blob corrupt = make_test_blob(test_id(2), "c");
        corrupt.payload.push_back(std::byte{1});
        CONJURE_CHECK_ERROR(writer.add(std::move(corrupt), "assets://c.ta"), asset_error_code::CACHE_CORRUPT);
        CHECK(writer.size() == 1);
    }

    TEST_CASE("pack - writer/reader round trip with a sorted TOC, names and dependencies") {
        core::vfs::memory_file_system fileSystem = {};
        const pack_writer writer = make_writer(5);
        core::result<pack_stats> stats = writer.write(fileSystem, "out/content.cpak");
        CONJURE_REQUIRE_OK(stats);
        CHECK(stats.value().entryCount == 5);
        CHECK(stats.value().duplicatesRemoved == 0);
        CHECK_FALSE(fileSystem.exists("out/content.cpak.tmp"));

        pack_reader reader = {};
        CHECK_FALSE(reader.is_open());
        CONJURE_REQUIRE_OK(reader.open(fileSystem, "out/content.cpak"));
        CHECK(reader.is_open());
        CHECK(reader.content_hash() == stats.value().contentHash);
        CHECK(reader.header().entryCount == 5);
        REQUIRE(reader.toc().size() == 5);
        for (usize index = 1; index < reader.toc().size(); ++index) {
            CHECK(reader.toc()[index - 1].id < reader.toc()[index].id);
        }

        for (u32 index = 1; index <= 5; ++index) {
            CAPTURE(index);
            const pack_toc_entry *entry = reader.find(test_id(index));
            REQUIRE(entry != nullptr);
            CHECK(entry->type == asset_type_traits<test_asset>::id());
            CHECK(entry->blobOffset % 16 == 0);
            CHECK(reader.name_of(*entry) == "assets://item" + std::to_string(index) + ".ta");
            CHECK(reader.find_by_name("assets://item" + std::to_string(index) + ".ta") == entry);
            core::result<cooked_blob> blob = reader.read(*entry);
            CONJURE_REQUIRE_OK(blob);
            CHECK(text_of(blob.value().payload) == "content " + std::to_string(index));
            CHECK(blob.value().header.id == test_id(index));
            CHECK(blob.value().header.payloadHash == entry->payloadHash);
            if (index > 1) {
                REQUIRE(blob.value().dependencies.size() == 1);
                CHECK(blob.value().dependencies[0] == test_id(index - 1));
                CHECK(reader.dependencies_of(*entry).size() == 1);
            } else {
                CHECK(blob.value().dependencies.empty());
            }
        }
        CHECK(reader.find(test_id(42)) == nullptr);
        CHECK(reader.find_by_name("assets://nope.ta") == nullptr);
    }

    TEST_CASE("pack_writer - identical payloads are stored once") {
        core::vfs::memory_file_system fileSystem = {};
        const pack_writer writer = make_writer(4, true);
        pack_stats stats = {};
        const std::vector<std::byte> bytes = writer.build(stats);
        CHECK(stats.duplicatesRemoved == 3);
        CHECK(stats.entryCount == 4);

        pack_reader reader = {};
        CONJURE_REQUIRE_OK(reader.open_bytes(bytes));
        const u64 offset = reader.toc()[0].blobOffset;
        for (const pack_toc_entry &entry : reader.toc()) {
            CHECK(entry.blobOffset == offset);
            CHECK(text_of(reader.read(entry).value().payload) == "same");
        }
        CHECK(bytes.size() == static_cast<usize>(offset) + 4);
    }

    TEST_CASE("pack_writer - writing twice is byte-identical") {
        pack_stats first = {};
        pack_stats second = {};
        const std::vector<std::byte> a = make_writer(6).build(first);
        const std::vector<std::byte> b = make_writer(6).build(second);
        CHECK(a == b);
        CHECK(first.contentHash == second.contentHash);
        CHECK(make_writer(6).build_manifest() == make_writer(6).build_manifest());
    }

    TEST_CASE("pack_reader - corrupt header, bad content hash and truncated blob are rejected at open") {
        core::vfs::memory_file_system fileSystem = {};
        pack_stats stats = {};
        const std::vector<std::byte> good = make_writer(3).build(stats);
        pack_reader reader = {};
        CONJURE_REQUIRE_OK(reader.open_bytes(good));

        std::vector<std::byte> badMagic = good;
        badMagic[1] = std::byte{'x'};
        CONJURE_CHECK_ERROR(pack_reader().open_bytes(badMagic), asset_error_code::PACK_CORRUPT);

        std::vector<std::byte> badVersion = good;
        badVersion[4] = std::byte{9};
        CONJURE_CHECK_ERROR(pack_reader().open_bytes(badVersion), asset_error_code::PACK_CORRUPT);

        std::vector<std::byte> flipped = good;
        flipped.back() = static_cast<std::byte>(static_cast<u8>(flipped.back()) ^ 0x80);
        CONJURE_CHECK_ERROR(pack_reader().open_bytes(flipped), asset_error_code::PACK_CORRUPT);

        std::vector<std::byte> truncated = good;
        truncated.resize(truncated.size() - 2);
        CONJURE_CHECK_ERROR(pack_reader().open_bytes(truncated), asset_error_code::PACK_CORRUPT);

        const std::vector<std::byte> tiny(16, std::byte{0});
        CONJURE_CHECK_ERROR(pack_reader().open_bytes(tiny), asset_error_code::PACK_CORRUPT);

        CONJURE_REQUIRE_OK(fileSystem.write_file("out/bad.cpak", flipped));
        pack_reader fromFile = {};
        CONJURE_CHECK_ERROR(fromFile.open(fileSystem, "out/bad.cpak"), asset_error_code::PACK_CORRUPT);
        CHECK_FALSE(fromFile.is_open());
        CONJURE_CHECK_ERROR(fromFile.open(fileSystem, "out/missing.cpak"), asset_error_code::NOT_FOUND);
    }

    TEST_CASE("regression - a TOC payload range that wraps past the end of the pack is rejected at open") {
        pack_stats stats = {};
        const std::vector<std::byte> good = make_writer(1).build(stats);
        REQUIRE(good.size() > FIRST_BLOB_SIZE + 8);

        // blobOffset + blobSize wraps to a small value: the sum-form check accepted it and
        // read_range() then copied from a wild address.
        std::vector<std::byte> wrapped = good;
        poke_u64(wrapped, FIRST_BLOB_OFFSET, ~u64{0} - 15);
        poke_u64(wrapped, FIRST_BLOB_SIZE, 32);
        resign(wrapped);
        CONJURE_CHECK_ERROR(pack_reader().open_bytes(wrapped), asset_error_code::PACK_CORRUPT);

        // A size that runs past the end without wrapping is rejected the same way.
        std::vector<std::byte> tooLong = good;
        poke_u64(tooLong, FIRST_BLOB_SIZE, good.size());
        resign(tooLong);
        CONJURE_CHECK_ERROR(pack_reader().open_bytes(tooLong), asset_error_code::PACK_CORRUPT);

        // The re-signing itself is sound: the untouched pack still opens after resign().
        std::vector<std::byte> resigned = good;
        resign(resigned);
        CHECK(resigned == good);
        CONJURE_CHECK_OK(pack_reader().open_bytes(resigned));
    }

    TEST_CASE("pack_reader - an empty pack opens and finds nothing") {
        pack_stats stats = {};
        const std::vector<std::byte> bytes = pack_writer().build(stats);
        CHECK(stats.entryCount == 0);
        CHECK(bytes.size() == pack_header::SIZE);
        pack_reader reader = {};
        CONJURE_REQUIRE_OK(reader.open_bytes(bytes));
        CHECK(reader.toc().empty());
        CHECK(reader.find(test_id(1)) == nullptr);
    }

    TEST_CASE("pack_writer - the manifest lists every entry sorted by uuid with its fields") {
        const pack_writer writer = make_writer(3);
        const std::string manifest = writer.build_manifest();
        CHECK(manifest.back() == '\n');
        core::result<core::json::document> parsed = core::json::document::parse(manifest);
        CONJURE_REQUIRE_OK(parsed);
        const core::json::value root = parsed.value().root();
        REQUIRE(root.is_array());
        REQUIRE(root.size() == 3);

        std::vector<asset_id> ids = {};
        for (const core::json::value item : root) {
            CHECK(item["name"].is_string());
            CHECK(item["type"].is_string());
            CHECK(item["hash"].is_string());
            CHECK(item["size"].is_integer());
            CHECK(item["dependencies"].is_array());
            const std::optional<core::uuid> id = core::uuid::parse(item["uuid"].as_string());
            REQUIRE(id.has_value());
            ids.push_back(*id);
        }
        CHECK(ids[0] < ids[1]);
        CHECK(ids[1] < ids[2]);

        core::vfs::memory_file_system fileSystem = {};
        CONJURE_REQUIRE_OK(writer.write_manifest(fileSystem, "out/manifest.json"));
        core::result<std::string> written = fileSystem.read_text_file("out/manifest.json");
        CONJURE_REQUIRE_OK(written);
        CHECK(written.value() == manifest);
    }

    TEST_CASE("pack_reader - read on a closed reader fails") {
        const pack_reader reader = {};
        CONJURE_CHECK_ERROR(reader.read(pack_toc_entry{}), asset_error_code::PACK_CORRUPT);
    }
}
