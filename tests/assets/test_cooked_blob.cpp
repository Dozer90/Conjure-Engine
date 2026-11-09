#include "conjure/testing/doctest.h"
#include "assets_test_support.h"

#include "conjure/assets/cooked_blob.h"
#include "conjure/core/hash.h"

#include <cstddef>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    // Header field offsets (docs/design/05_assets.md 2.8): payloadSize at 48, dependencyCount at 56.
    constexpr usize PAYLOAD_SIZE_OFFSET = 48;
    constexpr usize DEPENDENCY_COUNT_OFFSET = 56;

    void poke_u64(std::vector<std::byte> &bytes, usize offset, u64 value) {
        for (usize index = 0; index < 8; ++index) {
            bytes[offset + index] = static_cast<std::byte>((value >> (8 * index)) & 0xFF);
        }
    }

    void poke_u32(std::vector<std::byte> &bytes, usize offset, u32 value) {
        for (usize index = 0; index < 4; ++index) {
            bytes[offset + index] = static_cast<std::byte>((value >> (8 * index)) & 0xFF);
        }
    }
}

TEST_SUITE("assets/cooked_blob") {

    TEST_CASE("regression - a dependency count or payload size that wraps the total is CACHE_CORRUPT") {
        const std::vector<std::byte> good = write_cooked_blob(make_test_blob(test_id(1), "content"));
        REQUIRE(good.size() == 87);

        // dependencyCount = 0xFFFFFFFF with a payloadSize chosen so the wrapped total equals the
        // buffer size: the old check passed and the parser tried to reserve 64 GiB of ids.
        std::vector<std::byte> wrapped = good;
        poke_u32(wrapped, DEPENDENCY_COUNT_OFFSET, 0xFFFFFFFFu);
        cooked_blob_header probe = {};
        probe.dependencyCount = 0xFFFFFFFFu;
        const u64 wrappedPayloadSize = u64{0} - probe.payload_offset() + good.size();
        poke_u64(wrapped, PAYLOAD_SIZE_OFFSET, wrappedPayloadSize);
        probe.payloadSize = wrappedPayloadSize;
        CHECK(probe.total_size() == good.size());   // the wrap the old check fell for
        CONJURE_CHECK_ERROR(parse_cooked_blob(wrapped), asset_error_code::CACHE_CORRUPT);

        // One declared dependency on a blob that carries none: the list would run into the payload.
        std::vector<std::byte> oneDependency = good;
        poke_u32(oneDependency, DEPENDENCY_COUNT_OFFSET, 1);
        CONJURE_CHECK_ERROR(parse_cooked_blob(oneDependency), asset_error_code::CACHE_CORRUPT);

        // A payload size alone that wraps past the end.
        std::vector<std::byte> hugePayload = good;
        poke_u64(hugePayload, PAYLOAD_SIZE_OFFSET, ~u64{0});
        CONJURE_CHECK_ERROR(parse_cooked_blob(hugePayload), asset_error_code::CACHE_CORRUPT);

        // The header alone still parses: these fields are only validated against the body.
        CONJURE_CHECK_OK(parse_cooked_blob_header(wrapped));
        CONJURE_CHECK_OK(parse_cooked_blob(good));
    }

    TEST_CASE("cooked_blob_header - payload offset is the header plus dependencies rounded to 16") {
        cooked_blob_header header = {};
        CHECK(cooked_blob_header::SIZE == 80);
        CHECK(header.payload_offset() == 80);
        header.dependencyCount = 1;
        CHECK(header.payload_offset() == 96);
        header.dependencyCount = 2;
        CHECK(header.payload_offset() == 112);
        header.dependencyCount = 3;
        CHECK(header.payload_offset() == 128);
        header.payloadSize = 5;
        CHECK(header.total_size() == 133);
    }

    TEST_CASE("cooked_blob - write and parse round trip keeps every field and the dependency order") {
        cooked_blob_header header = {};
        header.type = make_asset_type_id("conjure.test.asset");
        header.typeVersion = 2;
        header.importerVersion = 3;
        header.sourceHash = 0x1111222233334444ull;
        header.settingsHash = 0x5555666677778888ull;
        header.flags = 7;
        header.id = test_id(1);
        const std::vector<asset_id> dependencies = {test_id(9), test_id(2), test_id(5)};
        const cooked_blob blob = make_cooked_blob(header, dependencies, bytes_of("payload bytes"));
        CHECK(blob.header.dependencyCount == 3);
        CHECK(blob.header.payloadSize == 13);
        CHECK(blob.header.payloadHash == hash_bytes(bytes_of("payload bytes")));
        CHECK(blob.header.formatVersion == COOKED_BLOB_FORMAT_VERSION);

        const std::vector<std::byte> bytes = write_cooked_blob(blob);
        CHECK(bytes.size() == blob.header.total_size());
        CHECK(bytes.size() == 128 + 13);
        CHECK(static_cast<char>(bytes[0]) == 'C');
        CHECK(static_cast<char>(bytes[3]) == 'B');

        core::result<cooked_blob> parsed = parse_cooked_blob(bytes);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().header.type == header.type);
        CHECK(parsed.value().header.typeVersion == 2);
        CHECK(parsed.value().header.importerVersion == 3);
        CHECK(parsed.value().header.sourceHash == header.sourceHash);
        CHECK(parsed.value().header.settingsHash == header.settingsHash);
        CHECK(parsed.value().header.flags == 7);
        CHECK(parsed.value().header.id == test_id(1));
        CHECK(parsed.value().dependencies == dependencies);
        CHECK(text_of(parsed.value().payload) == "payload bytes");
        CHECK(write_cooked_blob(parsed.value()) == bytes);
    }

    TEST_CASE("cooked_blob - the payload starts 16-byte aligned") {
        for (u32 count = 0; count < 6; ++count) {
            CAPTURE(count);
            std::vector<asset_id> dependencies = {};
            for (u32 index = 0; index < count; ++index) {
                dependencies.push_back(test_id(index + 1));
            }
            const cooked_blob blob = make_cooked_blob(cooked_blob_header{}, dependencies, bytes_of("x"));
            CHECK(blob.header.payload_offset() % 16 == 0);
            const std::vector<std::byte> bytes = write_cooked_blob(blob);
            CHECK(static_cast<char>(bytes[static_cast<usize>(blob.header.payload_offset())]) == 'x');
        }
    }

    TEST_CASE("cooked_blob - an empty payload round-trips") {
        const cooked_blob blob = make_cooked_blob(cooked_blob_header{}, {}, {});
        const std::vector<std::byte> bytes = write_cooked_blob(blob);
        CHECK(bytes.size() == 80);
        core::result<cooked_blob> parsed = parse_cooked_blob(bytes);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().payload.empty());
        CHECK(parsed.value().header.payloadHash == hash_bytes({}));
    }

    TEST_CASE("parse_cooked_blob - bad magic, format version, hash and truncation are CACHE_CORRUPT") {
        const cooked_blob blob = make_test_blob(test_id(1), "content", {test_id(2)});
        const std::vector<std::byte> good = write_cooked_blob(blob);
        CONJURE_REQUIRE_OK(parse_cooked_blob(good));

        std::vector<std::byte> badMagic = good;
        badMagic[0] = std::byte{'X'};
        CONJURE_CHECK_ERROR(parse_cooked_blob(badMagic), asset_error_code::CACHE_CORRUPT);
        CONJURE_CHECK_ERROR(parse_cooked_blob_header(badMagic), asset_error_code::CACHE_CORRUPT);

        std::vector<std::byte> badVersion = good;
        badVersion[4] = std::byte{9};
        CONJURE_CHECK_ERROR(parse_cooked_blob(badVersion), asset_error_code::CACHE_CORRUPT);

        std::vector<std::byte> flipped = good;
        flipped.back() = static_cast<std::byte>(static_cast<u8>(flipped.back()) ^ 0xFF);
        CONJURE_CHECK_ERROR(parse_cooked_blob(flipped), asset_error_code::CACHE_CORRUPT);

        std::vector<std::byte> truncated = good;
        truncated.pop_back();
        CONJURE_CHECK_ERROR(parse_cooked_blob(truncated), asset_error_code::CACHE_CORRUPT);

        std::vector<std::byte> extended = good;
        extended.push_back(std::byte{0});
        CONJURE_CHECK_ERROR(parse_cooked_blob(extended), asset_error_code::CACHE_CORRUPT);

        const std::vector<std::byte> tiny(10, std::byte{0});
        CONJURE_CHECK_ERROR(parse_cooked_blob(tiny), asset_error_code::CACHE_CORRUPT);
        CONJURE_CHECK_ERROR(parse_cooked_blob_header(tiny), asset_error_code::CACHE_CORRUPT);

        // The header alone parses even when the body is missing.
        const std::vector<std::byte> headerOnly(good.begin(), good.begin() + 80);
        CONJURE_CHECK_OK(parse_cooked_blob_header(headerOnly));
        CONJURE_CHECK_ERROR(parse_cooked_blob(headerOnly), asset_error_code::CACHE_CORRUPT);
    }

    TEST_CASE("hash_bytes - agrees with core FNV-1a and differs for different content") {
        const std::vector<std::byte> a = bytes_of("alpha");
        const std::vector<std::byte> b = bytes_of("beta");
        CHECK(hash_bytes(a) == core::hash::fnv1a64(std::span<const std::byte>(a)));
        CHECK(hash_bytes(a) != hash_bytes(b));
        CHECK(hash_bytes({}) == core::hash::FNV1A64_OFFSET_BASIS);
    }

    TEST_CASE("write_cooked_blob - recomputes stale derived fields") {
        cooked_blob blob = make_test_blob(test_id(1), "content");
        blob.header.payloadHash = 0;
        blob.header.payloadSize = 99;
        blob.header.dependencyCount = 42;
        const std::vector<std::byte> bytes = write_cooked_blob(blob);
        core::result<cooked_blob> parsed = parse_cooked_blob(bytes);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().header.payloadSize == 7);
        CHECK(parsed.value().header.dependencyCount == 0);
        CHECK(parsed.value().header.payloadHash == hash_bytes(bytes_of("content")));
    }
}
