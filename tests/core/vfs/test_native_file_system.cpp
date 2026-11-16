#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "vfs_test_helpers.h"

#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/vfs/native_file_system.h"
#include "core/vfs/native_platform.h"
#include "core/vfs/vfs_internal.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
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

        CHECK(fileSystem.to_native("").value() == root);
        CHECK(fileSystem.to_native("/").value() == root);
        CHECK(fileSystem.to_native(".").value() == root);
        CHECK(fileSystem.to_native("a/b.txt").value() == root / "a" / "b.txt");
        CHECK(fileSystem.to_native("/a/b.txt").value() == root / "a" / "b.txt");
        CHECK(fileSystem.to_native(".\\a\\\\b.txt").value() == root / "a" / "b.txt");
        CHECK(fileSystem.to_native("a/../b.txt").value() == root / "b.txt");
        CHECK(fileSystem.to_native("a/b/../..").value() == root);
        // A dot-dot segment is no escape in itself, and neither is a name that merely starts with dots.
        CHECK(fileSystem.to_native("a/..b/c").value() == root / "a" / "..b" / "c");
        // A name made of dots is no escape either. It is not a portable name, so on Windows it maps in the
        // extended-length form, the only one that addresses such an entry; elsewhere that form is the path itself.
        CHECK(fileSystem.to_native("...").value() == internal::always_extended_native_path(root) / "...");
    }

    TEST_CASE("native_file_system - to_native is lexical: a missing path and a read-only instance map too") {
        testing::temp_directory directory("core_vfs_native");
        const native_file_system writable(directory.path() / "not" / "there");
        const native_file_system readOnly(directory.path() / "not" / "there", true);

        const result<std::filesystem::path> missing = writable.to_native("no/such/file.bin");
        REQUIRE(missing);
        CHECK(*missing == directory.path() / "not" / "there" / "no" / "such" / "file.bin");
        CHECK_FALSE(std::filesystem::exists(directory.path() / "not"));

        const result<std::filesystem::path> fromReadOnly = readOnly.to_native("no/such/file.bin");
        REQUIRE(fromReadOnly);
        CHECK(*fromReadOnly == *missing);
        CHECK(readOnly.to_native("").value() == readOnly.root());
    }

    TEST_CASE("regression - code sweep F241: to_native rejects a path that leaves the root") {
        // to_native mapped any spelling without the check every file method applies, so "../outside.txt" came
        // back as a usable path above the root: an unchecked hole in a class that advertises a sandbox, for
        // any caller that hands the result to a process or a dynamic library. It now returns what the file
        // methods resolve, so an escape is PERMISSION_DENIED and names the offending path.
        testing::temp_directory directory("core_vfs_native");
        directory.write_file("outside.txt", "secret");
        native_file_system fileSystem(directory.path() / "root");
        REQUIRE(fileSystem.write_file("inside.txt", to_bytes("x")));

        constexpr std::array<std::string_view, 12> ESCAPES = {
            "..",
            "../outside.txt",
            "/../outside.txt",
            "..\\outside.txt",
            "inside.txt/../../outside.txt",
            "a/../../outside.txt",
            "./../outside.txt",
            "C:/outside.txt",
            "c:outside.txt",
            "./C:/outside.txt",
            "//server/share/outside.txt",
            "\\\\server\\share\\outside.txt",
        };
        for (const std::string_view escape : ESCAPES) {
            CAPTURE(escape);
            const result<std::filesystem::path> mapped = fileSystem.to_native(escape);
            CHECK(code_of(mapped) == error_code::PERMISSION_DENIED);
            if (!mapped.has_value()) {
                CHECK(mapped.get_error().message().find(escape) != std::string::npos);
            }
        }

        // The absolute path of a file inside the very root. On Windows it starts with a drive letter (or a UNC
        // prefix) and is still an escape. A POSIX absolute path starts with one separator, which means "from
        // the backend root": it is no escape and maps below the root like any relative path.
        const result<std::filesystem::path> inside = fileSystem.to_native("inside.txt");
        REQUIRE(inside);
        CHECK(read_native(*inside) == "x");
        const std::u8string absoluteUtf8 = std::filesystem::absolute(*inside).u8string();
        const std::string absolute(reinterpret_cast<const char *>(absoluteUtf8.data()), absoluteUtf8.size());
        const result<std::filesystem::path> fromAbsolute = fileSystem.to_native(absolute);
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            CHECK(code_of(fromAbsolute) == error_code::PERMISSION_DENIED);
        } else {
            REQUIRE(fromAbsolute);
            const std::filesystem::path below = fileSystem.root() / internal::path_from_utf8(absolute).relative_path();
            CHECK(fromAbsolute->lexically_normal() == below.lexically_normal());
        }

        // A read-only instance checks the same way: the denial is about the root, not about writing.
        const native_file_system readOnly(directory.path() / "root", true);
        CHECK(code_of(readOnly.to_native("../outside.txt")) == error_code::PERMISSION_DENIED);
        CHECK(readOnly.to_native("inside.txt").value() == *inside);
    }

    TEST_CASE("native_file_system - to_native beyond the classic Windows limit opens with the standard library") {
        // The mapping comes back in the form the operating system accepts whatever the length (the
        // extended-length form on Windows), so the caller can open what the file system wrote.
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

        const result<std::filesystem::path> mapped = fileSystem.to_native(file);
        REQUIRE(mapped);
        CHECK(read_native(*mapped) == "deep");
        REQUIRE(fileSystem.remove(file));
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
        CHECK(fileSystem.to_native("dir/file.txt").value() == directory.path() / "dir" / "file.txt");
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

        // The absolute path of a file inside the very same root never reads that file. On Windows its drive
        // letter (or UNC prefix) makes it an escape; a POSIX absolute path starts with one separator, which
        // means "from the backend root", so it names an entry below the root that does not exist.
        const result<std::filesystem::path> inside = fileSystem.to_native("inside.txt");
        REQUIRE(inside);
        const std::u8string absoluteUtf8 = std::filesystem::absolute(*inside).u8string();
        const std::string absolute(reinterpret_cast<const char *>(absoluteUtf8.data()), absoluteUtf8.size());
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            CHECK(code_of(fileSystem.read_file(absolute)) == error_code::PERMISSION_DENIED);
        } else {
            CHECK(code_of(fileSystem.read_file(absolute)) == error_code::NOT_FOUND);
        }
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

    TEST_CASE("regression - perf-round: an atomic write whose temporary alone crosses the classic Windows limit") {
        // write_file_atomic builds "<target>.tmp-<uuid>" (41 characters more than the target) after the
        // destination was resolved: a target of 219-247 characters (below the 248-character classic limit)
        // stayed in the classic form while its temporary reached 260 and could not be opened. The
        // integration tests hit it as soon as their
        // build directory was four characters longer. The target below is built to sit just under the
        // limit wherever the temporary root lives; the directory stays under the classic directory limit.
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        constexpr usize TARGET_LENGTH = 247;
        constexpr std::string_view FILE_NAME = "target.bin";
        const usize rootLength = directory.path().native().size();
        REQUIRE(rootLength + 1 + 1 + 1 + FILE_NAME.size() < TARGET_LENGTH);
        const std::string directoryName(TARGET_LENGTH - rootLength - 1 - 1 - FILE_NAME.size(), 'd');
        const std::string file = directoryName + "/" + std::string(FILE_NAME);
        CHECK(rootLength + 1 + file.size() == TARGET_LENGTH);
        CHECK(rootLength + 1 + file.size() + 41 > 262);

        REQUIRE(fileSystem.write_file_atomic(file, to_bytes("first")));
        REQUIRE(fileSystem.write_file_atomic(file, to_bytes("second")));
        const result<std::string> read = fileSystem.read_text_file(file);
        REQUIRE(read);
        CHECK(*read == "second");
        CHECK(native_entries(directory.path() / directoryName) == std::vector<std::string>{std::string(FILE_NAME)});
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

    TEST_CASE("regression - code sweep F243: write_file_atomic flushes the new contents before the swap") {
        // The temporary was written through a stream that was only closed, so its data could still sit in
        // the system cache when the rename reached the disk: a power cut in between left an empty or torn
        // file under the target's name, with the old contents gone. Every atomic write now goes through the
        // platform writer, which flushes the file and counts the files it flushed.
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        const u64 before = internal::flushed_file_count();

        REQUIRE(fileSystem.write_file_atomic("out/save.bin", to_bytes("first")));
        CHECK(internal::flushed_file_count() == before + 1);
        CHECK(read_native(directory.path() / "out" / "save.bin") == "first");

        REQUIRE(fileSystem.write_file_atomic("out/save.bin", to_bytes("second, replacing the first")));
        CHECK(internal::flushed_file_count() == before + 2);
        CHECK(read_native(directory.path() / "out" / "save.bin") == "second, replacing the first");

        REQUIRE(fileSystem.write_file_atomic("out/empty.bin", {}));
        CHECK(internal::flushed_file_count() == before + 3);
        CHECK(fileSystem.stat("out/empty.bin").value().size == 0);

        std::vector<std::string> entries = native_entries(directory.path() / "out");
        std::sort(entries.begin(), entries.end());
        CHECK(entries == std::vector<std::string>{"empty.bin", "save.bin"});
    }

    TEST_CASE("native_file_system - only write_file_atomic pays for a flush") {
        // write_file is for output that can be made again; it keeps the speed of a cached write.
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        REQUIRE(fileSystem.write_file_atomic("kept.bin", to_bytes("kept")));
        const u64 before = internal::flushed_file_count();

        REQUIRE(fileSystem.write_file("scratch.bin", to_bytes("scratch")));
        REQUIRE(fileSystem.write_text_file("scratch.txt", "scratch"));
        REQUIRE(fileSystem.create_directories("made/here"));
        REQUIRE(fileSystem.rename("scratch.bin", "kept.bin"));
        REQUIRE(fileSystem.rename("scratch.txt", "made/here/moved.txt"));
        REQUIRE(fileSystem.remove("kept.bin"));
        CHECK(fileSystem.read_text_file("made/here/moved.txt").value() == "scratch");

        CHECK(internal::flushed_file_count() == before);
    }

    TEST_CASE("native_file_system - a refused atomic write flushes nothing and leaves nothing behind") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        native_file_system readOnly(directory.path(), true);
        REQUIRE(fileSystem.create_directories("out/directory"));
        REQUIRE(fileSystem.write_file("out/blocker", to_bytes("x")));
        const u64 before = internal::flushed_file_count();

        CHECK(code_of(fileSystem.write_file_atomic("out/directory", to_bytes("y"))) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.write_file_atomic("out/blocker/below.bin", to_bytes("y"))) == error_code::IO_FAILURE);
        CHECK(code_of(fileSystem.write_file_atomic("out/bad:name", to_bytes("y"))) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(fileSystem.write_file_atomic("../outside.bin", to_bytes("y"))) == error_code::PERMISSION_DENIED);
        CHECK(code_of(readOnly.write_file_atomic("out/new.bin", to_bytes("y"))) == error_code::PERMISSION_DENIED);

        CHECK(internal::flushed_file_count() == before);
        std::vector<std::string> entries = native_entries(directory.path() / "out");
        std::sort(entries.begin(), entries.end());
        CHECK(entries == std::vector<std::string>{"blocker", "directory"});
        CHECK(read_native(directory.path() / "out" / "blocker") == "x");
    }

    TEST_CASE("native_file_system - a large atomic write arrives whole and a shorter one replaces all of it") {
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        constexpr usize LARGE_SIZE = 3 * 1024 * 1024 + 17;
        std::vector<std::byte> large(LARGE_SIZE);
        for (usize index = 0; index < large.size(); ++index) {
            large[index] = static_cast<std::byte>((index * 31 + 7) & 0xFF);
        }

        REQUIRE(fileSystem.write_file_atomic("big.bin", large));
        const result<std::vector<std::byte>> read = fileSystem.read_file("big.bin");
        REQUIRE(read);
        CHECK(read->size() == LARGE_SIZE);
        CHECK((*read == large));

        REQUIRE(fileSystem.write_file_atomic("big.bin", to_bytes("small")));
        CHECK(fileSystem.stat("big.bin").value().size == 5);
        CHECK(read_native(directory.path() / "big.bin") == "small");
        CHECK(native_entries(directory.path()) == std::vector<std::string>{"big.bin"});
    }

    TEST_CASE("native_file_system - the flushing writer creates and truncates, and a failure names the VFS path") {
        testing::temp_directory directory("core_vfs_native");
        const std::filesystem::path target = directory.path() / "direct.bin";

        REQUIRE(internal::write_file_flushed(target, to_bytes("a much longer first version"), "vfs/direct.bin"));
        CHECK(read_native(target) == "a much longer first version");
        REQUIRE(internal::write_file_flushed(target, to_bytes("short"), "vfs/direct.bin"));
        CHECK(read_native(target) == "short");
        REQUIRE(internal::write_file_flushed(target, {}, "vfs/direct.bin"));
        CHECK(std::filesystem::file_size(target) == 0);

        // It never creates directories (the file system does that first) and cannot write over one.
        const u64 before = internal::flushed_file_count();
        const result<void> orphan =
          internal::write_file_flushed(directory.path() / "missing" / "orphan.bin", to_bytes("x"), "vfs/orphan.bin");
        REQUIRE_FALSE(orphan);
        CHECK(orphan.get_error().generic_code() == error_code::IO_FAILURE);
        CHECK_MESSAGE(
          orphan.get_error().message().find("open for writing 'vfs/orphan.bin'") != std::string::npos,
          orphan.get_error().message()
        );
        REQUIRE(std::filesystem::create_directory(directory.path() / "occupied"));
        CHECK(code_of(internal::write_file_flushed(directory.path() / "occupied", to_bytes("x"), "vfs/occupied"))
              == error_code::IO_FAILURE);
        CHECK(internal::flushed_file_count() == before);
        CHECK_FALSE(std::filesystem::exists(directory.path() / "missing"));
        CHECK(std::filesystem::is_directory(directory.path() / "occupied"));
    }

    TEST_CASE("native_file_system - only a volume that has no flush at all keeps an atomic write it could not flush") {
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            // ERROR_INVALID_FUNCTION and ERROR_NOT_SUPPORTED are a driver without a flush routine; access
            // denied, a CRC error, a full disk and a device error are flushes that were tried and failed.
            CHECK(internal::flush_is_unsupported(1));
            CHECK(internal::flush_is_unsupported(50));
            CHECK_FALSE(internal::flush_is_unsupported(0));
            CHECK_FALSE(internal::flush_is_unsupported(5));
            CHECK_FALSE(internal::flush_is_unsupported(23));
            CHECK_FALSE(internal::flush_is_unsupported(112));
            CHECK_FALSE(internal::flush_is_unsupported(1117));
        } else {
            CHECK(internal::flush_is_unsupported(static_cast<u32>(EINVAL)));
            CHECK(internal::flush_is_unsupported(static_cast<u32>(ENOTSUP)));
            CHECK_FALSE(internal::flush_is_unsupported(0));
            CHECK_FALSE(internal::flush_is_unsupported(static_cast<u32>(EIO)));
            CHECK_FALSE(internal::flush_is_unsupported(static_cast<u32>(ENOSPC)));
            CHECK_FALSE(internal::flush_is_unsupported(static_cast<u32>(EROFS)));
        }
    }

    TEST_CASE("native_file_system - a volume that cannot flush is reported once per process, not on every save") {
        // The report is latched for the life of the process and cannot be taken back, so the case asks
        // whether an earlier case of a direct run of this executable (an atomic write on a volume
        // without a flush) already used it up; under CTest every case is a process of its own.
        const bool reportedBefore = internal::unflushable_volume_reported();
        const u32 expectedLines = reportedBefore ? 0 : 1;
        testing::log_capture logs = {};

        internal::note_unflushable_volume("project://saves/slot1.bin");
        internal::note_unflushable_volume("project://saves/slot2.bin");
        internal::note_unflushable_volume("project://settings.json");

        CHECK(internal::unflushable_volume_reported());
        CHECK(logs.count(log_level::WARN) == expectedLines);
        CHECK(logs.count() == expectedLines);
        if (!reportedBefore) {
            CHECK(logs.contains(log_level::WARN, "'project://saves/slot1.bin' cannot flush"));
        }

        // Every later save on such a volume is silent, whoever made the first report.
        internal::note_unflushable_volume("project://saves/slot3.bin");
        internal::note_unflushable_volume("project://saves/slot1.bin");
        CHECK(logs.count() == expectedLines);
        CHECK(internal::unflushable_volume_reported());
    }

    TEST_CASE("native_file_system - entries with names that cannot be created are still read, moved and removed") {
        // Creation is strict (code sweep F753), everything else is not: a project made elsewhere, or by
        // another program, may hold such names and must stay readable and cleanable. Windows keeps a
        // trailing dot or space (and, before Windows 11, a device name inside a directory) only for a path in
        // the extended-length form, so the case makes the odd entries through that form of the directory
        // (elsewhere it is the directory itself).
        // The file system has to reach them whichever form its root was given in, so the case runs once
        // under the ordinary root and once under the extended one. Every odd entry is gone again before the
        // fixture cleans up, which it could not do itself.
        testing::temp_directory directory("core_vfs_native");
        const std::filesystem::path literalRoot = internal::always_extended_native_path(directory.path());
        std::filesystem::path root = directory.path();
        SUBCASE("under an ordinary root") {}
        SUBCASE("under a root in the extended-length form") {
            root = literalRoot;
        }
        native_file_system fileSystem(root);
        const std::array<std::string_view, 4> oddNames = {"aux", "nul.txt", "trailing dot.", "trailing space "};
        for (const std::string_view name : oddNames) {
            CAPTURE(name);
            const std::filesystem::path oddName = internal::path_from_utf8(name);
            REQUIRE(std::filesystem::create_directories(literalRoot / "odd directory." / oddName));
            std::ofstream stream(literalRoot / oddName, std::ios::binary);
            stream << "odd " << name;
            REQUIRE(stream.good());
        }

        const result<enumeration> listing = enumerate_all(fileSystem, "", enumerate_flags::FILES);
        CHECK(code_of(listing) == error_code::NONE);
        const bool listed = listing.has_value()
                            && listing->paths
                                 == std::vector<std::string>{"aux", "nul.txt", "trailing dot.", "trailing space "};
        CHECK(listed);
        const result<enumeration> nested = enumerate_all(fileSystem, "odd directory.", enumerate_flags::DIRECTORIES);
        CHECK(code_of(nested) == error_code::NONE);
        CHECK((nested.has_value() && nested->paths.size() == oddNames.size()));

        for (const std::string_view name : oddNames) {
            CAPTURE(name);
            const std::string expected = "odd " + std::string(name);
            const result<std::string> text = fileSystem.read_text_file(name);
            CHECK((text.has_value() && *text == expected));
            const result<file_stat> info = fileSystem.stat(name);
            CHECK((info.has_value() && info->isRegular && info->size == expected.size()));
            const result<std::unique_ptr<file>> opened = fileSystem.open_read(name);
            CHECK((opened.has_value() && (*opened)->size() == expected.size()));

            // Written to, such a name is a new entry like any other and refused; nothing changes on disk.
            CHECK(code_of(fileSystem.write_file(name, to_bytes("rewritten"))) == error_code::INVALID_ARGUMENT);
            CHECK(code_of(fileSystem.write_file_atomic(name, to_bytes("rewritten"))) == error_code::INVALID_ARGUMENT);
            CHECK(code_of(fileSystem.create_directories(std::string(name) + "/below")) == error_code::INVALID_ARGUMENT);
        }

        // Cleaning up: move one to a portable name, remove the others, remove the directory tree.
        CHECK(code_of(fileSystem.rename("trailing dot.", "trailing dot.txt")) == error_code::NONE);
        const result<std::string> moved = fileSystem.read_text_file("trailing dot.txt");
        CHECK((moved.has_value() && *moved == "odd trailing dot."));
        CHECK(code_of(fileSystem.remove("aux")) == error_code::NONE);
        CHECK(code_of(fileSystem.remove("nul.txt")) == error_code::NONE);
        CHECK(code_of(fileSystem.remove("trailing space ")) == error_code::NONE);
        CHECK(code_of(fileSystem.remove("odd directory./aux")) == error_code::NONE);
        CHECK(code_of(fileSystem.remove_all("odd directory.")) == error_code::NONE);

        const result<enumeration> remaining = enumerate_all(fileSystem, "", enumerate_flags::RECURSIVE);
        CHECK(code_of(remaining) == error_code::NONE);
        CHECK((remaining.has_value() && remaining->paths == std::vector<std::string>{"trailing dot.txt"}));

        // Whatever the checks above found, nothing the fixture cannot remove stays behind.
        std::error_code ignored = {};
        std::filesystem::remove_all(literalRoot / "odd directory.", ignored);
        for (const std::string_view name : oddNames) {
            std::filesystem::remove(literalRoot / internal::path_from_utf8(name), ignored);
        }
    }

    TEST_CASE("regression - code sweep F753: a name Windows would rewrite reaches its own entry, not its neighbour") {
        // The reading and removing calls take any name (decision C12), but the native backend handed a short
        // path to Win32 in the classic form, where a trailing dot or space is dropped. Under an ordinary root
        // "report." therefore named its neighbour "report": stat, read_file and open_read described the wrong
        // file, rename moved it, to_native and enumerate("logs.") led to the neighbour as well, and
        // remove_all("logs.") took the directory "logs". Only a root given in the extended-length form worked.
        // A path with such a name now takes that form whatever its length. Elsewhere the names were always
        // separate entries and the case is a plain round trip.
        constexpr std::string_view DOTTED = "dotted report";
        testing::temp_directory directory("core_vfs_native");
        const std::filesystem::path literalRoot = internal::always_extended_native_path(directory.path());
        native_file_system fileSystem(directory.path());
        const auto writeLiteral = [&literalRoot](std::string_view relative, std::string_view text) {
            std::filesystem::path target = literalRoot / internal::path_from_utf8(relative);
            std::ofstream stream(target.make_preferred(), std::ios::binary);
            stream << text;
            return stream.good();
        };
        REQUIRE(fileSystem.write_file("report", to_bytes("plain")));
        REQUIRE(fileSystem.write_file("logs/kept.txt", to_bytes("kept")));
        REQUIRE(writeLiteral("report.", DOTTED));
        REQUIRE(std::filesystem::create_directory(literalRoot / "logs."));
        REQUIRE(writeLiteral("logs./inner.txt", "inner"));

        // Each name is an entry of its own: the size, the contents and the on-disk mapping are the dotted one's.
        const result<enumeration> listing = enumerate_all(fileSystem, "", enumerate_flags::NONE);
        const std::vector<std::string> expectedListing = {"logs", "logs.", "report", "report."};
        CHECK((listing.has_value() && listing->paths == expectedListing));
        const result<file_stat> dotted = fileSystem.stat("report.");
        CHECK((dotted.has_value() && dotted->isRegular && dotted->size == DOTTED.size()));
        const result<std::string> dottedText = fileSystem.read_text_file("report.");
        CHECK((dottedText.has_value() && *dottedText == DOTTED));
        {
            const result<std::unique_ptr<file>> opened = fileSystem.open_read("report.");
            CHECK((opened.has_value() && (*opened)->size() == DOTTED.size()));
        }
        const result<std::filesystem::path> mapped = fileSystem.to_native("report.");
        CHECK((mapped.has_value() && read_native(*mapped) == DOTTED));
        const result<enumeration> nested = enumerate_all(fileSystem, "logs.", enumerate_flags::NONE);
        CHECK((nested.has_value() && nested->paths == std::vector<std::string>{"logs./inner.txt"}));
        const result<std::string> innerText = fileSystem.read_text_file("logs./inner.txt");
        CHECK((innerText.has_value() && *innerText == "inner"));

        // Moving and removing the dotted entries leaves the plain ones alone.
        CHECK(code_of(fileSystem.rename("report.", "moved.txt")) == error_code::NONE);
        CHECK_FALSE(fileSystem.exists("report."));
        CHECK(read_native(directory.path() / "report") == "plain");
        CHECK(read_native(directory.path() / "moved.txt") == DOTTED);

        CHECK(writeLiteral("report.", "a second dotted report"));
        CHECK(code_of(fileSystem.remove("report.")) == error_code::NONE);
        CHECK_FALSE(fileSystem.exists("report."));
        CHECK(read_native(directory.path() / "report") == "plain");

        CHECK(code_of(fileSystem.remove_all("logs.")) == error_code::NONE);
        CHECK(read_native(directory.path() / "logs" / "kept.txt") == "kept");
        std::vector<std::string> entries = native_entries(directory.path());
        std::sort(entries.begin(), entries.end());
        CHECK(entries == std::vector<std::string>{"logs", "moved.txt", "report"});

        // Whatever the checks above found, nothing the fixture cannot remove stays behind.
        std::error_code ignored = {};
        std::filesystem::remove(literalRoot / "report.", ignored);
        std::filesystem::remove_all(literalRoot / "logs.", ignored);
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

    TEST_CASE("native_file_system - an unchanged file reports the same write time on every stat and enumerate") {
        // Asset and file watchers compare write times for equality. The libc++ conversion used to relate the
        // file clock to the system clock through two now() calls, so every stat of an untouched file differed.
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        REQUIRE(fileSystem.write_file("steady.txt", to_bytes("x")));
        const result<file_stat> first = fileSystem.stat("steady.txt");
        REQUIRE(first);

        int mismatches = 0;
        for (int index = 0; index < 100; ++index) {
            const result<file_stat> again = fileSystem.stat("steady.txt");
            if (!again.has_value() || again->writeTimeNanoseconds != first->writeTimeNanoseconds) {
                ++mismatches;
            }
        }
        const result<enumeration> listing = enumerate_all(fileSystem, "", enumerate_flags::FILES);
        REQUIRE(listing);
        REQUIRE(listing->stats.size() == 1);

        CHECK(mismatches == 0);
        CHECK(listing->stats[0].writeTimeNanoseconds == first->writeTimeNanoseconds);
    }

    TEST_CASE(
      "native_file_system - recursive enumerate and remove_all reach entries beyond the classic Windows path limit"
    ) {
        // The walk used to start from the classic form of a short root, so descending past MAX_PATH failed and
        // aborted the whole scan (and remove_all the same way). Four nested 60-character directories put the
        // file past 260 characters wherever the temporary root lives; the fixture's own clean-up cannot walk
        // that deep, so the case removes the tree through the file system, whatever the checks before found.
        testing::temp_directory directory("core_vfs_native");
        native_file_system fileSystem(directory.path());
        const std::string name(60, 'd');
        const std::string deep = name + "/" + name + "/" + name + "/" + name + "/deep.bin";
        CHECK(directory.path().native().size() + 1 + deep.size() > 262);
        REQUIRE(fileSystem.write_file(deep, to_bytes("deep")));

        const enumerate_flags allFiles = enumerate_flags::RECURSIVE | enumerate_flags::FILES;
        const result<enumeration> files = enumerate_all(fileSystem, "", allFiles);
        CHECK(code_of(files) == error_code::NONE);
        const bool listed = files.has_value() && files->paths == std::vector<std::string>{deep};
        CHECK(listed);

        CHECK(code_of(fileSystem.remove_all(name)) == error_code::NONE);
        CHECK_FALSE(fileSystem.exists(name));
    }

    TEST_CASE(
      "native_file_system - replace falls back to MoveFileExW for every error code that means no POSIX rename"
    ) {
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            // ERROR_INVALID_FUNCTION (SMB/NAS redirectors, some FAT/exFAT drivers), ERROR_NOT_SUPPORTED and
            // ERROR_INVALID_PARAMETER mean the volume has no POSIX rename; ERROR_ACCESS_DENIED is an ACL refusal
            // or a delete-pending target that MoveFileExW would refuse just the same.
            CHECK(core::vfs::internal::rename_falls_back_to_move(1));
            CHECK(core::vfs::internal::rename_falls_back_to_move(50));
            CHECK(core::vfs::internal::rename_falls_back_to_move(87));
            CHECK_FALSE(core::vfs::internal::rename_falls_back_to_move(5));
            CHECK_FALSE(core::vfs::internal::rename_falls_back_to_move(0));
        } else {
            CHECK_FALSE(core::vfs::internal::rename_falls_back_to_move(1));
        }
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
