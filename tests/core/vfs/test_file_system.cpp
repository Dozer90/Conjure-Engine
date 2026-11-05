#include "conjure/testing/doctest.h"

#include "vfs_test_helpers.h"

#include "conjure/core/vfs/file_system.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;
using namespace conjure::testing::vfs_helpers;

static_assert(std::has_virtual_destructor_v<file>);
static_assert(std::has_virtual_destructor_v<file_system>);
static_assert(!std::is_copy_constructible_v<file_system>);
static_assert(!std::is_copy_assignable_v<file_system>);
static_assert(!std::is_copy_constructible_v<file>);
static_assert(std::is_abstract_v<file>);
static_assert(std::is_abstract_v<file_system>);

namespace {
    /// <summary>
    /// Implements only the pure virtual methods by forwarding to a memory_file_system, so the
    /// default implementations in file_system itself are what gets exercised.
    /// </summary>
    class minimal_file_system final : public file_system {
        private:
            memory_file_system mStorage = {};
            int mRemoveCalls = 0;

        public:
            minimal_file_system() = default;
            ~minimal_file_system() override = default;

        public:
            [[nodiscard]] int remove_calls() const noexcept {
                return mRemoveCalls;
            }

            [[nodiscard]] result<std::vector<std::byte>> read_file(std::string_view path) override {
                return mStorage.read_file(path);
            }

            [[nodiscard]] result<void> write_file(std::string_view path, std::span<const std::byte> data) override {
                return mStorage.write_file(path, data);
            }

            [[nodiscard]] result<void> write_file_atomic(
              std::string_view path,
              std::span<const std::byte> data
            ) override {
                return mStorage.write_file_atomic(path, data);
            }

            [[nodiscard]] result<file_stat> stat(std::string_view path) override {
                return mStorage.stat(path);
            }

            [[nodiscard]] result<void> enumerate(
              std::string_view directory,
              enumerate_flags flags,
              const enumerate_callback &callback
            ) override {
                return mStorage.enumerate(directory, flags, callback);
            }

            [[nodiscard]] result<void> rename(std::string_view from, std::string_view to) override {
                return mStorage.rename(from, to);
            }

            [[nodiscard]] result<void> remove(std::string_view path) override {
                ++mRemoveCalls;
                return mStorage.remove(path);
            }

            [[nodiscard]] result<void> create_directories(std::string_view path) override {
                return mStorage.create_directories(path);
            }

            [[nodiscard]] result<std::unique_ptr<file>> open_read(std::string_view path) override {
                return mStorage.open_read(path);
            }
    };
}

TEST_SUITE("core/file_system") {

    TEST_CASE("file_system - file_stat defaults describe nothing") {
        const file_stat info = {};
        CHECK(info.size == 0);
        CHECK(info.writeTimeNanoseconds == 0);
        CHECK_FALSE(info.isDirectory);
        CHECK_FALSE(info.isRegular);
    }

    TEST_CASE("file_system - enumerate_flags combine as a flag set") {
        static_assert(std::is_same_v<std::underlying_type_t<enumerate_flags>, u8>);
        CHECK(to_underlying(enumerate_flags::NONE) == 0);
        CHECK((enumerate_flags::FILES | enumerate_flags::DIRECTORIES) == enumerate_flags::ALL);
        CHECK(has_flag(enumerate_flags::ALL, enumerate_flags::FILES));
        CHECK(has_flag(enumerate_flags::ALL, enumerate_flags::DIRECTORIES));
        CHECK_FALSE(has_flag(enumerate_flags::ALL, enumerate_flags::RECURSIVE));

        enumerate_flags flags = enumerate_flags::RECURSIVE;
        flags |= enumerate_flags::FILES;
        CHECK(has_flag(flags, enumerate_flags::RECURSIVE));
        CHECK(has_flag(flags, enumerate_flags::FILES));
        CHECK((flags & enumerate_flags::DIRECTORIES) == enumerate_flags::NONE);
        CHECK((~enumerate_flags::NONE & enumerate_flags::ALL) == enumerate_flags::ALL);
    }

    TEST_CASE("file_system - read_text_file default builds a string from read_file") {
        minimal_file_system fileSystem = {};
        REQUIRE(fileSystem.write_file("notes.txt", to_bytes("plain text")));

        const result<std::string> text = fileSystem.read_text_file("notes.txt");
        REQUIRE(text);
        CHECK(*text == "plain text");
    }

    TEST_CASE("file_system - read_text_file default drops a leading UTF-8 byte order mark only") {
        minimal_file_system fileSystem = {};
        REQUIRE(fileSystem.write_file("bom.txt", to_bytes("\xEF\xBB\xBFwith bom")));
        REQUIRE(fileSystem.write_file("inner.txt", to_bytes("a\xEF\xBB\xBF" "b")));
        REQUIRE(fileSystem.write_file("partial.txt", to_bytes("\xEF\xBB")));

        CHECK(fileSystem.read_text_file("bom.txt").value() == "with bom");
        CHECK(fileSystem.read_text_file("inner.txt").value() == "a\xEF\xBB\xBF" "b");
        CHECK(fileSystem.read_text_file("partial.txt").value() == "\xEF\xBB");
    }

    TEST_CASE("file_system - read_text_file default propagates read_file errors") {
        minimal_file_system fileSystem = {};
        CHECK(code_of(fileSystem.read_text_file("missing.txt")) == error_code::NOT_FOUND);
    }

    TEST_CASE("file_system - write_text_file stores the UTF-8 bytes verbatim") {
        minimal_file_system fileSystem = {};
        REQUIRE(fileSystem.write_text_file("text/hello.txt", "h\xC3\xA9llo"));

        const result<std::vector<std::byte>> bytes = fileSystem.read_file("text/hello.txt");
        REQUIRE(bytes);
        CHECK(to_string(*bytes) == "h\xC3\xA9llo");
        CHECK(bytes->size() == 6);
    }

    TEST_CASE("file_system - exists default is true exactly when stat succeeds") {
        minimal_file_system fileSystem = {};
        REQUIRE(fileSystem.write_file("dir/file.bin", to_bytes("x")));

        CHECK(fileSystem.exists("dir/file.bin"));
        CHECK(fileSystem.exists("dir"));
        CHECK(fileSystem.exists(""));
        CHECK_FALSE(fileSystem.exists("dir/other"));
        CHECK_FALSE(fileSystem.exists("../escape"));
    }

    TEST_CASE("file_system - remove_all default removes a subtree bottom-up through remove") {
        minimal_file_system fileSystem = {};
        REQUIRE(fileSystem.write_file("tree/a/1.txt", to_bytes("1")));
        REQUIRE(fileSystem.write_file("tree/a/b/2.txt", to_bytes("2")));
        REQUIRE(fileSystem.write_file("tree/3.txt", to_bytes("3")));
        REQUIRE(fileSystem.create_directories("tree/empty"));
        REQUIRE(fileSystem.write_file("keep.txt", to_bytes("k")));

        REQUIRE(fileSystem.remove_all("tree"));

        CHECK_FALSE(fileSystem.exists("tree"));
        CHECK(fileSystem.exists("keep.txt"));
        // tree/a/1.txt, tree/a/b/2.txt, tree/a/b, tree/a, tree/3.txt, tree/empty and tree itself.
        CHECK(fileSystem.remove_calls() == 7);
    }

    TEST_CASE(
      "file_system - remove_all default empties a directory before siblings that sort between it and its children"
    ) {
        minimal_file_system fileSystem = {};
        // '-' and '.' sort before '/', so "t/a-b" and "t/a.b" fall between "t/a" and "t/a/child".
        REQUIRE(fileSystem.write_file("t/a/child.txt", to_bytes("1")));
        REQUIRE(fileSystem.write_file("t/a-b", to_bytes("2")));
        REQUIRE(fileSystem.write_file("t/a.b", to_bytes("3")));
        REQUIRE(fileSystem.create_directories("t/a-dir/sub"));

        REQUIRE(fileSystem.remove_all("t"));
        CHECK_FALSE(fileSystem.exists("t"));
        // t/a/child.txt, t/a, t/a-b, t/a.b, t/a-dir/sub, t/a-dir and t itself.
        CHECK(fileSystem.remove_calls() == 7);
    }

    TEST_CASE("file_system - remove_all default removes a single file") {
        minimal_file_system fileSystem = {};
        REQUIRE(fileSystem.write_file("single.txt", to_bytes("s")));

        REQUIRE(fileSystem.remove_all("single.txt"));
        CHECK_FALSE(fileSystem.exists("single.txt"));
        CHECK(fileSystem.remove_calls() == 1);
    }

    TEST_CASE("file_system - remove_all default reports a missing path, the root and escapes") {
        minimal_file_system fileSystem = {};
        CHECK(code_of(fileSystem.remove_all("missing")) == error_code::NOT_FOUND);
        CHECK(code_of(fileSystem.remove_all("")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.remove_all("/")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.remove_all("../x")) == error_code::PERMISSION_DENIED);
        CHECK(fileSystem.remove_calls() == 0);
    }

    TEST_CASE("file_system - is_read_only default is false") {
        minimal_file_system fileSystem = {};
        CHECK_FALSE(fileSystem.is_read_only());
    }
}
