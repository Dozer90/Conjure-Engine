#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"

#include "vfs_test_helpers.h"

#include "conjure/core/vfs/file_system.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/core/vfs/native_file_system.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;
using namespace conjure::testing::vfs_helpers;

// Every case in this file runs once per backend. A fixture owns one writable file system and can
// hand out a read-only file system holding whatever a populate step wrote.

namespace {
    struct native_fixture {
        testing::temp_directory directory{"core_vfs_conformance"};
        native_file_system fileSystem{directory.path() / "root"};

        file_system &get() {
            return fileSystem;
        }

        std::unique_ptr<file_system> read_only_after(const std::function<void(file_system &)> &populate) {
            populate(fileSystem);
            return std::make_unique<native_file_system>(directory.path() / "root", true);
        }
    };

    struct memory_fixture {
        memory_file_system fileSystem = {};

        file_system &get() {
            return fileSystem;
        }

        std::unique_ptr<file_system> read_only_after(const std::function<void(file_system &)> &populate) {
            std::unique_ptr<memory_file_system> frozen = std::make_unique<memory_file_system>();
            populate(*frozen);
            frozen->set_read_only(true);
            return frozen;
        }
    };

    void populate_tree(file_system &fileSystem) {
        REQUIRE(fileSystem.write_file("tree/a.txt", to_bytes("a")));
        REQUIRE(fileSystem.write_file("tree/sub/b.txt", to_bytes("bb")));
        REQUIRE(fileSystem.write_file("tree/sub/deeper/c.txt", to_bytes("ccc")));
        REQUIRE(fileSystem.create_directories("tree/empty"));
    }
}

TYPE_TO_STRING(native_fixture);
TYPE_TO_STRING(memory_fixture);

#define CONJURE_VFS_CONFORMANCE_CASE(name) TEST_CASE_TEMPLATE(name, Fixture, native_fixture, memory_fixture)

TEST_SUITE("core/file_system_conformance") {

    /*-----------------------------------------------------------------------*/
    // write_file / read_file

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - write_file then read_file round-trips bytes") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        using namespace std::string_view_literals;
        const std::vector<std::byte> payload = to_bytes("hello\0binary\xFF world"sv);
        REQUIRE(payload.size() == 19);

        REQUIRE(fileSystem.write_file("data.bin", payload));

        const result<std::vector<std::byte>> read = fileSystem.read_file("data.bin");
        REQUIRE(read);
        CHECK(*read == payload);
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - write_file creates missing parent directories") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        REQUIRE(fileSystem.write_file("a/b/c/file.txt", to_bytes("deep")));

        CHECK(fileSystem.stat("a").value().isDirectory);
        CHECK(fileSystem.stat("a/b").value().isDirectory);
        CHECK(fileSystem.stat("a/b/c").value().isDirectory);
        CHECK(fileSystem.read_text_file("a/b/c/file.txt").value() == "deep");
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - write_file replaces the previous contents entirely") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        REQUIRE(fileSystem.write_file("file.txt", to_bytes("a much longer first version")));
        REQUIRE(fileSystem.write_file("file.txt", to_bytes("short")));

        CHECK(fileSystem.read_text_file("file.txt").value() == "short");
        CHECK(fileSystem.stat("file.txt").value().size == 5);
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - write_file with no bytes creates an empty file") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        REQUIRE(fileSystem.write_file("empty.bin", {}));

        const result<file_stat> info = fileSystem.stat("empty.bin");
        REQUIRE(info);
        CHECK(info->isRegular);
        CHECK(info->size == 0);
        CHECK(fileSystem.read_file("empty.bin").value().empty());
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - write_file onto a directory is INVALID_ARGUMENT") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.create_directories("dir"));

        CHECK(code_of(fileSystem.write_file("dir", to_bytes("x"))) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.write_file("", to_bytes("x"))) == error_code::INVALID_ARGUMENT);
        CHECK(fileSystem.stat("dir").value().isDirectory);
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - write_file below a file is IO_FAILURE") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("blocker", to_bytes("x")));

        CHECK(code_of(fileSystem.write_file("blocker/child.txt", to_bytes("y"))) == error_code::IO_FAILURE);
        CHECK(code_of(fileSystem.write_file("blocker/a/b.txt", to_bytes("y"))) == error_code::IO_FAILURE);
        CHECK(fileSystem.read_text_file("blocker").value() == "x");
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - read_file of a missing path is NOT_FOUND") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("present.txt", to_bytes("x")));

        CHECK(code_of(fileSystem.read_file("missing.txt")) == error_code::NOT_FOUND);
        CHECK(code_of(fileSystem.read_file("no/such/dir/file.txt")) == error_code::NOT_FOUND);
        CHECK(code_of(fileSystem.read_file("present.txt/child")) == error_code::NOT_FOUND);
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - read_file of a directory is INVALID_ARGUMENT") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.create_directories("dir"));

        CHECK(code_of(fileSystem.read_file("dir")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.read_file("")) == error_code::INVALID_ARGUMENT);
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - read_text_file and write_text_file round-trip text and drop a BOM"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        REQUIRE(fileSystem.write_text_file("text.txt", "line one\nline two"));
        CHECK(fileSystem.read_text_file("text.txt").value() == "line one\nline two");

        REQUIRE(fileSystem.write_text_file("bom.txt", "\xEF\xBB\xBFtext"));
        CHECK(fileSystem.read_text_file("bom.txt").value() == "text");
        CHECK(fileSystem.read_file("bom.txt").value().size() == 7);

        CHECK(code_of(fileSystem.read_text_file("missing.txt")) == error_code::NOT_FOUND);
    }

    /*-----------------------------------------------------------------------*/
    // write_file_atomic

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - write_file_atomic creates a file and its parents") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        REQUIRE(fileSystem.write_file_atomic("out/nested/result.bin", to_bytes("atomic")));

        CHECK(fileSystem.read_text_file("out/nested/result.bin").value() == "atomic");
        CHECK(fileSystem.stat("out/nested").value().isDirectory);
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - write_file_atomic replaces an existing file and leaves no temporary"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("out/result.bin", to_bytes("first version")));

        REQUIRE(fileSystem.write_file_atomic("out/result.bin", to_bytes("second")));

        CHECK(fileSystem.read_text_file("out/result.bin").value() == "second");
        const result<enumeration> listing = enumerate_all(fileSystem, "out", enumerate_flags::ALL);
        REQUIRE(listing);
        CHECK(listing->paths == std::vector<std::string>{"out/result.bin"});
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - write_file_atomic onto a directory is INVALID_ARGUMENT and leaves no temporary"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.create_directories("out/dir"));

        CHECK(code_of(fileSystem.write_file_atomic("out/dir", to_bytes("x"))) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.write_file_atomic("", to_bytes("x"))) == error_code::INVALID_ARGUMENT);

        const result<enumeration> listing = enumerate_all(fileSystem, "out", enumerate_flags::ALL);
        REQUIRE(listing);
        CHECK(listing->paths == std::vector<std::string>{"out/dir"});
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - write_file_atomic below a file is IO_FAILURE") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("blocker", to_bytes("x")));

        CHECK(code_of(fileSystem.write_file_atomic("blocker/child", to_bytes("y"))) == error_code::IO_FAILURE);
    }

    /*-----------------------------------------------------------------------*/
    // stat / exists

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - stat describes files, directories and the root") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("dir/file.bin", to_bytes("12345")));

        const result<file_stat> fileInfo = fileSystem.stat("dir/file.bin");
        REQUIRE(fileInfo);
        CHECK(fileInfo->isRegular);
        CHECK_FALSE(fileInfo->isDirectory);
        CHECK(fileInfo->size == 5);
        CHECK(fileInfo->writeTimeNanoseconds != 0);

        const result<file_stat> directoryInfo = fileSystem.stat("dir");
        REQUIRE(directoryInfo);
        CHECK(directoryInfo->isDirectory);
        CHECK_FALSE(directoryInfo->isRegular);
        CHECK(directoryInfo->size == 0);

        const result<file_stat> rootInfo = fileSystem.stat("");
        REQUIRE(rootInfo);
        CHECK(rootInfo->isDirectory);
        CHECK(fileSystem.stat("/").value().isDirectory);
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - stat of a missing path is NOT_FOUND") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("file", to_bytes("x")));

        CHECK(code_of(fileSystem.stat("missing")) == error_code::NOT_FOUND);
        CHECK(code_of(fileSystem.stat("file/below")) == error_code::NOT_FOUND);
        CHECK(code_of(fileSystem.stat("a/b/c")) == error_code::NOT_FOUND);
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - exists mirrors stat") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("dir/file", to_bytes("x")));

        CHECK(fileSystem.exists("dir/file"));
        CHECK(fileSystem.exists("dir"));
        CHECK(fileSystem.exists(""));
        CHECK_FALSE(fileSystem.exists("dir/none"));
        CHECK_FALSE(fileSystem.exists("../outside"));
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - write times are set and never decrease") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        REQUIRE(fileSystem.write_file("first.txt", to_bytes("1")));
        const u64 first = fileSystem.stat("first.txt").value().writeTimeNanoseconds;
        REQUIRE(fileSystem.write_file("second.txt", to_bytes("2")));
        const u64 second = fileSystem.stat("second.txt").value().writeTimeNanoseconds;
        REQUIRE(fileSystem.write_file("first.txt", to_bytes("1 again")));
        const u64 rewritten = fileSystem.stat("first.txt").value().writeTimeNanoseconds;

        CHECK(first != 0);
        CHECK(second >= first);
        CHECK(rewritten >= second);
    }

    /*-----------------------------------------------------------------------*/
    // enumerate

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - enumerate lists direct children with root-relative paths in ascending order"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        populate_tree(fileSystem);
        REQUIRE(fileSystem.write_file("tree/zed.txt", to_bytes("z")));

        const result<enumeration> listing = enumerate_all(fileSystem, "tree", enumerate_flags::NONE);
        REQUIRE(listing);
        CHECK(listing->paths == std::vector<std::string>{"tree/a.txt", "tree/empty", "tree/sub", "tree/zed.txt"});
        CHECK(listing->stats[0].isRegular);
        CHECK(listing->stats[0].size == 1);
        CHECK(listing->stats[1].isDirectory);
        CHECK(listing->stats[2].isDirectory);
        CHECK(listing->stats[3].isRegular);
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - enumerate of the root uses bare relative paths") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        populate_tree(fileSystem);
        REQUIRE(fileSystem.write_file("top.txt", to_bytes("t")));

        const result<enumeration> listing = enumerate_all(fileSystem, "", enumerate_flags::NONE);
        REQUIRE(listing);
        CHECK(listing->paths == std::vector<std::string>{"top.txt", "tree"});
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - enumerate RECURSIVE walks the whole subtree") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        populate_tree(fileSystem);

        const result<enumeration> listing = enumerate_all(fileSystem, "tree", enumerate_flags::RECURSIVE);
        REQUIRE(listing);
        const std::vector<std::string> expected = {
            "tree/a.txt", "tree/empty", "tree/sub", "tree/sub/b.txt", "tree/sub/deeper", "tree/sub/deeper/c.txt"
        };
        CHECK(listing->paths == expected);
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - enumerate filters by FILES and DIRECTORIES") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        populate_tree(fileSystem);

        const result<enumeration> files = enumerate_all(
          fileSystem,
          "tree",
          enumerate_flags::RECURSIVE | enumerate_flags::FILES
        );
        REQUIRE(files);
        CHECK(files->paths == std::vector<std::string>{"tree/a.txt", "tree/sub/b.txt", "tree/sub/deeper/c.txt"});

        const result<enumeration> directories = enumerate_all(
          fileSystem,
          "tree",
          enumerate_flags::RECURSIVE | enumerate_flags::DIRECTORIES
        );
        REQUIRE(directories);
        CHECK(directories->paths == std::vector<std::string>{"tree/empty", "tree/sub", "tree/sub/deeper"});

        const result<enumeration> shallowFiles = enumerate_all(fileSystem, "tree", enumerate_flags::FILES);
        REQUIRE(shallowFiles);
        CHECK(shallowFiles->paths == std::vector<std::string>{"tree/a.txt"});

        const result<enumeration> all = enumerate_all(fileSystem, "tree", enumerate_flags::ALL);
        REQUIRE(all);
        CHECK(all->paths == std::vector<std::string>{"tree/a.txt", "tree/empty", "tree/sub"});
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - enumerate of an empty directory reports nothing") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.create_directories("empty"));

        const result<enumeration> listing = enumerate_all(fileSystem, "empty", enumerate_flags::RECURSIVE);
        REQUIRE(listing);
        CHECK(listing->paths.empty());
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - enumerate of a missing directory is NOT_FOUND and of a file INVALID_ARGUMENT"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("file.txt", to_bytes("x")));

        CHECK(code_of(enumerate_all(fileSystem, "missing", enumerate_flags::NONE)) == error_code::NOT_FOUND);
        CHECK(code_of(enumerate_all(fileSystem, "file.txt", enumerate_flags::NONE)) == error_code::INVALID_ARGUMENT);
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - enumerate callbacks may call back into the file system") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        populate_tree(fileSystem);

        std::vector<std::string> contents = {};
        const result<void> outcome = fileSystem.enumerate(
          "tree",
          enumerate_flags::RECURSIVE | enumerate_flags::FILES,
          [&fileSystem, &contents](std::string_view path, const file_stat &) {
              contents.push_back(fileSystem.read_text_file(path).value());
              REQUIRE(fileSystem.write_file("tree/sub/written-during-enumeration.txt", to_bytes("w")));
          }
        );
        REQUIRE(outcome);
        CHECK(contents == std::vector<std::string>{"a", "bb", "ccc"});
        CHECK(fileSystem.exists("tree/sub/written-during-enumeration.txt"));
    }

    /*-----------------------------------------------------------------------*/
    // rename

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - rename moves a file") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("a/old.txt", to_bytes("content")));
        REQUIRE(fileSystem.create_directories("b"));

        REQUIRE(fileSystem.rename("a/old.txt", "b/new.txt"));

        CHECK_FALSE(fileSystem.exists("a/old.txt"));
        CHECK(fileSystem.read_text_file("b/new.txt").value() == "content");
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - rename replaces an existing file") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("source.txt", to_bytes("source")));
        REQUIRE(fileSystem.write_file("target.txt", to_bytes("target")));

        REQUIRE(fileSystem.rename("source.txt", "target.txt"));

        CHECK_FALSE(fileSystem.exists("source.txt"));
        CHECK(fileSystem.read_text_file("target.txt").value() == "source");
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - rename moves a directory with its contents") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        populate_tree(fileSystem);

        REQUIRE(fileSystem.rename("tree/sub", "moved"));

        CHECK_FALSE(fileSystem.exists("tree/sub"));
        CHECK(fileSystem.read_text_file("moved/b.txt").value() == "bb");
        CHECK(fileSystem.read_text_file("moved/deeper/c.txt").value() == "ccc");
        CHECK(fileSystem.exists("tree/a.txt"));
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - rename onto itself succeeds and changes nothing") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("same.txt", to_bytes("same")));

        REQUIRE(fileSystem.rename("same.txt", "./same.txt"));
        CHECK(fileSystem.read_text_file("same.txt").value() == "same");
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - rename onto another spelling of itself changes only the case"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("Dir/File.txt", to_bytes("cased")));

        // On a case-insensitive volume the destination is the source itself; on a case-sensitive
        // one it is a plain rename. Both must succeed and end with the new spelling.
        REQUIRE(fileSystem.rename("Dir", "dir"));
        CHECK(enumerate_all(fileSystem, "", enumerate_flags::NONE).value().paths == std::vector<std::string>{"dir"});
        CHECK(fileSystem.read_text_file("dir/File.txt").value() == "cased");

        REQUIRE(fileSystem.rename("dir/File.txt", "dir/file.txt"));
        CHECK(enumerate_all(fileSystem, "dir", enumerate_flags::NONE).value().paths
              == std::vector<std::string>{"dir/file.txt"});
        CHECK(fileSystem.read_text_file("dir/file.txt").value() == "cased");
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - rename of a missing source is NOT_FOUND") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        CHECK(code_of(fileSystem.rename("missing.txt", "other.txt")) == error_code::NOT_FOUND);
        CHECK(code_of(fileSystem.rename("no/dir/file.txt", "other.txt")) == error_code::NOT_FOUND);
        CHECK_FALSE(fileSystem.exists("other.txt"));
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - rename into a missing parent directory is NOT_FOUND") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("file.txt", to_bytes("x")));

        CHECK(code_of(fileSystem.rename("file.txt", "missing/file.txt")) == error_code::NOT_FOUND);
        CHECK(fileSystem.exists("file.txt"));
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - rename onto an existing directory is ALREADY_EXISTS") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("file.txt", to_bytes("x")));
        REQUIRE(fileSystem.create_directories("dir"));
        REQUIRE(fileSystem.create_directories("other"));

        CHECK(code_of(fileSystem.rename("file.txt", "dir")) == error_code::ALREADY_EXISTS);
        CHECK(code_of(fileSystem.rename("other", "dir")) == error_code::ALREADY_EXISTS);
        CHECK(fileSystem.exists("file.txt"));
        CHECK(fileSystem.exists("other"));
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - rename of a directory onto a file is ALREADY_EXISTS") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.create_directories("dir"));
        REQUIRE(fileSystem.write_file("file.txt", to_bytes("x")));

        CHECK(code_of(fileSystem.rename("dir", "file.txt")) == error_code::ALREADY_EXISTS);
        CHECK(fileSystem.stat("dir").value().isDirectory);
        CHECK(fileSystem.read_text_file("file.txt").value() == "x");
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - rename of a directory into itself is INVALID_ARGUMENT") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        populate_tree(fileSystem);

        CHECK(code_of(fileSystem.rename("tree", "tree/sub/tree")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.rename("tree", "tree/inside")) == error_code::INVALID_ARGUMENT);
        CHECK(fileSystem.exists("tree/sub/deeper/c.txt"));
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - rename involving the root is INVALID_ARGUMENT") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.create_directories("dir"));

        CHECK(code_of(fileSystem.rename("", "dir")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.rename("dir", "")) == error_code::INVALID_ARGUMENT);
    }

    /*-----------------------------------------------------------------------*/
    // remove / remove_all

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - remove deletes a file and an empty directory") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("dir/file.txt", to_bytes("x")));

        REQUIRE(fileSystem.remove("dir/file.txt"));
        CHECK_FALSE(fileSystem.exists("dir/file.txt"));
        CHECK(fileSystem.exists("dir"));

        REQUIRE(fileSystem.remove("dir"));
        CHECK_FALSE(fileSystem.exists("dir"));
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - remove of a non-empty directory is INVALID_STATE") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        populate_tree(fileSystem);

        CHECK(code_of(fileSystem.remove("tree")) == error_code::INVALID_STATE);
        CHECK(code_of(fileSystem.remove("tree/sub")) == error_code::INVALID_STATE);
        CHECK(fileSystem.exists("tree/sub/deeper/c.txt"));
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - remove of a missing path is NOT_FOUND and of the root INVALID_ARGUMENT"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("keep.txt", to_bytes("x")));

        CHECK(code_of(fileSystem.remove("missing")) == error_code::NOT_FOUND);
        CHECK(code_of(fileSystem.remove("no/such/path")) == error_code::NOT_FOUND);
        CHECK(code_of(fileSystem.remove("")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.remove("/")) == error_code::INVALID_ARGUMENT);
        CHECK(fileSystem.exists("keep.txt"));
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - remove_all deletes a whole subtree and a single file") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        populate_tree(fileSystem);
        REQUIRE(fileSystem.write_file("keep.txt", to_bytes("k")));

        REQUIRE(fileSystem.remove_all("tree"));
        CHECK_FALSE(fileSystem.exists("tree"));
        CHECK(fileSystem.exists("keep.txt"));

        REQUIRE(fileSystem.remove_all("keep.txt"));
        CHECK_FALSE(fileSystem.exists("keep.txt"));
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - remove_all of a missing path is NOT_FOUND and of the root INVALID_ARGUMENT"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("keep.txt", to_bytes("x")));

        CHECK(code_of(fileSystem.remove_all("missing")) == error_code::NOT_FOUND);
        CHECK(code_of(fileSystem.remove_all("")) == error_code::INVALID_ARGUMENT);
        CHECK(fileSystem.exists("keep.txt"));
    }

    /*-----------------------------------------------------------------------*/
    // create_directories

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - create_directories creates nested directories and is idempotent"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        REQUIRE(fileSystem.create_directories("x/y/z"));
        CHECK(fileSystem.stat("x").value().isDirectory);
        CHECK(fileSystem.stat("x/y").value().isDirectory);
        CHECK(fileSystem.stat("x/y/z").value().isDirectory);

        REQUIRE(fileSystem.create_directories("x/y/z"));
        REQUIRE(fileSystem.create_directories("x/y"));
        REQUIRE(fileSystem.create_directories(""));
        CHECK(enumerate_all(fileSystem, "x/y", enumerate_flags::NONE).value().paths
              == std::vector<std::string>{"x/y/z"});
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - create_directories over a file is ALREADY_EXISTS and below a file IO_FAILURE"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("file.txt", to_bytes("x")));

        CHECK(code_of(fileSystem.create_directories("file.txt")) == error_code::ALREADY_EXISTS);
        CHECK(code_of(fileSystem.create_directories("file.txt/sub")) == error_code::IO_FAILURE);
        CHECK(fileSystem.stat("file.txt").value().isRegular);
    }

    /*-----------------------------------------------------------------------*/
    // open_read

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - open_read supports whole, partial and offset reads") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("data.bin", to_bytes("0123456789")));

        const result<std::unique_ptr<file>> opened = fileSystem.open_read("data.bin");
        REQUIRE(opened);
        file &handle = **opened;
        CHECK(handle.size() == 10);

        std::array<std::byte, 10> whole = {};
        REQUIRE(handle.read_at(0, whole).value() == 10);
        CHECK(to_string(whole) == "0123456789");

        std::array<std::byte, 4> middle = {};
        REQUIRE(handle.read_at(3, middle).value() == 4);
        CHECK(to_string(middle) == "3456");

        std::array<std::byte, 16> tail = {};
        REQUIRE(handle.read_at(7, tail).value() == 3);
        CHECK(to_string(std::span<const std::byte>(tail).first(3)) == "789");

        std::array<std::byte, 2> again = {};
        REQUIRE(handle.read_at(0, again).value() == 2);
        CHECK(to_string(again) == "01");
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - open_read at the end reads nothing and past the end is OUT_OF_RANGE"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("data.bin", to_bytes("abc")));

        const result<std::unique_ptr<file>> opened = fileSystem.open_read("data.bin");
        REQUIRE(opened);
        std::array<std::byte, 4> buffer = {};
        CHECK((*opened)->read_at(3, buffer).value() == 0);
        CHECK(code_of((*opened)->read_at(4, buffer)) == error_code::OUT_OF_RANGE);
        CHECK((*opened)->read_at(1, std::span<std::byte>()).value() == 0);
    }

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - open_read of an empty file has size zero") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("empty.bin", {}));

        const result<std::unique_ptr<file>> opened = fileSystem.open_read("empty.bin");
        REQUIRE(opened);
        CHECK((*opened)->size() == 0);
        std::array<std::byte, 1> buffer = {};
        CHECK((*opened)->read_at(0, buffer).value() == 0);
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - an open handle does not block replacing, renaming over or removing its file"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("held.txt", to_bytes("old")));
        REQUIRE(fileSystem.write_file("incoming.txt", to_bytes("incoming")));

        const result<std::unique_ptr<file>> opened = fileSystem.open_read("held.txt");
        REQUIRE(opened);
        file &handle = **opened;

        REQUIRE(fileSystem.write_file_atomic("held.txt", to_bytes("new")));
        CHECK(fileSystem.read_text_file("held.txt").value() == "new");

        REQUIRE(fileSystem.rename("incoming.txt", "held.txt"));
        CHECK_FALSE(fileSystem.exists("incoming.txt"));
        CHECK(fileSystem.read_text_file("held.txt").value() == "incoming");

        // A second handle on the current file: removing it must take the name away at once.
        const result<std::unique_ptr<file>> current = fileSystem.open_read("held.txt");
        REQUIRE(current);
        REQUIRE(fileSystem.remove("held.txt"));
        CHECK_FALSE(fileSystem.exists("held.txt"));
        CHECK(code_of(fileSystem.stat("held.txt")) == error_code::NOT_FOUND);
        CHECK(code_of(fileSystem.open_read("held.txt")) == error_code::NOT_FOUND);
        CHECK(enumerate_all(fileSystem, "", enumerate_flags::FILES).value().paths.empty());
        REQUIRE(fileSystem.write_file("held.txt", to_bytes("again")));
        CHECK(fileSystem.read_text_file("held.txt").value() == "again");
        REQUIRE(fileSystem.remove("held.txt"));

        // The handle still reads the contents it was opened on.
        std::array<std::byte, 3> buffer = {};
        CHECK(handle.size() == 3);
        REQUIRE(handle.read_at(0, buffer).value() == 3);
        CHECK(to_string(buffer) == "old");
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - readers holding handles run concurrently with atomic writers and removers"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("hot.bin", to_bytes("0")));
        std::atomic<bool> stop = false;
        std::atomic<int> failures = 0;
        std::mutex firstFailureMutex = {};
        std::string firstFailure = {};
        const auto fail = [&failures, &firstFailureMutex, &firstFailure](const std::string &what) {
            ++failures;
            const std::scoped_lock lock(firstFailureMutex);
            if (firstFailure.empty()) {
                firstFailure = what;
            }
        };
        const auto message = [](const auto &outcome) {
            return outcome.has_value() ? std::string("succeeded") : std::string(outcome.get_error().message());
        };

        std::thread reader([&fileSystem, &stop, &fail, &message] {
            while (!stop) {
                const result<std::unique_ptr<file>> opened = fileSystem.open_read("hot.bin");
                if (!opened.has_value()) {
                    // The writer may have removed the file; anything else is a failure.
                    if (code_of(opened) != error_code::NOT_FOUND) {
                        fail("open_read: " + message(opened));
                    }
                    continue;
                }
                std::array<std::byte, 8> buffer = {};
                const result<usize> read = (*opened)->read_at(0, buffer);
                if (!read.has_value() || *read != (*opened)->size()) {
                    fail("read_at: " + message(read));
                }
                const result<std::vector<std::byte>> whole = fileSystem.read_file("hot.bin");
                if (!whole.has_value() && code_of(whole) != error_code::NOT_FOUND) {
                    fail("read_file: " + message(whole));
                }
            }
        });

        for (int index = 0; index < 200; ++index) {
            const std::vector<std::byte> payload = to_bytes(std::to_string(index));
            const result<void> written = fileSystem.write_file_atomic("hot.bin", payload);
            if (!written.has_value()) {
                fail("write_file_atomic: " + message(written));
            }
            if (index % 10 == 9) {
                const result<void> removed = fileSystem.remove("hot.bin");
                if (!removed.has_value()) {
                    fail("remove: " + message(removed));
                }
                const result<void> staged = fileSystem.write_file("staged.bin", payload);
                if (!staged.has_value()) {
                    fail("write_file: " + message(staged));
                }
                const result<void> renamed = fileSystem.rename("staged.bin", "hot.bin");
                if (!renamed.has_value()) {
                    fail("rename: " + message(renamed));
                }
            }
        }
        stop = true;
        reader.join();

        INFO("first failure: ", firstFailure);
        CHECK(failures.load() == 0);
        CHECK(fileSystem.read_text_file("hot.bin").value() == "199");
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - open_read of a missing path is NOT_FOUND and of a directory INVALID_ARGUMENT"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.create_directories("dir"));

        CHECK(code_of(fileSystem.open_read("missing.bin")) == error_code::NOT_FOUND);
        CHECK(code_of(fileSystem.open_read("dir")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.open_read("")) == error_code::INVALID_ARGUMENT);
    }

    /*-----------------------------------------------------------------------*/
    // Path handling

    CONJURE_VFS_CONFORMANCE_CASE("file_system_conformance - different spellings of a path address the same entry") {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("dir/file.txt", to_bytes("x")));

        CHECK(fileSystem.read_text_file("./dir/file.txt").value() == "x");
        CHECK(fileSystem.read_text_file("dir//file.txt").value() == "x");
        CHECK(fileSystem.read_text_file("dir\\file.txt").value() == "x");
        CHECK(fileSystem.read_text_file("/dir/file.txt").value() == "x");
        CHECK(fileSystem.read_text_file("dir/other/../file.txt").value() == "x");
        CHECK(fileSystem.read_text_file("other/../dir/file.txt").value() == "x");
        CHECK(fileSystem.stat("dir/").value().isDirectory);

        REQUIRE(fileSystem.write_file(".\\dir\\\\second.txt", to_bytes("y")));
        CHECK(enumerate_all(fileSystem, "dir/", enumerate_flags::NONE).value().paths
              == std::vector<std::string>{"dir/file.txt", "dir/second.txt"});
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - paths that escape the root are PERMISSION_DENIED for every operation"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("inside.txt", to_bytes("x")));

        const std::array<std::string_view, 8> escapes = {
            "..", "../x", "a/../../x", "/../x", "\\..\\x", "C:/x", "./C:/x", "//server/share/x"
        };
        for (const std::string_view escape : escapes) {
            CAPTURE(escape);
            CHECK(code_of(fileSystem.read_file(escape)) == error_code::PERMISSION_DENIED);
            CHECK(code_of(fileSystem.read_text_file(escape)) == error_code::PERMISSION_DENIED);
            CHECK(code_of(fileSystem.write_file(escape, to_bytes("y"))) == error_code::PERMISSION_DENIED);
            CHECK(code_of(fileSystem.write_file_atomic(escape, to_bytes("y"))) == error_code::PERMISSION_DENIED);
            CHECK(code_of(fileSystem.stat(escape)) == error_code::PERMISSION_DENIED);
            CHECK_FALSE(fileSystem.exists(escape));
            CHECK(code_of(enumerate_all(fileSystem, escape, enumerate_flags::NONE)) == error_code::PERMISSION_DENIED);
            CHECK(code_of(fileSystem.rename(escape, "inside.txt")) == error_code::PERMISSION_DENIED);
            CHECK(code_of(fileSystem.rename("inside.txt", escape)) == error_code::PERMISSION_DENIED);
            CHECK(code_of(fileSystem.remove(escape)) == error_code::PERMISSION_DENIED);
            CHECK(code_of(fileSystem.remove_all(escape)) == error_code::PERMISSION_DENIED);
            CHECK(code_of(fileSystem.create_directories(escape)) == error_code::PERMISSION_DENIED);
            CHECK(code_of(fileSystem.open_read(escape)) == error_code::PERMISSION_DENIED);
        }
        CHECK(fileSystem.read_text_file("inside.txt").value() == "x");
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - a parent reference that stays inside the root is allowed, one above it is not"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        REQUIRE(fileSystem.write_file("a/b/../c.txt", to_bytes("c")));
        CHECK(fileSystem.read_text_file("a/c.txt").value() == "c");
        CHECK_FALSE(fileSystem.exists("a/b"));
        REQUIRE(fileSystem.write_file("/a/../top.txt", to_bytes("t")));
        CHECK(fileSystem.read_text_file("top.txt").value() == "t");

        // A leading slash means "from the root", and the root has no parent to climb to.
        CHECK(code_of(fileSystem.write_file("/../top.txt", to_bytes("escaped"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.read_file("/../top.txt")) == error_code::PERMISSION_DENIED);
        CHECK(fileSystem.read_text_file("top.txt").value() == "t");
        CHECK(enumerate_all(fileSystem, "", enumerate_flags::NONE).value().paths
              == std::vector<std::string>{"a", "top.txt"});
    }

    /*-----------------------------------------------------------------------*/
    // Names of new entries

    CONJURE_VFS_CONFORMANCE_CASE(
      "regression - code sweep F753: a name that is not portable is INVALID_ARGUMENT wherever an entry is created"
    ) {
        // A ':' in a later segment made Windows write an alternate data stream of another file and leave a
        // 0-byte file behind, a trailing dot or space was dropped so the entry landed under another name,
        // and a device name never reached the disk at all; the memory backend took every one of them.
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("notes.txt", to_bytes("kept")));
        REQUIRE(fileSystem.write_file("dir/source.txt", to_bytes("source")));

        using namespace std::string_view_literals;
        const std::array<std::string_view, 30> refused = {
            "notes.txt:draft",   "fresh:stream",   "dir/in:side.txt", "what?.txt",      "star*.txt",
            "quote\".txt",       "less<.txt",      "greater>.txt",    "pipe|.txt",      "tab\there.txt",
            "bell\x07"sv,        "delete\x7F"sv,   "trailing.",       "trailing ",      "dots...",
            "dir./file.txt",     "dir /file.txt",  "dir/deep /x/y",   "nul",            "NUL.txt",
            "Con",               "aux.tar.gz",     "prn .txt",        "com1",           "COM9.log",
            "lpt1",              "Lpt5.dat",       "dir/aux/file",    "dir/nul.txt",    "a/b/c:d/e.txt"
        };
        for (const std::string_view path : refused) {
            CAPTURE(path);
            CHECK(code_of(fileSystem.write_file(path, to_bytes("x"))) == error_code::INVALID_ARGUMENT);
            CHECK(code_of(fileSystem.write_file_atomic(path, to_bytes("x"))) == error_code::INVALID_ARGUMENT);
            CHECK(code_of(fileSystem.write_text_file(path, "x")) == error_code::INVALID_ARGUMENT);
            CHECK(code_of(fileSystem.create_directories(path)) == error_code::INVALID_ARGUMENT);
            CHECK(code_of(fileSystem.rename("dir/source.txt", path)) == error_code::INVALID_ARGUMENT);
        }

        // Nothing was created, no stream was attached to the file that exists and the source of the
        // refused renames is where it was.
        const result<enumeration> listing = enumerate_all(fileSystem, "", enumerate_flags::RECURSIVE);
        REQUIRE(listing);
        CHECK(listing->paths == std::vector<std::string>{"dir", "dir/source.txt", "notes.txt"});
        CHECK(fileSystem.read_text_file("notes.txt").value() == "kept");
        CHECK(fileSystem.stat("notes.txt").value().size == 4);
        CHECK(fileSystem.read_text_file("dir/source.txt").value() == "source");
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - a refused name is reported with the path and the segment at fault"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        const result<void> written = fileSystem.write_file("saves\\slot 1 /state.bin", to_bytes("x"));
        REQUIRE_FALSE(written);
        const std::string &message = written.get_error().message();
        CHECK(written.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK_MESSAGE(message.find("saves\\slot 1 /state.bin") != std::string::npos, message);
        CHECK_MESSAGE(message.find("'slot 1 '") != std::string::npos, message);
        CHECK_MESSAGE(message.find("ends with a dot or a space") != std::string::npos, message);

        const result<void> device = fileSystem.create_directories("logs/aux");
        REQUIRE_FALSE(device);
        CHECK_MESSAGE(device.get_error().message().find("'aux'") != std::string::npos, device.get_error().message());
        CHECK_MESSAGE(
          device.get_error().message().find("Windows device name") != std::string::npos,
          device.get_error().message()
        );
        CHECK_FALSE(fileSystem.exists("saves"));
        CHECK_FALSE(fileSystem.exists("logs"));
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - names that only resemble a refused one are created, read and removed"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        const std::array<std::string_view, 24> accepted = {
            "com0",          "com10",         "lpt0",          "lpt12.txt",    "console",       "nulls.txt",
            "auxiliary",     "prnt",          "xcon",          "con1",         "a.b.c",         ".hidden",
            "..data",        "dots...inside", "with space.txt", " leading",    "semi;colon",    "hash#tag",
            "tilde~1",       "per%cent",      "brace{}[]()",   "plus+equals=", "at@sign!$&',",  "\xC3\xBC" "ber.txt"
        };
        for (const std::string_view name : accepted) {
            CAPTURE(name);
            const std::string file = "files/" + std::string(name);
            const std::string directory = "directories/" + std::string(name);
            REQUIRE(fileSystem.write_file(file, to_bytes(name)));
            REQUIRE(fileSystem.create_directories(directory));
            CHECK(fileSystem.read_text_file(file).value() == name);
            CHECK(fileSystem.stat(directory).value().isDirectory);

            const std::string moved = "moved/" + std::string(name);
            REQUIRE(fileSystem.write_file_atomic(moved + ".staged", to_bytes("staged")));
            REQUIRE(fileSystem.rename(moved + ".staged", moved));
            CHECK(fileSystem.read_text_file(moved).value() == "staged");
        }

        CHECK(enumerate_all(fileSystem, "files", enumerate_flags::NONE).value().paths.size() == accepted.size());
        REQUIRE(fileSystem.remove_all("files"));
        REQUIRE(fileSystem.remove_all("directories"));
        REQUIRE(fileSystem.remove_all("moved"));
        CHECK(enumerate_all(fileSystem, "", enumerate_flags::NONE).value().paths.empty());
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - reading, describing, listing and removing judge no name"
    ) {
        // Only creation is strict: an entry that carries such a name already (made on another platform,
        // or by another program) must stay reachable, so these calls look the name up and report what
        // they find. Nothing of that name exists here, which is NOT_FOUND, never INVALID_ARGUMENT.
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("present.txt", to_bytes("x")));

        const std::array<std::string_view, 6> lenient = {
            "missing:stream", "dir/in:side.txt", "what?.txt", "star*.txt", "trailing.", "trailing "
        };
        for (const std::string_view path : lenient) {
            CAPTURE(path);
            CHECK(code_of(fileSystem.read_file(path)) == error_code::NOT_FOUND);
            CHECK(code_of(fileSystem.read_text_file(path)) == error_code::NOT_FOUND);
            CHECK(code_of(fileSystem.open_read(path)) == error_code::NOT_FOUND);
            CHECK(code_of(fileSystem.stat(path)) == error_code::NOT_FOUND);
            CHECK_FALSE(fileSystem.exists(path));
            CHECK(code_of(enumerate_all(fileSystem, path, enumerate_flags::NONE)) == error_code::NOT_FOUND);
            CHECK(code_of(fileSystem.remove(path)) == error_code::NOT_FOUND);
            CHECK(code_of(fileSystem.remove_all(path)) == error_code::NOT_FOUND);
            CHECK(code_of(fileSystem.rename(path, "portable.txt")) == error_code::NOT_FOUND);
        }
        CHECK(fileSystem.read_text_file("present.txt").value() == "x");
        CHECK_FALSE(fileSystem.exists("portable.txt"));
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - a refused name ranks below an escape and a read-only backend, above the lookup"
    ) {
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();
        REQUIRE(fileSystem.write_file("file.txt", to_bytes("x")));

        // A path that leaves the root is an escape whatever its segments are called; "a:b" at the very
        // start is a drive letter, which is an escape too.
        CHECK(code_of(fileSystem.write_file("../out:side", to_bytes("y"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.write_file("a:b.txt", to_bytes("y"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.create_directories("../nul")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.rename("file.txt", "../trailing.")) == error_code::PERMISSION_DENIED);

        // The destination's name is judged before the source is looked up, and the root stays the root.
        CHECK(code_of(fileSystem.rename("missing.txt", "bad:name")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.rename("bad:name", "")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.write_file("file.txt/bad:name", to_bytes("y"))) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.create_directories("file.txt/nul")) == error_code::INVALID_ARGUMENT);

        const std::unique_ptr<file_system> readOnly = fixture.read_only_after([](file_system &writable) {
            REQUIRE(writable.write_file("frozen.txt", to_bytes("frozen")));
        });
        CHECK(code_of(readOnly->write_file("bad:name", to_bytes("y"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(readOnly->write_file_atomic("nul", to_bytes("y"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(readOnly->create_directories("trailing.")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(readOnly->rename("frozen.txt", "what?")) == error_code::PERMISSION_DENIED);
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - other spellings of a portable path are still accepted where an entry is created"
    ) {
        // The rule is applied to the normalised path, so separators, "." and ".." are not names.
        Fixture fixture = {};
        file_system &fileSystem = fixture.get();

        REQUIRE(fileSystem.write_file(".\\made\\.\\up/../here.txt", to_bytes("x")));
        REQUIRE(fileSystem.create_directories("/made//deeper/./"));
        REQUIRE(fileSystem.rename("made/here.txt", "made\\deeper\\..\\there.txt"));
        REQUIRE(fileSystem.create_directories(""));
        REQUIRE(fileSystem.create_directories("."));

        CHECK(enumerate_all(fileSystem, "", enumerate_flags::RECURSIVE).value().paths
              == std::vector<std::string>{"made", "made/deeper", "made/there.txt"});
    }

    /*-----------------------------------------------------------------------*/
    // Read-only backends

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - a read-only file system rejects every mutation and still reads"
    ) {
        Fixture fixture = {};
        const std::unique_ptr<file_system> readOnly = fixture.read_only_after([](file_system &writable) {
            REQUIRE(writable.write_file("dir/file.txt", to_bytes("frozen")));
            REQUIRE(writable.create_directories("dir/empty"));
        });
        file_system &fileSystem = *readOnly;

        CHECK(fileSystem.is_read_only());
        CHECK(fileSystem.read_text_file("dir/file.txt").value() == "frozen");
        CHECK(fileSystem.stat("dir/file.txt").value().size == 6);
        CHECK(fileSystem.exists("dir/empty"));
        CHECK(enumerate_all(fileSystem, "dir", enumerate_flags::NONE).value().paths
              == std::vector<std::string>{"dir/empty", "dir/file.txt"});
        CHECK(fileSystem.open_read("dir/file.txt").value()->size() == 6);

        CHECK(code_of(fileSystem.write_file("dir/file.txt", to_bytes("x"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.write_file("new.txt", to_bytes("x"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.write_file_atomic("new.txt", to_bytes("x"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.write_text_file("new.txt", "x")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.rename("dir/file.txt", "dir/other.txt")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.remove("dir/file.txt")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.remove("dir/empty")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.remove_all("dir")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.create_directories("dir/created")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(fileSystem.create_directories("dir/empty")) == error_code::PERMISSION_DENIED);

        CHECK(fileSystem.read_text_file("dir/file.txt").value() == "frozen");
        CHECK_FALSE(fileSystem.exists("new.txt"));
        CHECK_FALSE(fileSystem.exists("dir/created"));
    }

    CONJURE_VFS_CONFORMANCE_CASE(
      "file_system_conformance - a read-only file system still reports escapes and missing paths"
    ) {
        Fixture fixture = {};
        const std::unique_ptr<file_system> readOnly = fixture.read_only_after([](file_system &) {});

        CHECK(code_of(readOnly->read_file("../x")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(readOnly->read_file("missing")) == error_code::NOT_FOUND);
        CHECK(code_of(readOnly->write_file("../x", to_bytes("y"))) == error_code::PERMISSION_DENIED);
    }
}
