#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"

#include "vfs_test_helpers.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/vfs/native_file_system.h"

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;
using namespace conjure::testing::vfs_helpers;

namespace {
    std::vector<std::string> native_entries(const std::filesystem::path &directory) {
        std::vector<std::string> names = {};
        std::error_code ignored = {};
        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, ignored)) {
            const std::u8string name = entry.path().filename().u8string();
            names.emplace_back(reinterpret_cast<const char *>(name.data()), name.size());
        }
        return names;
    }

    std::string read_native(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }
}

TEST_SUITE("core/native_file_system") {

    TEST_CASE("native_file_system - root is stored normalised without a trailing separator") {
        const native_file_system plain(std::filesystem::path("some") / "root");
        CHECK(plain.root() == std::filesystem::path("some") / "root");

        const native_file_system trailing(std::filesystem::path("some/root/"));
        CHECK(trailing.root() == std::filesystem::path("some") / "root");

        const native_file_system dotted(std::filesystem::path("some/./other/../root"));
        CHECK(dotted.root() == std::filesystem::path("some") / "root");

        CHECK_FALSE(plain.is_read_only());
        CHECK(native_file_system("x", true).is_read_only());
    }

    TEST_CASE("native_file_system - to_native maps relative paths under the root") {
        const std::filesystem::path root = std::filesystem::path("base") / "root";
        const native_file_system fileSystem(root);

        CHECK(fileSystem.to_native("") == root);
        CHECK(fileSystem.to_native("/") == root);
        CHECK(fileSystem.to_native("a/b.txt") == root / "a" / "b.txt");
        CHECK(fileSystem.to_native("/a/b.txt") == root / "a" / "b.txt");
        CHECK(fileSystem.to_native(".\\a\\\\b.txt") == root / "a" / "b.txt");
        CHECK(fileSystem.to_native("a/../b.txt") == root / "b.txt");
    }

    TEST_CASE("native_file_system - the root directory is created by the first write") {
        testing::temp_directory directory("core_vfs_native");
        const std::filesystem::path root = directory.path() / "not" / "yet" / "there";
        native_file_system fileSystem(root);

        CHECK_FALSE(std::filesystem::exists(root));
        CHECK(code_of(fileSystem.stat("")) == error_code::NOT_FOUND);
        CHECK_FALSE(fileSystem.exists(""));

        REQUIRE(fileSystem.write_file("file.txt", to_bytes("x")));
        CHECK(std::filesystem::is_directory(root));
        CHECK(fileSystem.stat("").value().isDirectory);
    }

    TEST_CASE("native_file_system - files land where to_native says and are readable by the standard library") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());

        REQUIRE(fileSystem.write_file("dir/file.txt", to_bytes("on disk")));

        CHECK(read_native(directory.path() / "dir" / "file.txt") == "on disk");
        CHECK(fileSystem.to_native("dir/file.txt") == directory.path() / "dir" / "file.txt");
    }

    TEST_CASE("native_file_system - files written by the standard library are visible") {
        testing::temp_directory directory("core_vfs_native");
        directory.write_file("external/data.txt", "external");
        native_file_system fileSystem(directory.path());

        CHECK(fileSystem.read_text_file("external/data.txt").value() == "external");
        CHECK(enumerate_all(fileSystem, "", enumerate_flags::RECURSIVE).value().paths
              == std::vector<std::string>{"external", "external/data.txt"});
    }

    TEST_CASE("native_file_system - root escapes are rejected even when the target exists on disk") {
        testing::temp_directory directory("core_vfs_native");
        directory.write_file("outside.txt", "secret");
        native_file_system fileSystem(directory.path() / "root");
        REQUIRE(fileSystem.write_file("inside.txt", to_bytes("x")));

        CHECK(code_of(fileSystem.read_file("../outside.txt")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.read_file("/../outside.txt")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.read_file("inside.txt/../../outside.txt")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.stat("..")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.write_file("../written.txt", to_bytes("y"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.remove("../outside.txt")) == error_code::PERMISSION_DENIED);
        CHECK_FALSE(std::filesystem::exists(directory.path() / "written.txt"));
        CHECK(read_native(directory.path() / "outside.txt") == "secret");

        // An absolute path to the very same root is still an escape: only relative paths are accepted.
        const std::u8string absoluteUtf8 = std::filesystem::absolute(fileSystem.to_native("inside.txt")).u8string();
        const std::string absolute(reinterpret_cast<const char *>(absoluteUtf8.data()), absoluteUtf8.size());
        CHECK(code_of(fileSystem.read_file(absolute)) == error_code::PERMISSION_DENIED);
    }

    TEST_CASE("native_file_system - UTF-8 file names round-trip through the native file system") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        const std::string name = "d\xC3\xA9j\xC3\xA0 vu/\xE6\x97\xA5\xE6\x9C\xAC.txt";

        REQUIRE(fileSystem.write_file(name, to_bytes("unicode")));

        CHECK(fileSystem.read_text_file(name).value() == "unicode");
        CHECK(enumerate_all(fileSystem, "", enumerate_flags::RECURSIVE).value().paths
              == std::vector<std::string>{"d\xC3\xA9j\xC3\xA0 vu", name});
        CHECK(native_entries(directory.path()) == std::vector<std::string>{"d\xC3\xA9j\xC3\xA0 vu"});
        CHECK(read_native(directory.path() / std::filesystem::path(std::u8string_view(
          reinterpret_cast<const char8_t *>(name.data()),
          name.size()
        ))) == "unicode");
    }

    TEST_CASE("regression - wave4-integration: paths beyond the classic Windows limit are read and written") {
        // A project cache under a deep temporary root crossed 260 characters on Windows and every write
        // failed with "open for writing failed" (MAX_PATH); the native backend now takes the extended-length
        // form for such paths. The directories below stay under the classic directory limit (248, so the
        // recursive enumeration and the fixture's clean-up still walk them) while the file names push the
        // full path past 260 characters wherever the temporary root lives.
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        std::string deep = {};
        const usize rootLength = directory.path().native().size();
        while (rootLength + deep.size() < 200) {
            deep += "a_directory_name_that_is_long_enough_to_matter/";
        }
        const std::string file = deep + "payload_with_a_file_name_long_enough_to_pass_the_classic_limit.bin";
        CHECK(rootLength + deep.size() < 248);
        CHECK(rootLength + file.size() > 262);

        REQUIRE(fileSystem.write_file(file, to_bytes("deep")));
        CHECK(fileSystem.exists(file));
        const result<std::string> read = fileSystem.read_text_file(file);
        REQUIRE(read);
        CHECK(*read == "deep");
        REQUIRE(fileSystem.write_file_atomic(file, to_bytes("deeper")));
        const result<std::string> replaced = fileSystem.read_text_file(file);
        REQUIRE(replaced);
        CHECK(*replaced == "deeper");
        const result<file_stat> info = fileSystem.stat(file);
        REQUIRE(info);
        CHECK(info->size == 6);

        const enumerate_flags allFiles = enumerate_flags::RECURSIVE | enumerate_flags::FILES;
        const result<enumeration> files = enumerate_all(fileSystem, "", allFiles);
        REQUIRE(files);
        CHECK(files->paths.size() == 1);
        REQUIRE(fileSystem.remove(file));
        CHECK_FALSE(fileSystem.exists(file));
    }

    TEST_CASE("regression - review4: a root already in extended-length form serves nested paths") {
        // On Windows extend_native_path returned a path carrying the \\?\ prefix unchanged, so the forward
        // slashes of the VFS-relative part reached Win32, which takes the extended form literally: every stat,
        // read and write under such a root failed with not-found, however short the path. Elsewhere the prefix
        // does not exist and the case is a plain nested round trip.
        testing::temp_directory directory("core_vfs_native");
        (void) directory.write_file("nested/inner/file.txt", "payload");
        std::filesystem::path root = std::filesystem::absolute(directory.path()).lexically_normal().make_preferred();
#if CONJURE_PLATFORM_WINDOWS
        root = std::filesystem::path(L"\\\\?\\" + root.native());
#endif
        native_file_system fileSystem(root);
        CHECK(fileSystem.exists("nested/inner/file.txt"));
        const result<std::string> read = fileSystem.read_text_file("nested/inner/file.txt");
        REQUIRE(read);
        CHECK(*read == "payload");
        const result<file_stat> info = fileSystem.stat("nested/inner");
        REQUIRE(info);
        CHECK(info->isDirectory);
        REQUIRE(fileSystem.write_file("nested/inner/other.txt", to_bytes("other")));
        CHECK(read_native(directory.path() / "nested" / "inner" / "other.txt") == "other");
        REQUIRE(fileSystem.write_file_atomic("nested/inner/other.txt", to_bytes("replaced")));
        CHECK(read_native(directory.path() / "nested" / "inner" / "other.txt") == "replaced");
    }

    TEST_CASE("regression - review4: a failed write beyond the classic Windows limit names the VFS path") {
        // write_bytes derived its display path with lexically_relative(root); for a native path in extended-length
        // form the root names differ and the result was empty, so every error of the long-path branch read
        // "write '': ...". The display path is now the caller's VFS path on every platform.
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        std::string deep = {};
        const usize rootLength = directory.path().native().size();
        while (rootLength + deep.size() < 200) {
            deep += "a_directory_name_that_is_long_enough_to_matter/";
        }
        const std::string blocker = deep + "a_file_standing_where_a_directory_is_needed_and_long_enough_to_pass.bin";
        CHECK(rootLength + blocker.size() > 262);
        REQUIRE(fileSystem.write_file(blocker, to_bytes("x")));

        const std::string child = blocker + "/child.bin";
        const result<void> written = fileSystem.write_file(child, to_bytes("y"));
        REQUIRE_FALSE(written);
        const std::string &message = written.get_error().message();
        CHECK_MESSAGE(message.find(child) != std::string::npos, message);
        CHECK_MESSAGE(message.find("''") == std::string::npos, message);
        const result<void> atomic = fileSystem.write_file_atomic(child, to_bytes("y"));
        REQUIRE_FALSE(atomic);
        CHECK_MESSAGE(atomic.get_error().message().find(child) != std::string::npos, atomic.get_error().message());
        REQUIRE(fileSystem.remove(blocker));
    }

    TEST_CASE("native_file_system - write_file_atomic leaves exactly the target behind") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        REQUIRE(fileSystem.write_file("out/target.bin", to_bytes("old")));

        REQUIRE(fileSystem.write_file_atomic("out/target.bin", to_bytes("new contents")));

        CHECK(native_entries(directory.path() / "out") == std::vector<std::string>{"target.bin"});
        CHECK(read_native(directory.path() / "out" / "target.bin") == "new contents");
    }

    TEST_CASE("native_file_system - a failed atomic write removes its temporary file") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        REQUIRE(fileSystem.write_file("out/blocker", to_bytes("x")));

        // The parent of the destination is a file, so the temporary cannot even be created.
        CHECK(code_of(fileSystem.write_file_atomic("out/blocker/target.bin", to_bytes("y"))) == error_code::IO_FAILURE);
        CHECK(native_entries(directory.path() / "out") == std::vector<std::string>{"blocker"});
    }

    TEST_CASE("native_file_system - the write time changes when a file is rewritten") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        REQUIRE(fileSystem.write_file("clock.txt", to_bytes("one")));
        const u64 before = fileSystem.stat("clock.txt").value().writeTimeNanoseconds;
        REQUIRE(before != 0);

        // File systems stamp with clocks as coarse as two seconds (FAT); keep rewriting until the
        // stamp moves or a generous budget runs out, so the test is fast on NTFS and robust elsewhere.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        u64 after = before;
        while (after <= before && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            REQUIRE(fileSystem.write_file("clock.txt", to_bytes("two")));
            after = fileSystem.stat("clock.txt").value().writeTimeNanoseconds;
        }

        CHECK(after > before);
        CHECK(after - before < 60'000'000'000ull);
    }

    TEST_CASE("native_file_system - a held reader does not stop the standard library from seeing a replaced file") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        REQUIRE(fileSystem.write_file("held.txt", to_bytes("old")));
        const result<std::unique_ptr<file>> opened = fileSystem.open_read("held.txt");
        REQUIRE(opened);

        REQUIRE(fileSystem.write_file_atomic("held.txt", to_bytes("new contents")));
        CHECK(read_native(directory.path() / "held.txt") == "new contents");
        CHECK(native_entries(directory.path()) == std::vector<std::string>{"held.txt"});

        REQUIRE(fileSystem.remove("held.txt"));
        CHECK(native_entries(directory.path()).empty());

        std::array<std::byte, 3> buffer = {};
        REQUIRE((*opened)->read_at(0, buffer).value() == 3);
        CHECK(to_string(buffer) == "old");
    }

    TEST_CASE("native_file_system - read_file sees the bytes on disk even when the file changed since it was stat'ed") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        REQUIRE(fileSystem.write_file("grow.txt", to_bytes("short")));
        directory.write_file("grow.txt", "a longer replacement");

        CHECK(fileSystem.read_text_file("grow.txt").value() == "a longer replacement");
        CHECK(fileSystem.open_read("grow.txt").value()->size() == 20);
    }

    TEST_CASE("native_file_system - the write time is close to the system clock") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());

        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const u64 nowNanoseconds = static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
        REQUIRE(fileSystem.write_file("stamp.txt", to_bytes("x")));
        const u64 stamp = fileSystem.stat("stamp.txt").value().writeTimeNanoseconds;

        const u64 difference = stamp > nowNanoseconds ? stamp - nowNanoseconds : nowNanoseconds - stamp;
        CHECK(difference < 60'000'000'000ull);
    }

    TEST_CASE("native_file_system - enumerate reports entry stats and skips nothing in a mixed directory") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        REQUIRE(fileSystem.write_file("mixed/b.txt", to_bytes("bb")));
        REQUIRE(fileSystem.create_directories("mixed/a"));
        REQUIRE(fileSystem.write_file("mixed/c.txt", to_bytes("c")));

        const result<enumeration> listing = enumerate_all(fileSystem, "mixed", enumerate_flags::NONE);
        REQUIRE(listing);
        REQUIRE(listing->paths == std::vector<std::string>{"mixed/a", "mixed/b.txt", "mixed/c.txt"});
        CHECK(listing->stats[0].isDirectory);
        CHECK(listing->stats[1].size == 2);
        CHECK(listing->stats[1].writeTimeNanoseconds != 0);
        CHECK(listing->stats[2].size == 1);
    }

    TEST_CASE("native_file_system - read-only instances share the directory with writable ones") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system writable(directory.path());
        native_file_system readOnly(directory.path(), true);

        REQUIRE(writable.write_file("shared.txt", to_bytes("shared")));
        CHECK(readOnly.read_text_file("shared.txt").value() == "shared");
        CHECK(code_of(readOnly.write_file("shared.txt", to_bytes("x"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(readOnly.create_directories("dir")) == error_code::PERMISSION_DENIED);
        CHECK(readOnly.is_read_only());
        CHECK_FALSE(writable.is_read_only());
    }

    TEST_CASE("native_file_system - open_read handles see the size at open time and can be read concurrently") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        std::string contents = {};
        for (int index = 0; index < 1000; ++index) {
            contents += std::to_string(index % 10);
        }
        REQUIRE(fileSystem.write_file("big.bin", to_bytes(contents)));

        const result<std::unique_ptr<file>> opened = fileSystem.open_read("big.bin");
        REQUIRE(opened);
        file &handle = **opened;
        CHECK(handle.size() == 1000);

        std::atomic<int> failures = 0;
        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < 4; ++threadIndex) {
            threads.emplace_back([&handle, &failures, threadIndex] {
                std::vector<std::byte> buffer(7);
                for (int iteration = 0; iteration < 200; ++iteration) {
                    const u64 offset = static_cast<u64>((threadIndex * 37 + iteration * 11) % 994);
                    const result<usize> read = handle.read_at(offset, buffer);
                    if (!read.has_value() || *read != 7) {
                        ++failures;
                        continue;
                    }
                    for (usize index = 0; index < 7; ++index) {
                        const char expected = static_cast<char>('0' + static_cast<int>((offset + index) % 10));
                        if (static_cast<char>(buffer[index]) != expected) {
                            ++failures;
                        }
                    }
                }
            });
        }
        for (std::thread &thread : threads) {
            thread.join();
        }
        CHECK(failures.load() == 0);
    }

    TEST_CASE("native_file_system - one instance serves many threads at once") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());

        std::atomic<int> failures = 0;
        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < 4; ++threadIndex) {
            threads.emplace_back([&fileSystem, &failures, threadIndex] {
                for (int index = 0; index < 25; ++index) {
                    const std::string path = "t" + std::to_string(threadIndex) + "/f" + std::to_string(index) + ".txt";
                    const std::string payload = std::to_string(threadIndex * 1000 + index);
                    if (!fileSystem.write_file_atomic(path, to_bytes(payload))) {
                        ++failures;
                        continue;
                    }
                    const result<std::string> read = fileSystem.read_text_file(path);
                    if (!read.has_value() || *read != payload) {
                        ++failures;
                    }
                }
            });
        }
        for (std::thread &thread : threads) {
            thread.join();
        }

        CHECK(failures.load() == 0);
        const enumerate_flags allFiles = enumerate_flags::RECURSIVE | enumerate_flags::FILES;
        const result<enumeration> files = enumerate_all(fileSystem, "", allFiles);
        REQUIRE(files);
        CHECK(files->paths.size() == 100);
    }
}
