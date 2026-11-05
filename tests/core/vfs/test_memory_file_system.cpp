#include "conjure/testing/doctest.h"

#include "vfs_test_helpers.h"

#include "conjure/core/vfs/memory_file_system.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;
using namespace conjure::testing::vfs_helpers;

TEST_SUITE("core/memory_file_system") {

    TEST_CASE("memory_file_system - a new instance is an empty writable root") {
        memory_file_system fileSystem = {};

        CHECK_FALSE(fileSystem.is_read_only());
        CHECK(fileSystem.write_counter() == 0);
        const result<file_stat> root = fileSystem.stat("");
        REQUIRE(root);
        CHECK(root->isDirectory);
        CHECK(root->writeTimeNanoseconds == 0);
        CHECK(enumerate_all(fileSystem, "", enumerate_flags::RECURSIVE).value().paths.empty());
    }

    TEST_CASE("memory_file_system - write times come from a counter that increases with every mutation") {
        memory_file_system fileSystem = {};

        REQUIRE(fileSystem.write_file("a.txt", to_bytes("a")));
        CHECK(fileSystem.write_counter() == 1);
        CHECK(fileSystem.stat("a.txt").value().writeTimeNanoseconds == 1);
        CHECK(fileSystem.stat("").value().writeTimeNanoseconds == 1);

        REQUIRE(fileSystem.write_file("a.txt", to_bytes("aa")));
        CHECK(fileSystem.write_counter() == 2);
        CHECK(fileSystem.stat("a.txt").value().writeTimeNanoseconds == 2);
        CHECK(fileSystem.stat("").value().writeTimeNanoseconds == 1);

        // "dir" (tick 3) and "dir/sub" (tick 4, which also stamps "dir") are created before the
        // file itself (tick 5, which stamps "dir/sub").
        REQUIRE(fileSystem.write_file("dir/sub/b.txt", to_bytes("b")));
        CHECK(fileSystem.write_counter() == 5);
        CHECK(fileSystem.stat("").value().writeTimeNanoseconds == 3);
        CHECK(fileSystem.stat("dir").value().writeTimeNanoseconds == 4);
        CHECK(fileSystem.stat("dir/sub").value().writeTimeNanoseconds == 5);
        CHECK(fileSystem.stat("dir/sub/b.txt").value().writeTimeNanoseconds == 5);

        REQUIRE(fileSystem.rename("dir/sub/b.txt", "dir/c.txt"));
        CHECK(fileSystem.write_counter() == 6);
        CHECK(fileSystem.stat("dir").value().writeTimeNanoseconds == 6);
        CHECK(fileSystem.stat("dir/sub").value().writeTimeNanoseconds == 6);

        REQUIRE(fileSystem.remove("dir/sub"));
        CHECK(fileSystem.write_counter() == 7);
        CHECK(fileSystem.stat("dir").value().writeTimeNanoseconds == 7);

        REQUIRE(fileSystem.create_directories("dir"));
        CHECK(fileSystem.write_counter() == 7);
    }

    TEST_CASE("memory_file_system - failed mutations do not advance the counter") {
        memory_file_system fileSystem = {};
        REQUIRE(fileSystem.write_file("file", to_bytes("x")));
        REQUIRE(fileSystem.create_directories("dir"));
        const u64 before = fileSystem.write_counter();

        CHECK_FALSE(fileSystem.write_file("dir", to_bytes("x")));
        CHECK_FALSE(fileSystem.rename("missing", "other"));
        CHECK_FALSE(fileSystem.remove("missing"));
        CHECK_FALSE(fileSystem.create_directories("file"));
        CHECK_FALSE(fileSystem.write_file("../escape", to_bytes("x")));

        CHECK(fileSystem.write_counter() == before);
    }

    TEST_CASE("memory_file_system - set_read_only freezes and thaws the tree") {
        memory_file_system fileSystem = {};
        REQUIRE(fileSystem.write_file("a.txt", to_bytes("a")));

        fileSystem.set_read_only(true);
        CHECK(fileSystem.is_read_only());
        CHECK(code_of(fileSystem.write_file("b.txt", to_bytes("b"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.remove("a.txt")) == error_code::PERMISSION_DENIED);
        CHECK(fileSystem.read_text_file("a.txt").value() == "a");

        fileSystem.set_read_only(false);
        CHECK_FALSE(fileSystem.is_read_only());
        REQUIRE(fileSystem.write_file("b.txt", to_bytes("b")));
        CHECK(fileSystem.exists("b.txt"));
    }

    TEST_CASE("memory_file_system - read-only checks happen after path validation") {
        memory_file_system fileSystem = {};
        fileSystem.set_read_only(true);

        CHECK(code_of(fileSystem.write_file("../x", to_bytes("x"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.write_file("ok", to_bytes("x"))) == error_code::PERMISSION_DENIED);
        CHECK(fileSystem.write_counter() == 0);
    }

    TEST_CASE("memory_file_system - an open file keeps the snapshot it was opened on") {
        memory_file_system fileSystem = {};
        REQUIRE(fileSystem.write_file("live.txt", to_bytes("version one")));

        const result<std::unique_ptr<file>> opened = fileSystem.open_read("live.txt");
        REQUIRE(opened);
        REQUIRE(fileSystem.write_file("live.txt", to_bytes("v2")));
        REQUIRE(fileSystem.remove("live.txt"));

        file &handle = **opened;
        CHECK(handle.size() == 11);
        std::array<std::byte, 11> buffer = {};
        REQUIRE(handle.read_at(0, buffer).value() == 11);
        CHECK(to_string(buffer) == "version one");
        CHECK_FALSE(fileSystem.exists("live.txt"));
    }

    TEST_CASE("memory_file_system - read_file returns a copy that later writes do not touch") {
        memory_file_system fileSystem = {};
        REQUIRE(fileSystem.write_file("copy.txt", to_bytes("original")));

        const result<std::vector<std::byte>> first = fileSystem.read_file("copy.txt");
        REQUIRE(first);
        REQUIRE(fileSystem.write_file("copy.txt", to_bytes("changed")));

        CHECK(to_string(*first) == "original");
        CHECK(fileSystem.read_text_file("copy.txt").value() == "changed");
    }

    TEST_CASE("memory_file_system - concurrent writers to distinct files all land") {
        memory_file_system fileSystem = {};
        constexpr int THREAD_COUNT = 8;
        constexpr int FILES_PER_THREAD = 200;

        std::atomic<int> failures = 0;
        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([&fileSystem, &failures, threadIndex] {
                for (int index = 0; index < FILES_PER_THREAD; ++index) {
                    const std::string path = "thread" + std::to_string(threadIndex) + "/file" + std::to_string(index);
                    const std::string payload = std::to_string(threadIndex) + ":" + std::to_string(index);
                    if (!fileSystem.write_file(path, to_bytes(payload))) {
                        ++failures;
                    }
                }
            });
        }
        for (std::thread &thread : threads) {
            thread.join();
        }

        CHECK(failures.load() == 0);
        // One directory per thread plus one tick per file.
        CHECK(fileSystem.write_counter() == static_cast<u64>(THREAD_COUNT + THREAD_COUNT * FILES_PER_THREAD));
        const result<enumeration> listing = enumerate_all(
          fileSystem,
          "",
          enumerate_flags::RECURSIVE | enumerate_flags::FILES
        );
        REQUIRE(listing);
        CHECK(listing->paths.size() == static_cast<usize>(THREAD_COUNT * FILES_PER_THREAD));
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            for (int index = 0; index < FILES_PER_THREAD; index += 37) {
                const std::string path = "thread" + std::to_string(threadIndex) + "/file" + std::to_string(index);
                const std::string expected = std::to_string(threadIndex) + ":" + std::to_string(index);
                CHECK(fileSystem.read_text_file(path).value() == expected);
            }
        }
    }

    TEST_CASE("memory_file_system - concurrent writers to the same file leave one complete version") {
        memory_file_system fileSystem = {};
        constexpr int THREAD_COUNT = 8;
        constexpr int WRITES_PER_THREAD = 300;

        std::vector<std::string> payloads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            const usize length = static_cast<usize>(64 + threadIndex * 16);
            payloads.push_back(std::string(length, static_cast<char>('A' + threadIndex)));
        }

        std::atomic<int> failures = 0;
        std::atomic<int> torn = 0;
        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([&fileSystem, &failures, &torn, &payloads, threadIndex] {
                const std::vector<std::byte> mine = to_bytes(payloads[static_cast<usize>(threadIndex)]);
                for (int iteration = 0; iteration < WRITES_PER_THREAD; ++iteration) {
                    if (!fileSystem.write_file_atomic("shared.bin", mine)) {
                        ++failures;
                    }
                    const result<std::vector<std::byte>> read = fileSystem.read_file("shared.bin");
                    if (!read.has_value()) {
                        ++failures;
                        continue;
                    }
                    const std::string seen = to_string(*read);
                    bool known = false;
                    for (const std::string &candidate : payloads) {
                        known = known || seen == candidate;
                    }
                    if (!known) {
                        ++torn;
                    }
                }
            });
        }
        for (std::thread &thread : threads) {
            thread.join();
        }

        CHECK(failures.load() == 0);
        CHECK(torn.load() == 0);
        CHECK(fileSystem.write_counter() == static_cast<u64>(THREAD_COUNT * WRITES_PER_THREAD));
    }

    TEST_CASE("memory_file_system - readers, enumerators and removers run concurrently with writers") {
        memory_file_system fileSystem = {};
        REQUIRE(fileSystem.create_directories("work"));
        std::atomic<bool> stop = false;
        std::atomic<int> failures = 0;

        std::thread writer([&fileSystem, &stop, &failures] {
            for (int index = 0; index < 500; ++index) {
                const std::string path = "work/item" + std::to_string(index % 50);
                if (!fileSystem.write_file(path, to_bytes(std::to_string(index)))) {
                    ++failures;
                }
            }
            stop = true;
        });
        std::thread remover([&fileSystem, &stop] {
            while (!stop) {
                for (int index = 0; index < 50; index += 5) {
                    // Removing a file another thread may have just removed is an expected NOT_FOUND.
                    static_cast<void>(fileSystem.remove("work/item" + std::to_string(index)));
                }
            }
        });
        std::thread reader([&fileSystem, &stop, &failures] {
            while (!stop) {
                const result<enumeration> listing = enumerate_all(fileSystem, "work", enumerate_flags::FILES);
                if (!listing.has_value()) {
                    ++failures;
                    continue;
                }
                for (const std::string &path : listing->paths) {
                    const result<std::vector<std::byte>> bytes = fileSystem.read_file(path);
                    if (!bytes.has_value() && code_of(bytes) != error_code::NOT_FOUND) {
                        ++failures;
                    }
                }
            }
        });

        writer.join();
        remover.join();
        reader.join();
        CHECK(failures.load() == 0);
        CHECK(fileSystem.exists("work"));
    }

    TEST_CASE("memory_file_system - set_read_only is safe to flip while other threads operate") {
        memory_file_system fileSystem = {};
        std::atomic<bool> stop = false;
        std::atomic<int> unexpected = 0;

        std::thread toggler([&fileSystem, &stop] {
            for (int index = 0; index < 2000; ++index) {
                fileSystem.set_read_only((index & 1) == 0);
            }
            fileSystem.set_read_only(false);
            stop = true;
        });
        std::thread writer([&fileSystem, &stop, &unexpected] {
            int index = 0;
            while (!stop) {
                const result<void> written = fileSystem.write_file("toggle.txt", to_bytes(std::to_string(index++)));
                if (!written.has_value() && code_of(written) != error_code::PERMISSION_DENIED) {
                    ++unexpected;
                }
            }
        });

        toggler.join();
        writer.join();
        CHECK(unexpected.load() == 0);
        REQUIRE(fileSystem.write_file("toggle.txt", to_bytes("final")));
        CHECK(fileSystem.read_text_file("toggle.txt").value() == "final");
    }
}
