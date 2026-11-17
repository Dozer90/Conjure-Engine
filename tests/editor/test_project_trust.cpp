#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/native_file_system.h"
#include "conjure/editor/project_trust.h"
#include "editor/core/user_paths.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// project_trust.h (docs/design/04_editor.md D27): which projects the editor may open, for the session and in the
// trust store, and where that store lives. The gate that asks it is covered in test_editor_application.cpp, the
// commands in test_commands_project_trust.cpp.

using namespace conjure;
using namespace conjure::editor;

namespace {
    /// <summary>
    /// A folder under the scratch directory that stands for a project root (project_trust reads no project.json).
    /// </summary>
    [[nodiscard]] std::filesystem::path make_folder(const testing::temp_directory &directory, std::string_view name) {
        const std::filesystem::path folder = directory.file(name);
        std::filesystem::create_directories(folder);
        return folder;
    }

    void write_owner_only(const std::filesystem::path &file, std::string_view text) {
        CONJURE_REQUIRE_OK(
          core::vfs::write_private_file(file, std::as_bytes(std::span<const char>(text.data(), text.size())))
        );
    }

    /// <summary>
    /// The text of a trust store that names the given keys, oldest first.
    /// </summary>
    [[nodiscard]] std::string store_text(const std::vector<std::string> &keys) {
        core::json::document projects = core::json::document::array();
        for (const std::string &key : keys) {
            projects.push_back(core::json::object({
              {"path", key},
              {"trustedAt", "2026-11-17T09:30:00Z"},
              {"origin", "test"},
            }));
        }
        return core::json::object({{"schema_version", 1}, {"projects", projects}}).to_string();
    }
}

TEST_SUITE("editor/project_trust") {

    TEST_CASE("trust_state - the three states have wire names and grow with how long the answer lasts") {
        CHECK(to_string(trust_state::UNTRUSTED) == "untrusted");
        CHECK(to_string(trust_state::SESSION) == "session");
        CHECK(to_string(trust_state::REMEMBERED) == "remembered");
        CHECK(to_string(static_cast<trust_state>(200)).empty());
        CHECK(trust_state::UNTRUSTED < trust_state::SESSION);
        CHECK(trust_state::SESSION < trust_state::REMEMBERED);
    }

    TEST_CASE("project_trust::project_key - spellings of one folder give one absolute key with forward slashes") {
        const testing::temp_directory directory = {};
        const std::filesystem::path project = make_folder(directory, "games/demo");
        const std::string key = editor_test::project_key_of(project);
        CHECK(key.find('\\') == std::string::npos);
        CHECK(key.back() != '/');
        CHECK(std::filesystem::path(std::u8string(key.begin(), key.end())).is_absolute());
        CHECK(core::str::ends_with(key, "/games/demo"));

        SUBCASE("a trailing separator") {
            CHECK(editor_test::project_key_of(project / "") == key);
        }
        SUBCASE("dot and dot-dot segments") {
            CHECK(editor_test::project_key_of(directory.path() / "games" / "." / "other" / ".." / "demo") == key);
        }
        SUBCASE("another folder is another key, and so are a folder inside and the folder above") {
            CHECK(editor_test::project_key_of(make_folder(directory, "games/other")) != key);
            CHECK(editor_test::project_key_of(project / "assets") != key);
            CHECK(editor_test::project_key_of(project.parent_path()) != key);
        }
        SUBCASE("a folder that does not exist yet has a key as well") {
            const std::string missing = editor_test::project_key_of(directory.path() / "games" / "not_yet");
            CHECK(core::str::ends_with(missing, "/games/not_yet"));
        }
        SUBCASE("a relative path is taken against the working directory") {
            const std::string relative = editor_test::project_key_of(std::filesystem::path("relative_project"));
            CHECK(std::filesystem::path(std::u8string(relative.begin(), relative.end())).is_absolute());
            CHECK(core::str::ends_with(relative, "/relative_project"));
        }
    }

    TEST_CASE("project_trust::project_key - a symbolic link to a folder is that folder") {
        const testing::temp_directory directory = {};
        const std::filesystem::path project = make_folder(directory, "real");
        const std::filesystem::path link = directory.file("link");
        std::error_code linkError = {};
        std::filesystem::create_directory_symlink(project, link, linkError);
        if (linkError) {
            // Windows creates symbolic links only for administrators or in developer mode.
            MESSAGE("this account cannot create a symbolic link (" << linkError.message() << "): the case did not run");
            return;
        }
        CHECK(editor_test::project_key_of(link) == editor_test::project_key_of(project));
        // Trusted under one name, trusted under the other: the answer is about the folder.
        project_trust trust = {};
        CONJURE_REQUIRE_OK(trust.trust(link, false, "test"));
        CHECK(trust.state_of(project) == trust_state::SESSION);
    }

    TEST_CASE("project_trust::project_key - an empty path and one that is not UTF-8 have no key") {
        CONJURE_CHECK_ERROR(project_trust::project_key({}), core::error_code::INVALID_ARGUMENT);
        project_trust trust = {};
        CHECK(trust.state_of({}) == trust_state::UNTRUSTED);
        CHECK_FALSE(trust.is_trusted({}));
        CONJURE_CHECK_ERROR(trust.trust({}, false, "test"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(trust.revoke({}), core::error_code::INVALID_ARGUMENT);

        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            // A Windows path is UTF-16 and always has a UTF-8 form.
            MESSAGE("file names are not byte strings on Windows: the non-UTF-8 part did not run");
        } else {
            const testing::temp_directory directory = {};
            // Never created, and the answer must not depend on what the volume says about the name: looking it
            // up answers "not found" on Linux and "illegal byte sequence" on APFS, so the key is refused from
            // the text of the path before the file system is asked.
            const std::filesystem::path odd = directory.path() / std::filesystem::path(std::string("bad_\xff\xfe"));
            CONJURE_CHECK_ERROR(project_trust::project_key(odd), core::error_code::INVALID_ARGUMENT);
            CHECK(trust.state_of(odd) == trust_state::UNTRUSTED);
            CONJURE_CHECK_ERROR(trust.trust(odd, false, "test"), core::error_code::INVALID_ARGUMENT);
            CHECK_FALSE(trust.is_trusted(odd));
        }
    }

    TEST_CASE("project_trust - a project nobody answered for is untrusted, with a store and without") {
        const testing::temp_directory directory = {};
        const std::filesystem::path project = make_folder(directory, "project");
        project_trust trust = {};
        CHECK(trust.store_file().empty());
        CHECK(trust.state_of(project) == trust_state::UNTRUSTED);
        CHECK_FALSE(trust.is_trusted(project));
        CHECK(trust.remembered().empty());
        const std::filesystem::path store = directory.file("state/trusted_projects.json");
        trust.set_store_file(store);
        CHECK(trust.store_file() == store);
        CHECK(trust.state_of(project) == trust_state::UNTRUSTED);
        // Asking creates nothing: neither the file nor its folder.
        CHECK_FALSE(std::filesystem::exists(store.parent_path()));
    }

    TEST_CASE("project_trust - without a store every answer is for the session and nothing is written") {
        const testing::temp_directory directory = {};
        const std::filesystem::path project = make_folder(directory, "project");
        const std::filesystem::path other = make_folder(directory, "other");
        project_trust trust = {};
        const core::result<trust_state> answered = trust.trust(project, true, "test");
        CONJURE_REQUIRE_OK(answered);
        CHECK(answered.value() == trust_state::SESSION);
        CHECK(trust.state_of(project) == trust_state::SESSION);
        CHECK(trust.is_trusted(project));
        // The answer is for that project and that object: not for another folder, not for another editor.
        CHECK(trust.state_of(other) == trust_state::UNTRUSTED);
        CHECK(trust.state_of(project / "assets") == trust_state::UNTRUSTED);
        CHECK(trust.state_of(directory.path()) == trust_state::UNTRUSTED);
        const project_trust second = {};
        CHECK(second.state_of(project) == trust_state::UNTRUSTED);
        // Answering twice changes nothing.
        CONJURE_REQUIRE_OK(trust.trust(project, false, "test"));
        CHECK(trust.state_of(project) == trust_state::SESSION);
    }

    TEST_CASE("project_trust - a remembered answer is in the owner-only store and another editor reads it") {
        const testing::temp_directory directory = {};
        if (!editor_test::owner_only_files_available(directory.path())) {
            return;
        }
        const std::filesystem::path project = make_folder(directory, "project");
        const std::filesystem::path store = directory.file("state/conjure/trusted_projects.json");
        project_trust trust = {};
        trust.set_store_file(store);
        const core::result<trust_state> answered = trust.trust(project, true, "tcp");
        CONJURE_REQUIRE_OK(answered);
        CHECK(answered.value() == trust_state::REMEMBERED);
        CHECK(trust.state_of(project) == trust_state::REMEMBERED);

        // The folder of the store was created, and no other account may open the file.
        REQUIRE(std::filesystem::is_regular_file(store));
        const core::result<bool> ownerOnly = core::vfs::is_private_file(store);
        CONJURE_REQUIRE_OK(ownerOnly);
        CHECK(ownerOnly.value());
        const core::result<core::json::document> written = core::json::document::parse_file(store);
        CONJURE_REQUIRE_OK(written);
        const core::json::value root = written.value().root();
        CHECK(root["schema_version"].as_u64() == project_trust::STORE_SCHEMA_VERSION);
        REQUIRE(root["projects"].size() == 1);
        const core::json::value entry = *root["projects"].begin();
        CHECK(entry["path"].as_string() == editor_test::project_key_of(project));
        CHECK(entry["origin"].as_string() == "tcp");
        // "2026-11-17T09:30:00Z": the UTC form the audit log uses.
        const std::string_view trustedAt = entry["trustedAt"].as_string();
        REQUIRE(trustedAt.size() == 20);
        CHECK(trustedAt[10] == 'T');
        CHECK(trustedAt.back() == 'Z');

        project_trust second = {};
        second.set_store_file(store);
        CHECK(second.state_of(project) == trust_state::REMEMBERED);
        const std::vector<trusted_project> remembered = second.remembered();
        REQUIRE(remembered.size() == 1);
        CHECK(remembered.front().path == editor_test::project_key_of(project));
        CHECK(remembered.front().origin == "tcp");

        // Answered again: still one entry, with the newer origin, and an over-long origin is cut.
        const std::string longOrigin(project_trust::MAX_ORIGIN_BYTES + 40, 'x');
        CONJURE_REQUIRE_OK(second.trust(project, true, longOrigin));
        const std::vector<trusted_project> again = trust.remembered();
        REQUIRE(again.size() == 1);
        CHECK(again.front().origin == std::string(project_trust::MAX_ORIGIN_BYTES, 'x'));
    }

    TEST_CASE("project_trust - an answer that is not to be remembered stays out of the store") {
        const testing::temp_directory directory = {};
        const std::filesystem::path project = make_folder(directory, "project");
        const std::filesystem::path store = directory.file("trusted_projects.json");
        project_trust trust = {};
        trust.set_store_file(store);
        const core::result<trust_state> answered = trust.trust(project, false, "command line");
        CONJURE_REQUIRE_OK(answered);
        CHECK(answered.value() == trust_state::SESSION);
        CHECK_FALSE(std::filesystem::exists(store));
        project_trust second = {};
        second.set_store_file(store);
        CHECK(second.state_of(project) == trust_state::UNTRUSTED);
        // Naming another store keeps the session answers.
        trust.set_store_file(directory.file("elsewhere.json"));
        CHECK(trust.state_of(project) == trust_state::SESSION);
    }

    TEST_CASE("project_trust - revoke forgets the session answer and the remembered one") {
        const testing::temp_directory directory = {};
        const std::filesystem::path project = make_folder(directory, "project");
        const std::filesystem::path kept = make_folder(directory, "kept");
        project_trust trust = {};

        SUBCASE("a project nobody answered for") {
            const core::result<bool> revoked = trust.revoke(project);
            CONJURE_REQUIRE_OK(revoked);
            CHECK_FALSE(revoked.value());
        }
        SUBCASE("a session answer") {
            CONJURE_REQUIRE_OK(trust.trust(project, false, "test"));
            CONJURE_REQUIRE_OK(trust.trust(kept, false, "test"));
            const core::result<bool> revoked = trust.revoke(project);
            CONJURE_REQUIRE_OK(revoked);
            CHECK(revoked.value());
            CHECK(trust.state_of(project) == trust_state::UNTRUSTED);
            CHECK(trust.state_of(kept) == trust_state::SESSION);
            const core::result<bool> twice = trust.revoke(project);
            CONJURE_REQUIRE_OK(twice);
            CHECK_FALSE(twice.value());
        }
        SUBCASE("a remembered answer, also once the folder is gone") {
            if (!editor_test::owner_only_files_available(directory.path())) {
                return;
            }
            const std::filesystem::path store = directory.file("trusted_projects.json");
            trust.set_store_file(store);
            CONJURE_REQUIRE_OK(trust.trust(project, true, "test"));
            CONJURE_REQUIRE_OK(trust.trust(kept, true, "test"));
            // Also trusted for the session by another answer: both go.
            CONJURE_REQUIRE_OK(trust.trust(project, false, "test"));
            const std::string key = editor_test::project_key_of(project);
            std::error_code ignored = {};
            REQUIRE(std::filesystem::remove_all(project, ignored) > 0);
            const core::result<bool> revoked = trust.revoke(project);
            CONJURE_REQUIRE_OK(revoked);
            CHECK(revoked.value());
            CHECK(trust.state_of(project) == trust_state::UNTRUSTED);
            CHECK(trust.state_of(kept) == trust_state::REMEMBERED);
            const std::vector<trusted_project> remembered = trust.remembered();
            REQUIRE(remembered.size() == 1);
            CHECK(remembered.front().path != key);
            // The rewritten store is still nobody else's to open.
            const core::result<bool> ownerOnly = core::vfs::is_private_file(store);
            CONJURE_REQUIRE_OK(ownerOnly);
            CHECK(ownerOnly.value());
        }
    }

    TEST_CASE("project_trust - a store another account may have written is not believed, and is replaced") {
        testing::log_capture logs = {};
        const testing::temp_directory directory = {};
        const std::filesystem::path project = make_folder(directory, "project");
        const std::filesystem::path store = directory.file("trusted_projects.json");
        {
            // An ordinary file: on Windows it inherits the folder's access control list, on POSIX the mode set
            // below lets group and others read it. It names the project as trusted.
            std::ofstream planted(store, std::ios::binary | std::ios::trunc);
            REQUIRE(planted.is_open());
            planted << store_text({editor_test::project_key_of(project)});
        }
        std::error_code ignored = {};
        std::filesystem::permissions(
          store,
          std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
            std::filesystem::perms::group_read | std::filesystem::perms::others_read,
          ignored
        );
        project_trust trust = {};
        trust.set_store_file(store);
        CHECK(trust.state_of(project) == trust_state::UNTRUSTED);
        CHECK(trust.remembered().empty());
        CHECK(logs.contains(core::log_level::WARN, "is not used"));
        CHECK(logs.contains(core::log_level::WARN, "another account"));

        if (!editor_test::owner_only_files_available(directory.path())) {
            return;
        }
        // The next remembered answer puts an owner-only file in its place.
        const std::filesystem::path other = make_folder(directory, "other");
        const core::result<trust_state> answered = trust.trust(other, true, "test");
        CONJURE_REQUIRE_OK(answered);
        CHECK(answered.value() == trust_state::REMEMBERED);
        const core::result<bool> ownerOnly = core::vfs::is_private_file(store);
        CONJURE_REQUIRE_OK(ownerOnly);
        CHECK(ownerOnly.value());
        // What the planted file said did not survive.
        CHECK(trust.state_of(project) == trust_state::UNTRUSTED);
        CHECK(trust.remembered().size() == 1);
    }

    TEST_CASE("project_trust - a store that cannot be used counts as empty and the next answer replaces it") {
        testing::log_capture logs = {};
        const testing::temp_directory directory = {};
        if (!editor_test::owner_only_files_available(directory.path())) {
            return;
        }
        const std::filesystem::path project = make_folder(directory, "project");
        const std::string key = editor_test::project_key_of(project);
        const std::filesystem::path store = directory.file("trusted_projects.json");
        std::string content = "";
        std::string reason = "";
        SUBCASE("not JSON") {
            content = "{\"schema_version\": 1, \"projects\": [";
            reason = "not JSON";
        }
        SUBCASE("an empty file") {
            reason = "not JSON";
        }
        SUBCASE("JSON that is not a store") {
            content = std::format(R"([{{"path":"{}"}}])", key);
            reason = "not a trust store";
        }
        SUBCASE("a store without its list") {
            content = R"({"schema_version": 1})";
            reason = "not a trust store";
        }
        SUBCASE("a store of a newer editor") {
            content = std::format(R"({{"schema_version": 2, "projects": [{{"path":"{}"}}]}})", key);
            reason = "schema version 2";
        }
        SUBCASE("a store beyond the size limit") {
            content = store_text({key});
            content.append(project_trust::MAX_STORE_BYTES, ' ');
            reason = "more than";
        }
        CAPTURE(reason);
        write_owner_only(store, content);
        project_trust trust = {};
        trust.set_store_file(store);
        CHECK(trust.state_of(project) == trust_state::UNTRUSTED);
        CHECK(logs.contains(core::log_level::WARN, "is not used"));
        CHECK(logs.contains(core::log_level::WARN, reason));

        const core::result<trust_state> answered = trust.trust(project, true, "test");
        CONJURE_REQUIRE_OK(answered);
        CHECK(answered.value() == trust_state::REMEMBERED);
        const std::vector<trusted_project> remembered = trust.remembered();
        REQUIRE(remembered.size() == 1);
        CHECK(remembered.front().path == key);
    }

    TEST_CASE("project_trust - entries without a path are skipped and the others still count") {
        const testing::temp_directory directory = {};
        if (!editor_test::owner_only_files_available(directory.path())) {
            return;
        }
        const std::filesystem::path project = make_folder(directory, "project");
        const std::filesystem::path store = directory.file("trusted_projects.json");
        write_owner_only(
          store,
          std::format(
            R"({{"schema_version":1,"projects":[{{"origin":"nobody"}},17,{{"path":""}},{{"path":"{}"}}]}})",
            editor_test::project_key_of(project)
          )
        );
        project_trust trust = {};
        trust.set_store_file(store);
        CHECK(trust.state_of(project) == trust_state::REMEMBERED);
        const std::vector<trusted_project> remembered = trust.remembered();
        REQUIRE(remembered.size() == 1);
        CHECK(remembered.front().trustedAt.empty());
        CHECK(remembered.front().origin.empty());
    }

    TEST_CASE("project_trust - the store keeps the newest answers and drops the oldest beyond its limit") {
        const testing::temp_directory directory = {};
        if (!editor_test::owner_only_files_available(directory.path())) {
            return;
        }
        const std::filesystem::path store = directory.file("trusted_projects.json");
        // Entries need not name folders that exist: the store is a list of texts.
        std::vector<std::string> keys = {};
        for (usize index = 0; index < project_trust::MAX_REMEMBERED_PROJECTS + 5; ++index) {
            keys.push_back(std::format("/remembered/project_{}", index));
        }
        write_owner_only(store, store_text(keys));
        project_trust trust = {};
        trust.set_store_file(store);
        // A file with more entries than the limit (edited by hand) is read as its newest ones.
        std::vector<trusted_project> remembered = trust.remembered();
        REQUIRE(remembered.size() == project_trust::MAX_REMEMBERED_PROJECTS);
        CHECK(remembered.front().path == "/remembered/project_5");
        CHECK(remembered.back().path == keys.back());

        const std::filesystem::path project = make_folder(directory, "project");
        const core::result<trust_state> answered = trust.trust(project, true, "test");
        CONJURE_REQUIRE_OK(answered);
        CHECK(answered.value() == trust_state::REMEMBERED);
        remembered = trust.remembered();
        REQUIRE(remembered.size() == project_trust::MAX_REMEMBERED_PROJECTS);
        CHECK(remembered.front().path == "/remembered/project_6");
        CHECK(remembered.back().path == editor_test::project_key_of(project));
    }

    TEST_CASE("project_trust - an answer the store cannot take holds for the session and says why") {
        testing::log_capture logs = {};
        const testing::temp_directory directory = {};
        const std::filesystem::path project = make_folder(directory, "project");
        std::filesystem::path store = {};
        SUBCASE("a file where the folder of the store belongs") {
            CONJURE_REQUIRE_OK(directory.write_text("blocker", "in the way"));
            store = directory.file("blocker") / "trusted_projects.json";
        }
        SUBCASE("a folder where the store file belongs") {
            store = make_folder(directory, "trusted_projects.json");
        }
        project_trust trust = {};
        trust.set_store_file(store);
        const core::result<trust_state> answered = trust.trust(project, true, "test");
        // The caller said yes: that is kept, only not beyond this editor.
        CONJURE_REQUIRE_OK(answered);
        CHECK(answered.value() == trust_state::SESSION);
        CHECK(trust.state_of(project) == trust_state::SESSION);
        CHECK(logs.contains(core::log_level::WARN, "cannot be remembered"));
        project_trust second = {};
        second.set_store_file(store);
        CHECK(second.state_of(project) == trust_state::UNTRUSTED);
        // And it can be taken back like any session answer.
        const core::result<bool> revoked = trust.revoke(project);
        CONJURE_REQUIRE_OK(revoked);
        CHECK(revoked.value());
        CHECK(trust.state_of(project) == trust_state::UNTRUSTED);
    }

    TEST_CASE("regression - code sweep F187 project_trust an answer that would take the store past its size limit "
              "holds for the session and leaves the store as it was") {
        testing::log_capture logs = {};
        const testing::temp_directory directory = {};
        if (!editor_test::owner_only_files_available(directory.path())) {
            return;
        }
        const std::filesystem::path project = make_folder(directory, "project");
        const std::filesystem::path store = directory.file("trusted_projects.json");
        // A store that is a few bytes short of its limit: entries of very long paths, the last one cut so that
        // the file ends just under MAX_STORE_BYTES. Any further entry takes it past the limit.
        std::vector<std::string> keys = {};
        for (usize index = 0; index < 99; ++index) {
            keys.push_back(std::format("/remembered/{:02}/{}", index, std::string(40000, 'd')));
        }
        keys.emplace_back("/remembered/last/");
        const usize spare = project_trust::MAX_STORE_BYTES - 8 - store_text(keys).size();
        keys.back() += std::string(spare, 'd');
        const std::string text = store_text(keys);
        REQUIRE(text.size() == project_trust::MAX_STORE_BYTES - 8);
        write_owner_only(store, text);
        project_trust trust = {};
        trust.set_store_file(store);
        REQUIRE(trust.remembered().size() == keys.size());

        // Written all the same, the file would be one that is not read any more: every answer in it would be
        // lost, and this one would come back as "untrusted" although the caller said yes.
        const core::result<trust_state> answered = trust.trust(project, true, "test");
        CONJURE_REQUIRE_OK(answered);
        CHECK(answered.value() == trust_state::SESSION);
        CHECK(trust.state_of(project) == trust_state::SESSION);
        CHECK(logs.contains(core::log_level::WARN, "cannot be remembered"));
        CHECK(logs.contains(core::log_level::WARN, "more than the"));
        CHECK(editor_test::read_text_file(store) == text);
        CHECK(trust.remembered().size() == keys.size());
        project_trust second = {};
        second.set_store_file(store);
        CHECK(second.state_of(project) == trust_state::UNTRUSTED);
        CHECK(second.remembered().size() == keys.size());
        // The same answer is remembered once the store has room for it.
        write_owner_only(store, store_text({keys.front()}));
        const core::result<trust_state> again = trust.trust(project, true, "test");
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value() == trust_state::REMEMBERED);
        CHECK(second.state_of(project) == trust_state::REMEMBERED);
    }

    TEST_CASE("project_trust - revoke says when the store cannot be rewritten, and the project stays trusted there") {
        testing::log_capture logs = {};
        const testing::temp_directory directory = {};
        if (!editor_test::owner_only_files_available(directory.path())) {
            return;
        }
        const std::filesystem::path project = make_folder(directory, "project");
        const std::filesystem::path other = make_folder(directory, "other");
        // A folder of its own: on POSIX the blocker takes the write permission from the store's folder.
        const std::filesystem::path store = make_folder(directory, "state") / "trusted_projects.json";
        project_trust trust = {};
        trust.set_store_file(store);
        CONJURE_REQUIRE_OK(trust.trust(project, true, "test"));
        CONJURE_REQUIRE_OK(trust.trust(project, false, "test"));
        {
            const editor_test::replace_blocker blocker(store);
            if (!blocker.active()) {
                return;
            }
            const core::result<bool> revoked = trust.revoke(project);
            CONJURE_CHECK_ERROR(revoked, core::error_code::IO_FAILURE);
            REQUIRE_FALSE(revoked.has_value());
            CHECK(revoked.get_error().message().find("still trusted") != std::string::npos);
            // Not silently half done: the store still names the project, and that is what state_of reports.
            CHECK(trust.state_of(project) == trust_state::REMEMBERED);
            // An answer given meanwhile holds for the session, and says that it could not be kept.
            const core::result<trust_state> answered = trust.trust(other, true, "test");
            CONJURE_REQUIRE_OK(answered);
            CHECK(answered.value() == trust_state::SESSION);
            CHECK(logs.contains(core::log_level::WARN, "cannot be remembered"));
        }
        // Once the store can be written again the same call takes the answer back.
        const core::result<bool> revoked = trust.revoke(project);
        CONJURE_REQUIRE_OK(revoked);
        CHECK(revoked.value());
        CHECK(trust.state_of(project) == trust_state::UNTRUSTED);
        CHECK(trust.state_of(other) == trust_state::SESSION);
    }

    TEST_CASE("default_trust_store_file - names the store in an absolute per-user folder, or says why not") {
        const core::result<std::filesystem::path> store = default_trust_store_file();
        if (!store.has_value()) {
            // A bare environment (no HOME): nothing is remembered, which is what the caller is told.
            CONJURE_CHECK_ERROR(store, core::error_code::NOT_FOUND);
            MESSAGE("this account has no per-user state folder: " << store.get_error().message());
            return;
        }
        CHECK(store.value().is_absolute());
        CHECK(store.value().filename() == std::filesystem::path(project_trust::STORE_FILE_NAME));
        const std::string folder = store.value().parent_path().filename().generic_string();
        const std::string_view expected = CONJURE_PLATFORM_LINUX == 1 ? "conjure" : "Conjure";
        CHECK(folder == expected);
        const core::result<std::filesystem::path> directory = user_state_directory();
        CONJURE_REQUIRE_OK(directory);
        CHECK(directory.value() == store.value().parent_path());
    }

    TEST_CASE("posix_user_state_directory - XDG_STATE_HOME, then HOME, and only absolute values count") {
        struct expectation {
            std::string_view xdgStateHome = "";
            std::string_view home = "";
            bool macosLayout = false;
            std::string_view directory = "";
        };
        const std::vector<expectation> expectations = {
            {"", "/home/ada", false, "/home/ada/.local/state/conjure"},
            {"/var/state", "/home/ada", false, "/var/state/conjure"},
            {"/var/state//", "/home/ada", false, "/var/state/conjure"},
            // The specification tells a program to ignore a relative XDG value.
            {"state", "/home/ada", false, "/home/ada/.local/state/conjure"},
            {"", "/home/ada/", false, "/home/ada/.local/state/conjure"},
            {"", "/", false, "/.local/state/conjure"},
            {"/", "", false, "/conjure"},
            {"", "", false, ""},
            {"", "home/ada", false, ""},
            {"state", "", false, ""},
            {"", "/Users/ada", true, "/Users/ada/Library/Application Support/Conjure"},
            // macOS has one place for this; an XDG variable somebody exported is not looked at.
            {"/var/state", "/Users/ada", true, "/Users/ada/Library/Application Support/Conjure"},
            {"/var/state", "", true, ""},
            {"", "Users/ada", true, ""},
        };
        for (usize index = 0; index < expectations.size(); ++index) {
            CAPTURE(index);
            const expectation &expected = expectations[index];
            const std::string directory =
              posix_user_state_directory(expected.xdgStateHome, expected.home, expected.macosLayout);
            CHECK(directory == expected.directory);
        }
    }
}
