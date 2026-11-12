#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "vfs_test_helpers.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/core/vfs/mount_table.h"
#include "conjure/core/vfs/native_file_system.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;
using namespace conjure::testing::vfs_helpers;

namespace {
    using split_pair = std::pair<std::string_view, std::string_view>;

    struct mounted_pair {
        std::shared_ptr<memory_file_system> assets = std::make_shared<memory_file_system>();
        std::shared_ptr<memory_file_system> cooked = std::make_shared<memory_file_system>();
        mount_table table = {};

        mounted_pair() {
            REQUIRE(table.mount("assets", assets));
            REQUIRE(table.mount("cooked://", cooked));
        }
    };

    /// <summary>
    /// Forwards to a memory_file_system but refuses every remove(), like a backend whose files
    /// are held open by another process.
    /// </summary>
    class undeletable_file_system final : public file_system {
        private:
            memory_file_system mStorage = {};

        public:
            undeletable_file_system() = default;
            ~undeletable_file_system() override = default;

        public:
            [[nodiscard]] memory_file_system &storage() noexcept {
                return mStorage;
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

            [[nodiscard]] result<void> remove(std::string_view) override {
                return make_error(error_code::IO_FAILURE, "the file is in use");
            }

            [[nodiscard]] result<void> create_directories(std::string_view path) override {
                return mStorage.create_directories(path);
            }

            [[nodiscard]] result<std::unique_ptr<file>> open_read(std::string_view path) override {
                return mStorage.open_read(path);
            }
    };
}

TEST_SUITE("core/mount_table") {

    TEST_CASE("mount_table - split separates scheme and path") {
        const result<split_pair> simple = mount_table::split("assets://textures/a.png");
        REQUIRE(simple);
        CHECK(simple->first == "assets");
        CHECK(simple->second == "textures/a.png");

        const result<split_pair> module = mount_table::split("modules://my-mod/data/x.json");
        REQUIRE(module);
        CHECK(module->first == "modules");
        CHECK(module->second == "my-mod/data/x.json");

        const result<split_pair> rootOnly = mount_table::split("project://");
        REQUIRE(rootOnly);
        CHECK(rootOnly->first == "project");
        CHECK(rootOnly->second == "");

        const result<split_pair> nested = mount_table::split("a.b+c-d_e://x://y");
        REQUIRE(nested);
        CHECK(nested->first == "a.b+c-d_e");
        CHECK(nested->second == "x://y");
    }

    TEST_CASE("mount_table - split rejects bare paths and malformed schemes") {
        CHECK(code_of(mount_table::split("textures/a.png")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(mount_table::split("")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(mount_table::split("://path")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(mount_table::split("C:/windows/path")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(mount_table::split("as sets://x")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(mount_table::split("as/sets://x")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(mount_table::split("assets:/x")) == error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("mount_table - mount, find_mount, schemes and unmount") {
        mount_table table = {};
        const std::shared_ptr<memory_file_system> backend = std::make_shared<memory_file_system>();
        CHECK(table.schemes().empty());
        CHECK(table.find_mount("assets") == nullptr);

        REQUIRE(table.mount("assets", backend));
        CHECK(table.find_mount("assets") == backend.get());
        CHECK(table.find_mount("assets://") == backend.get());
        CHECK(table.find_mount("other") == nullptr);
        CHECK(table.schemes() == std::vector<std::string>{"assets"});

        REQUIRE(table.mount("project://", std::make_shared<memory_file_system>()));
        CHECK(table.schemes() == std::vector<std::string>{"assets", "project"});
        CHECK(table.find_mount("project") != nullptr);

        REQUIRE(table.unmount("assets://"));
        CHECK(table.find_mount("assets") == nullptr);
        CHECK(table.schemes() == std::vector<std::string>{"project"});
        REQUIRE(table.unmount("project"));
        CHECK(table.schemes().empty());
    }

    TEST_CASE("mount_table - mount rejects duplicates, null backends, bad schemes and itself") {
        mount_table table = {};
        REQUIRE(table.mount("assets", std::make_shared<memory_file_system>()));

        CHECK(code_of(table.mount("assets", std::make_shared<memory_file_system>())) == error_code::ALREADY_EXISTS);
        CHECK(code_of(table.mount("assets://", std::make_shared<memory_file_system>())) == error_code::ALREADY_EXISTS);
        CHECK(code_of(table.mount("other", nullptr)) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.mount("", std::make_shared<memory_file_system>())) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.mount("://", std::make_shared<memory_file_system>())) == error_code::INVALID_ARGUMENT);
        const std::shared_ptr<memory_file_system> spare = std::make_shared<memory_file_system>();
        CHECK(code_of(table.mount("bad scheme", spare)) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.mount("bad/scheme", spare)) == error_code::INVALID_ARGUMENT);

        const std::shared_ptr<mount_table> self = std::make_shared<mount_table>();
        CHECK(code_of(self->mount("loop", self)) == error_code::INVALID_ARGUMENT);
        CHECK(table.schemes() == std::vector<std::string>{"assets"});
    }

    TEST_CASE("mount_table - unmount of an unknown scheme is NOT_FOUND") {
        mount_table table = {};
        CHECK(code_of(table.unmount("assets")) == error_code::NOT_FOUND);
        CHECK(code_of(table.unmount("")) == error_code::NOT_FOUND);
    }

    TEST_CASE("mount_table - unmount keeps a backend alive while another owner holds it") {
        mount_table table = {};
        const std::shared_ptr<memory_file_system> backend = std::make_shared<memory_file_system>();
        REQUIRE(backend->write_file("kept.txt", to_bytes("kept")));
        REQUIRE(table.mount("assets", backend));
        CHECK(backend.use_count() == 2);

        REQUIRE(table.unmount("assets"));
        CHECK(backend.use_count() == 1);
        CHECK(backend->read_text_file("kept.txt").value() == "kept");
    }

    TEST_CASE("mount_table - every operation routes to the mounted backend") {
        mounted_pair mounts = {};
        file_system &table = mounts.table;

        REQUIRE(table.write_file("assets://textures/a.png", to_bytes("png")));
        REQUIRE(table.write_file_atomic("assets://textures/b.png", to_bytes("png2")));
        REQUIRE(table.write_text_file("cooked://index.txt", "index"));
        REQUIRE(table.create_directories("assets://models/empty"));

        CHECK(mounts.assets->read_text_file("textures/a.png").value() == "png");
        CHECK(mounts.assets->read_text_file("textures/b.png").value() == "png2");
        CHECK(mounts.cooked->read_text_file("index.txt").value() == "index");
        CHECK(mounts.assets->stat("models/empty").value().isDirectory);

        CHECK(to_string(table.read_file("assets://textures/a.png").value()) == "png");
        CHECK(table.read_text_file("cooked://index.txt").value() == "index");
        CHECK(table.stat("assets://textures/b.png").value().size == 4);
        CHECK(table.exists("assets://textures"));
        CHECK_FALSE(table.exists("cooked://textures"));
        CHECK(table.open_read("assets://textures/a.png").value()->size() == 3);

        REQUIRE(table.rename("assets://textures/a.png", "assets://textures/renamed.png"));
        CHECK(mounts.assets->exists("textures/renamed.png"));
        CHECK_FALSE(mounts.assets->exists("textures/a.png"));

        REQUIRE(table.remove("assets://textures/renamed.png"));
        CHECK_FALSE(mounts.assets->exists("textures/renamed.png"));
        REQUIRE(table.remove_all("assets://textures"));
        CHECK_FALSE(mounts.assets->exists("textures"));
        CHECK(mounts.assets->exists("models/empty"));
    }

    TEST_CASE("mount_table - the root of a mount is addressed by scheme:// alone") {
        mounted_pair mounts = {};
        REQUIRE(mounts.table.write_file("assets://a.txt", to_bytes("a")));

        CHECK(mounts.table.stat("assets://").value().isDirectory);
        CHECK(mounts.table.exists("assets://"));
        CHECK(enumerate_all(mounts.table, "assets://", enumerate_flags::NONE).value().paths
              == std::vector<std::string>{"assets://a.txt"});
        CHECK(code_of(mounts.table.remove("assets://")) == error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("mount_table - enumerate callbacks receive URIs that route back to the same entries") {
        mounted_pair mounts = {};
        REQUIRE(mounts.table.write_file("assets://dir/a.txt", to_bytes("a")));
        REQUIRE(mounts.table.write_file("assets://dir/sub/b.txt", to_bytes("b")));

        const result<enumeration> listing = enumerate_all(mounts.table, "assets://dir", enumerate_flags::RECURSIVE);
        REQUIRE(listing);
        const std::vector<std::string> expected = {"assets://dir/a.txt", "assets://dir/sub", "assets://dir/sub/b.txt"};
        CHECK(listing->paths == expected);
        CHECK(listing->stats[0].isRegular);
        CHECK(listing->stats[1].isDirectory);
        for (const std::string &path : listing->paths) {
            CHECK(mounts.table.exists(path));
        }

        const result<enumeration> files = enumerate_all(
          mounts.table,
          "assets://dir/",
          enumerate_flags::RECURSIVE | enumerate_flags::FILES
        );
        REQUIRE(files);
        CHECK(files->paths == std::vector<std::string>{"assets://dir/a.txt", "assets://dir/sub/b.txt"});
    }

    TEST_CASE("mount_table - bare paths are INVALID_ARGUMENT for every operation") {
        mounted_pair mounts = {};
        file_system &table = mounts.table;
        REQUIRE(table.write_file("assets://a.txt", to_bytes("a")));

        CHECK(code_of(table.read_file("a.txt")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.read_text_file("a.txt")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.write_file("b.txt", to_bytes("b"))) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.write_file_atomic("b.txt", to_bytes("b"))) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.stat("a.txt")) == error_code::INVALID_ARGUMENT);
        CHECK_FALSE(table.exists("a.txt"));
        CHECK(code_of(enumerate_all(table, "", enumerate_flags::NONE)) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.rename("a.txt", "assets://b.txt")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.rename("assets://a.txt", "b.txt")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.remove("a.txt")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.remove_all("a.txt")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.create_directories("dir")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(table.open_read("a.txt")) == error_code::INVALID_ARGUMENT);
        CHECK(mounts.assets->exists("a.txt"));
    }

    TEST_CASE("mount_table - unmounted schemes are NOT_FOUND for every operation") {
        mounted_pair mounts = {};
        file_system &table = mounts.table;

        CHECK(code_of(table.read_file("engine://a.txt")) == error_code::NOT_FOUND);
        CHECK(code_of(table.write_file("engine://a.txt", to_bytes("a"))) == error_code::NOT_FOUND);
        CHECK(code_of(table.stat("engine://")) == error_code::NOT_FOUND);
        CHECK_FALSE(table.exists("engine://"));
        CHECK(code_of(enumerate_all(table, "engine://", enumerate_flags::NONE)) == error_code::NOT_FOUND);
        CHECK(code_of(table.rename("engine://a", "assets://a")) == error_code::NOT_FOUND);
        CHECK(code_of(table.rename("assets://a", "engine://a")) == error_code::NOT_FOUND);
        CHECK(code_of(table.remove("engine://a")) == error_code::NOT_FOUND);
        CHECK(code_of(table.remove_all("engine://a")) == error_code::NOT_FOUND);
        CHECK(code_of(table.create_directories("engine://a")) == error_code::NOT_FOUND);
        CHECK(code_of(table.open_read("engine://a")) == error_code::NOT_FOUND);
    }

    TEST_CASE("mount_table - backend errors pass through unchanged") {
        mounted_pair mounts = {};
        REQUIRE(mounts.table.create_directories("assets://dir"));

        CHECK(code_of(mounts.table.read_file("assets://missing")) == error_code::NOT_FOUND);
        CHECK(code_of(mounts.table.read_file("assets://dir")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(mounts.table.read_file("assets://../x")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(mounts.table.remove("assets://dir/../")) == error_code::INVALID_ARGUMENT);

        mounts.cooked->set_read_only(true);
        CHECK(code_of(mounts.table.write_file("cooked://x", to_bytes("x"))) == error_code::PERMISSION_DENIED);
    }

    TEST_CASE("mount_table - rename across mounts copies a file and removes the source") {
        mounted_pair mounts = {};
        REQUIRE(mounts.table.write_file("assets://raw/model.bin", to_bytes("model bytes")));
        REQUIRE(mounts.table.create_directories("cooked://models"));

        REQUIRE(mounts.table.rename("assets://raw/model.bin", "cooked://models/model.bin"));

        CHECK_FALSE(mounts.assets->exists("raw/model.bin"));
        CHECK(mounts.assets->exists("raw"));
        CHECK(mounts.cooked->read_text_file("models/model.bin").value() == "model bytes");

        // An existing file at the destination is replaced, as a rename within one backend does.
        REQUIRE(mounts.table.write_file("assets://raw/model.bin", to_bytes("rebuilt")));
        REQUIRE(mounts.table.rename("assets://raw/model.bin", "cooked://models/model.bin"));
        CHECK_FALSE(mounts.assets->exists("raw/model.bin"));
        CHECK(mounts.cooked->read_text_file("models/model.bin").value() == "rebuilt");
    }

    TEST_CASE(
      "mount_table - rename across mounts onto a directory is ALREADY_EXISTS and into a missing parent is NOT_FOUND"
    ) {
        mounted_pair mounts = {};
        REQUIRE(mounts.table.write_file("assets://file.txt", to_bytes("f")));
        REQUIRE(mounts.table.create_directories("cooked://dir"));
        REQUIRE(mounts.table.write_file("cooked://blocker.txt", to_bytes("b")));

        CHECK(code_of(mounts.table.rename("assets://file.txt", "cooked://dir")) == error_code::ALREADY_EXISTS);
        CHECK(code_of(mounts.table.rename("assets://file.txt", "cooked://")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(mounts.table.rename("assets://file.txt", "cooked://dir/..")) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(mounts.table.rename("assets://file.txt", "cooked://missing/file.txt")) == error_code::NOT_FOUND);
        CHECK(code_of(mounts.table.rename("assets://file.txt", "cooked://blocker.txt/f")) == error_code::NOT_FOUND);
        CHECK(code_of(mounts.table.rename("assets://file.txt", "cooked://../f")) == error_code::PERMISSION_DENIED);

        // Nothing was copied, created or removed by the refused renames.
        CHECK(mounts.assets->read_text_file("file.txt").value() == "f");
        CHECK_FALSE(mounts.cooked->exists("missing"));
        CHECK(mounts.cooked->stat("dir").value().isDirectory);
        CHECK(mounts.cooked->read_text_file("blocker.txt").value() == "b");
        CHECK(enumerate_all(*mounts.cooked, "", enumerate_flags::RECURSIVE).value().paths
              == std::vector<std::string>{"blocker.txt", "dir"});

        // The parent is taken from the normalised path, as a rename within one backend does:
        // "moved.txt/missing/.." names "moved.txt", whose parent is the root.
        REQUIRE(mounts.table.rename("assets://file.txt", "cooked://moved.txt/missing/.."));
        CHECK(mounts.cooked->read_text_file("moved.txt").value() == "f");
        CHECK_FALSE(mounts.assets->exists("file.txt"));
    }

    TEST_CASE("mount_table - rename across mounts from a read-only mount is PERMISSION_DENIED and copies nothing") {
        mounted_pair mounts = {};
        REQUIRE(mounts.table.write_file("assets://file.txt", to_bytes("f")));
        mounts.assets->set_read_only(true);

        CHECK(code_of(mounts.table.rename("assets://file.txt", "cooked://file.txt")) == error_code::PERMISSION_DENIED);
        CHECK_FALSE(mounts.cooked->exists("file.txt"));
        CHECK(mounts.assets->read_text_file("file.txt").value() == "f");
    }

    TEST_CASE("mount_table - rename across mounts removes its copy again when the source cannot be removed") {
        mount_table table = {};
        const std::shared_ptr<undeletable_file_system> held = std::make_shared<undeletable_file_system>();
        const std::shared_ptr<memory_file_system> cooked = std::make_shared<memory_file_system>();
        REQUIRE(table.mount("held", held));
        REQUIRE(table.mount("cooked", cooked));
        REQUIRE(held->storage().write_file("model.bin", to_bytes("model")));

        SUBCASE("a new file is removed again") {
            CHECK(code_of(table.rename("held://model.bin", "cooked://model.bin")) == error_code::IO_FAILURE);
            CHECK_FALSE(cooked->exists("model.bin"));
        }

        SUBCASE("a replaced file keeps the copy, since its old contents are gone") {
            REQUIRE(cooked->write_file("model.bin", to_bytes("old")));
            CHECK(code_of(table.rename("held://model.bin", "cooked://model.bin")) == error_code::IO_FAILURE);
            CHECK(cooked->read_text_file("model.bin").value() == "model");
        }

        SUBCASE("a copy that cannot be removed either is reported as a warning") {
            testing::log_capture logs = {};
            const std::shared_ptr<undeletable_file_system> stuck = std::make_shared<undeletable_file_system>();
            REQUIRE(table.mount("stuck", stuck));

            CHECK(code_of(table.rename("held://model.bin", "stuck://model.bin")) == error_code::IO_FAILURE);
            CHECK(stuck->storage().read_text_file("model.bin").value() == "model");
            CHECK(logs.contains(log_level::WARN, "could not remove the copy at 'stuck://model.bin'"));
        }

        CHECK(held->storage().read_text_file("model.bin").value() == "model");
    }

    TEST_CASE("mount_table - rename across mounts into a nested mount table checks the parent where the file lands") {
        mounted_pair mounts = {};
        const std::shared_ptr<mount_table> inner = std::make_shared<mount_table>();
        const std::shared_ptr<memory_file_system> data = std::make_shared<memory_file_system>();
        REQUIRE(inner->mount("data", data));
        REQUIRE(mounts.table.mount("inner", inner));
        REQUIRE(mounts.table.write_file("assets://a.txt", to_bytes("a")));
        REQUIRE(mounts.table.write_file("assets://b.txt", to_bytes("b")));

        CHECK(code_of(mounts.table.rename("assets://a.txt", "inner://data://missing/a.txt")) == error_code::NOT_FOUND);
        CHECK(code_of(mounts.table.rename("assets://a.txt", "inner://other://a.txt")) == error_code::NOT_FOUND);
        CHECK_FALSE(data->exists("missing"));
        CHECK(mounts.assets->exists("a.txt"));

        // "data://a.txt" lies at the root of the inner scheme. Its parent is looked up inside the
        // inner table; taking the URI itself apart would yield "data:/", which is not a URI.
        REQUIRE(mounts.table.rename("assets://a.txt", "inner://data://a.txt"));
        CHECK(data->read_text_file("a.txt").value() == "a");
        CHECK_FALSE(mounts.assets->exists("a.txt"));

        REQUIRE(data->create_directories("dir"));
        REQUIRE(mounts.table.rename("assets://b.txt", "inner://data://dir/b.txt"));
        CHECK(data->read_text_file("dir/b.txt").value() == "b");
    }

    TEST_CASE(
      "mount_table - rename across mounts reports a missing source, refuses directories and keeps the source on failure"
    ) {
        mounted_pair mounts = {};
        REQUIRE(mounts.table.create_directories("assets://dir"));
        REQUIRE(mounts.table.write_file("assets://file.txt", to_bytes("f")));

        CHECK(code_of(mounts.table.rename("assets://missing", "cooked://x")) == error_code::NOT_FOUND);
        CHECK(code_of(mounts.table.rename("assets://dir", "cooked://dir")) == error_code::UNSUPPORTED);
        CHECK(code_of(mounts.table.rename("assets://", "cooked://x")) == error_code::INVALID_ARGUMENT);
        CHECK(mounts.assets->exists("dir"));
        CHECK_FALSE(mounts.cooked->exists("dir"));

        mounts.cooked->set_read_only(true);
        CHECK(code_of(mounts.table.rename("assets://file.txt", "cooked://file.txt")) == error_code::PERMISSION_DENIED);
        CHECK(mounts.assets->read_text_file("file.txt").value() == "f");
    }

    TEST_CASE("mount_table - one backend under two schemes renames instead of copying") {
        mount_table table = {};
        const std::shared_ptr<memory_file_system> shared = std::make_shared<memory_file_system>();
        REQUIRE(table.mount("a", shared));
        REQUIRE(table.mount("b", shared));
        REQUIRE(table.write_file("a://x.txt", to_bytes("x")));
        REQUIRE(table.write_file("a://dir/inner.txt", to_bytes("inner")));

        // The same path through both schemes is the same file: nothing may be lost.
        REQUIRE(table.rename("a://x.txt", "b://x.txt"));
        CHECK(shared->read_text_file("x.txt").value() == "x");
        CHECK(table.read_text_file("b://x.txt").value() == "x");

        REQUIRE(table.rename("a://x.txt", "b://moved.txt"));
        CHECK_FALSE(shared->exists("x.txt"));
        CHECK(shared->read_text_file("moved.txt").value() == "x");

        // A real rename also moves directories, which copy-and-remove cannot.
        REQUIRE(table.rename("b://dir", "a://renamed"));
        CHECK_FALSE(shared->exists("dir"));
        CHECK(shared->read_text_file("renamed/inner.txt").value() == "inner");
    }

    TEST_CASE("mount_table - rename across mounts onto the same file through a nested table keeps it") {
        mount_table table = {};
        const std::shared_ptr<memory_file_system> shared = std::make_shared<memory_file_system>();
        const std::shared_ptr<mount_table> nested = std::make_shared<mount_table>();
        REQUIRE(nested->mount("c", shared));
        REQUIRE(table.mount("a", shared));
        REQUIRE(table.mount("b", nested));
        REQUIRE(table.write_file("a://x.txt", to_bytes("x")));
        REQUIRE(table.write_file("a://dir/inner.txt", to_bytes("inner")));

        // "a://x.txt" and "b://c://x.txt" are the same file, so renaming one onto the other changes
        // nothing in either direction.
        REQUIRE(table.rename("a://x.txt", "b://c://x.txt"));
        CHECK(shared->read_text_file("x.txt").value() == "x");
        REQUIRE(table.rename("b://c://x.txt", "a://x.txt"));
        CHECK(shared->read_text_file("x.txt").value() == "x");

        // Both sides reach one backend, so it is a real rename there and a directory moves too.
        REQUIRE(table.rename("a://dir", "b://c://renamed"));
        CHECK_FALSE(shared->exists("dir"));
        CHECK(shared->read_text_file("renamed/inner.txt").value() == "inner");
    }

    TEST_CASE(
      "mount_table - rename across mounts from a read-only backend behind a nested table is PERMISSION_DENIED"
    ) {
        mounted_pair mounts = {};
        const std::shared_ptr<mount_table> nested = std::make_shared<mount_table>();
        const std::shared_ptr<memory_file_system> frozen = std::make_shared<memory_file_system>();
        const std::shared_ptr<memory_file_system> live = std::make_shared<memory_file_system>();
        REQUIRE(frozen->write_file("x.txt", to_bytes("source")));
        frozen->set_read_only(true);
        REQUIRE(live->write_file("y.txt", to_bytes("y")));
        REQUIRE(nested->mount("r", frozen));
        REQUIRE(nested->mount("w", live));
        REQUIRE(mounts.table.mount("n", nested));
        REQUIRE(mounts.table.write_file("cooked://x.txt", to_bytes("old")));

        // The nested table as a whole is writable; the backend that holds the source is not.
        REQUIRE_FALSE(nested->is_read_only());
        CHECK(code_of(mounts.table.rename("n://r://x.txt", "cooked://x.txt")) == error_code::PERMISSION_DENIED);
        CHECK(code_of(mounts.table.rename("n://r://x.txt", "cooked://new.txt")) == error_code::PERMISSION_DENIED);
        CHECK(mounts.cooked->read_text_file("x.txt").value() == "old");
        CHECK_FALSE(mounts.cooked->exists("new.txt"));
        CHECK(frozen->read_text_file("x.txt").value() == "source");

        // A writable backend in the same nested table still moves its file.
        REQUIRE(mounts.table.rename("n://w://y.txt", "cooked://y.txt"));
        CHECK(mounts.cooked->read_text_file("y.txt").value() == "y");
        CHECK_FALSE(live->exists("y.txt"));
    }

    TEST_CASE("mount_table - mount rejects a mount table that leads back to this one") {
        const std::shared_ptr<mount_table> outer = std::make_shared<mount_table>();
        const std::shared_ptr<mount_table> middle = std::make_shared<mount_table>();
        const std::shared_ptr<mount_table> inner = std::make_shared<mount_table>();
        REQUIRE(inner->mount("data", std::make_shared<memory_file_system>()));
        REQUIRE(middle->mount("inner", inner));
        REQUIRE(outer->mount("middle", middle));

        CHECK(code_of(inner->mount("loop", outer)) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(inner->mount("loop", middle)) == error_code::INVALID_ARGUMENT);
        CHECK(code_of(middle->mount("loop", outer)) == error_code::INVALID_ARGUMENT);
        CHECK(inner->schemes() == std::vector<std::string>{"data"});

        // A table that does not contain this one is fine, and nested routing works through it.
        REQUIRE(inner->mount("sibling", std::make_shared<mount_table>()));
        REQUIRE(outer->write_text_file("middle://inner://data://file.txt", "nested"));
        CHECK(inner->read_text_file("data://file.txt").value() == "nested");
        CHECK_FALSE(outer->is_read_only());
    }

    TEST_CASE("mount_table - is_read_only is true only when every mounted backend is read-only") {
        mount_table table = {};
        CHECK_FALSE(table.is_read_only());

        const std::shared_ptr<memory_file_system> frozen = std::make_shared<memory_file_system>();
        frozen->set_read_only(true);
        REQUIRE(table.mount("frozen", frozen));
        CHECK(table.is_read_only());

        const std::shared_ptr<memory_file_system> live = std::make_shared<memory_file_system>();
        REQUIRE(table.mount("live", live));
        CHECK_FALSE(table.is_read_only());

        live->set_read_only(true);
        CHECK(table.is_read_only());
        REQUIRE(table.unmount("frozen"));
        CHECK(table.is_read_only());
    }

    TEST_CASE("mount_table - the engine's standard schemes can all be mounted side by side") {
        testing::temp_directory directory("core_vfs_mounts");
        mount_table table = {};
        const std::array<std::string_view, 5> schemes = {"project", "assets", "engine", "cooked", "modules"};
        for (const std::string_view scheme : schemes) {
            REQUIRE(table.mount(scheme, std::make_shared<native_file_system>(directory.path() / std::string(scheme))));
        }
        REQUIRE(table.write_text_file("modules://my-mod/manifest.json", "{}"));
        REQUIRE(table.write_text_file("project://project.json", "{}"));

        CHECK(table.read_text_file("modules://my-mod/manifest.json").value() == "{}");
        CHECK(std::filesystem::exists(directory.path() / "modules" / "my-mod" / "manifest.json"));
        CHECK(std::filesystem::exists(directory.path() / "project" / "project.json"));
        CHECK(table.schemes() == std::vector<std::string>{"assets", "cooked", "engine", "modules", "project"});
    }

    TEST_CASE("mount_table - mounting and unmounting while other threads route stays consistent") {
        mount_table table = {};
        const std::shared_ptr<memory_file_system> stable = std::make_shared<memory_file_system>();
        REQUIRE(stable->write_file("file.txt", to_bytes("stable")));
        REQUIRE(table.mount("stable", stable));

        std::atomic<bool> stop = false;
        std::atomic<int> unexpected = 0;
        std::thread mounter([&table, &stop] {
            for (int index = 0; index < 500; ++index) {
                const std::shared_ptr<memory_file_system> transient = std::make_shared<memory_file_system>();
                static_cast<void>(transient->write_file("t.txt", to_bytes("t")));
                static_cast<void>(table.mount("transient", transient));
                static_cast<void>(table.unmount("transient"));
            }
            stop = true;
        });

        std::vector<std::thread> readers = {};
        for (int readerIndex = 0; readerIndex < 4; ++readerIndex) {
            readers.emplace_back([&table, &stop, &unexpected] {
                while (!stop) {
                    const result<std::string> stableRead = table.read_text_file("stable://file.txt");
                    if (!stableRead.has_value() || *stableRead != "stable") {
                        ++unexpected;
                    }
                    const result<std::string> transientRead = table.read_text_file("transient://t.txt");
                    if (!transientRead.has_value() && code_of(transientRead) != error_code::NOT_FOUND) {
                        ++unexpected;
                    }
                    if (transientRead.has_value() && *transientRead != "t") {
                        ++unexpected;
                    }
                    static_cast<void>(table.find_mount("transient"));
                    static_cast<void>(table.schemes());
                }
            });
        }

        mounter.join();
        for (std::thread &reader : readers) {
            reader.join();
        }
        CHECK(unexpected.load() == 0);
        CHECK(table.schemes() == std::vector<std::string>{"stable"});
    }
}
