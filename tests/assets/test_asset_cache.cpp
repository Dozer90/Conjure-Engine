#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_cache.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <cstddef>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    cache_expectation expectation_of(const cooked_blob &blob) {
        cache_expectation expected = {};
        expected.type = blob.header.type;
        expected.typeVersion = blob.header.typeVersion;
        expected.importerVersion = blob.header.importerVersion;
        expected.sourceHash = blob.header.sourceHash;
        expected.settingsHash = blob.header.settingsHash;
        return expected;
    }
}

TEST_SUITE("assets/asset_cache") {

    TEST_CASE("asset_cache - paths follow <root>/<platform>/blobs/<xx>/<uuid>.cab") {
        core::vfs::memory_file_system fileSystem = {};
        const asset_cache cache(fileSystem, ".conjure/cache", "windows");
        CHECK(cache.root() == ".conjure/cache/windows");
        CHECK(cache.platform() == "windows");
        const asset_id id = test_id(1);
        const std::string text = id.to_string();
        CHECK(cache.blob_path(id) == ".conjure/cache/windows/blobs/" + text.substr(0, 2) + "/" + text + ".cab");
        CHECK(&cache.vfs() == &fileSystem);
    }

    TEST_CASE("asset_cache - write and read round trip, header read, contains and remove") {
        core::vfs::memory_file_system fileSystem = {};
        const asset_cache cache(fileSystem, "cache", "test");
        const cooked_blob blob = make_test_blob(test_id(1), "cached", {test_id(2)});
        CHECK_FALSE(cache.contains(test_id(1)));
        CONJURE_CHECK_ERROR(cache.read(test_id(1)), assets_error::CACHE_MISS);
        CONJURE_CHECK_ERROR(cache.read_header(test_id(1)), assets_error::CACHE_MISS);

        CONJURE_REQUIRE_OK(cache.write(blob));
        CHECK(cache.contains(test_id(1)));
        CHECK_FALSE(fileSystem.exists(cache.blob_path(test_id(1)) + ".tmp"));

        core::result<cooked_blob> read = cache.read(test_id(1));
        CONJURE_REQUIRE_OK(read);
        CHECK(text_of(read.value().payload) == "cached");
        CHECK(read.value().dependencies == std::vector<asset_id>{test_id(2)});
        core::result<cooked_blob_header> header = cache.read_header(test_id(1));
        CONJURE_REQUIRE_OK(header);
        CHECK(header.value().id == test_id(1));
        CHECK(header.value().payloadHash == blob.header.payloadHash);

        CONJURE_REQUIRE_OK(cache.remove(test_id(1)));
        CHECK_FALSE(cache.contains(test_id(1)));
        CONJURE_CHECK_OK(cache.remove(test_id(1)));
    }

    TEST_CASE("asset_cache - is_valid fails on each mismatching field") {
        const cooked_blob blob = make_test_blob(test_id(1), "x");
        const cache_expectation good = expectation_of(blob);
        CHECK(asset_cache::is_valid(blob.header, good));

        cache_expectation sourceHash = good;
        sourceHash.sourceHash ^= 1;
        CHECK_FALSE(asset_cache::is_valid(blob.header, sourceHash));

        cache_expectation settingsHash = good;
        settingsHash.settingsHash ^= 1;
        CHECK_FALSE(asset_cache::is_valid(blob.header, settingsHash));

        cache_expectation importerVersion = good;
        importerVersion.importerVersion += 1;
        CHECK_FALSE(asset_cache::is_valid(blob.header, importerVersion));

        cache_expectation typeVersion = good;
        typeVersion.typeVersion += 1;
        CHECK_FALSE(asset_cache::is_valid(blob.header, typeVersion));

        cache_expectation type = good;
        type.type = asset_type_traits<other_asset>::id();
        CHECK_FALSE(asset_cache::is_valid(blob.header, type));

        cooked_blob_header formatVersion = blob.header;
        formatVersion.formatVersion = 2;
        CHECK_FALSE(asset_cache::is_valid(formatVersion, good));
    }

    TEST_CASE("asset_cache - a truncated blob or a flipped payload byte is CACHE_CORRUPT") {
        core::vfs::memory_file_system fileSystem = {};
        const asset_cache cache(fileSystem, "cache", "test");
        const cooked_blob blob = make_test_blob(test_id(1), "corruptible");
        CONJURE_REQUIRE_OK(cache.write(blob));
        const std::string path = cache.blob_path(test_id(1));

        core::result<std::vector<std::byte>> bytes = fileSystem.read_file(path);
        CONJURE_REQUIRE_OK(bytes);
        std::vector<std::byte> flipped = bytes.value();
        flipped.back() = static_cast<std::byte>(static_cast<u8>(flipped.back()) ^ 0x01);
        CONJURE_REQUIRE_OK(fileSystem.write_file(path, flipped));
        CONJURE_CHECK_ERROR(cache.read(test_id(1)), assets_error::CACHE_CORRUPT);
        CONJURE_CHECK_OK(cache.read_header(test_id(1)));

        std::vector<std::byte> truncated = bytes.value();
        truncated.resize(truncated.size() - 3);
        CONJURE_REQUIRE_OK(fileSystem.write_file(path, truncated));
        CONJURE_CHECK_ERROR(cache.read(test_id(1)), assets_error::CACHE_CORRUPT);

        std::vector<std::byte> shortHeader(bytes.value().begin(), bytes.value().begin() + 20);
        CONJURE_REQUIRE_OK(fileSystem.write_file(path, shortHeader));
        CONJURE_CHECK_ERROR(cache.read_header(test_id(1)), assets_error::CACHE_CORRUPT);
    }

    TEST_CASE("asset_cache - a blob stored under another uuid's path is CACHE_CORRUPT") {
        core::vfs::memory_file_system fileSystem = {};
        const asset_cache cache(fileSystem, "cache", "test");
        const cooked_blob blob = make_test_blob(test_id(1), "mislabelled");
        CONJURE_REQUIRE_OK(fileSystem.write_file(cache.blob_path(test_id(2)), write_cooked_blob(blob)));
        CONJURE_CHECK_ERROR(cache.read(test_id(2)), assets_error::CACHE_CORRUPT);
        CONJURE_CHECK_ERROR(cache.read_header(test_id(2)), assets_error::CACHE_CORRUPT);
    }

    TEST_CASE("asset_cache - remove_temporaries deletes leftover .tmp files only") {
        core::vfs::memory_file_system fileSystem = {};
        const asset_cache cache(fileSystem, "cache", "test");
        core::result<u32> none = cache.remove_temporaries();
        CONJURE_REQUIRE_OK(none);
        CHECK(none.value() == 0);

        CONJURE_REQUIRE_OK(cache.write(make_test_blob(test_id(1), "keep")));
        CONJURE_REQUIRE_OK(fileSystem.write_file(cache.blob_path(test_id(2)) + ".tmp", bytes_of("half")));
        CONJURE_REQUIRE_OK(fileSystem.write_file("cache/test/blobs/zz/stray.tmp", bytes_of("half")));
        core::result<u32> removed = cache.remove_temporaries();
        CONJURE_REQUIRE_OK(removed);
        CHECK(removed.value() == 2);
        CHECK(cache.contains(test_id(1)));
        CHECK_FALSE(fileSystem.exists(cache.blob_path(test_id(2)) + ".tmp"));
    }

    TEST_CASE("asset_cache - clear removes every blob of the platform only") {
        core::vfs::memory_file_system fileSystem = {};
        const asset_cache windows(fileSystem, "cache", "windows");
        const asset_cache linux(fileSystem, "cache", "linux");
        CONJURE_REQUIRE_OK(windows.write(make_test_blob(test_id(1), "w")));
        CONJURE_REQUIRE_OK(linux.write(make_test_blob(test_id(1), "l")));
        CONJURE_REQUIRE_OK(windows.clear());
        CHECK_FALSE(windows.contains(test_id(1)));
        CHECK(linux.contains(test_id(1)));
        CONJURE_CHECK_OK(windows.clear());
    }

    TEST_CASE("asset_cache - a read-only file system fails writes with IO_ERROR") {
        core::vfs::memory_file_system fileSystem = {};
        fileSystem.set_read_only(true);
        const asset_cache cache(fileSystem, "cache", "test");
        CONJURE_CHECK_ERROR(cache.write(make_test_blob(test_id(1), "x")), assets_error::IO_ERROR);
    }
}
