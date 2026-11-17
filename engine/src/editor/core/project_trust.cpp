#include "conjure/editor/project_trust.h"

#include "editor/core/user_paths.h"

#include "conjure/editor/log_category.h"
#include "conjure/editor/protocol/audit_log.h"

#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/native_file_system.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <span>
#include <system_error>
#include <utility>

namespace conjure::editor {

    namespace {
        /// <summary>
        /// The path as UTF-8 with forward slashes. Through char8_t: a narrow string would be written in the ANSI
        /// code page on Windows.
        /// </summary>
        [[nodiscard]] std::string utf8_text(const std::filesystem::path &path) {
            const std::u8string text = path.generic_u8string();
            return std::string(text.begin(), text.end());
        }

        [[nodiscard]] bool holds(const std::vector<trusted_project> &projects, const std::string &key) {
            return std::ranges::any_of(projects, [&key](const trusted_project &project) {
                return project.path == key;
            });
        }
    }

    std::string_view to_string(trust_state state) noexcept {
        switch (state) {
            case trust_state::UNTRUSTED:
                return "untrusted";
            case trust_state::SESSION:
                return "session";
            case trust_state::REMEMBERED:
                return "remembered";
        }
        return "";
    }

    project_trust::project_trust() = default;

    project_trust::~project_trust() = default;

    void project_trust::set_store_file(std::filesystem::path file) {
        mStoreFile = std::move(file);
    }

    core::result<std::string> project_trust::project_key(const std::filesystem::path &projectRoot) {
        if (projectRoot.empty()) {
            return core::error(core::error_code::INVALID_ARGUMENT, "a project path is empty");
        }
        if (!core::str::is_valid_utf8(utf8_text(projectRoot))) {
            // Before the file system is asked anything: what a volume answers about such a name differs (not
            // found on one, an illegal byte sequence on another), and the answer here must not.
            return core::error(core::error_code::INVALID_ARGUMENT, "the project path is not valid UTF-8");
        }
        std::error_code failure = {};
        const std::filesystem::path absolute = std::filesystem::absolute(projectRoot, failure);
        if (failure) {
            return core::error(
              core::error_code::IO_FAILURE,
              std::format("the project path cannot be made absolute: {}", failure.message())
            );
        }
        // Resolved by the file system, not lexically: a link to a trusted folder is that folder, and a folder
        // reached through a link somebody retargeted is not the one that was trusted.
        std::filesystem::path resolved = std::filesystem::weakly_canonical(absolute, failure);
        if (failure) {
            return core::error(
              core::error_code::IO_FAILURE,
              std::format("the project path cannot be resolved: {}", failure.message())
            );
        }
        if (!resolved.has_filename() && resolved.has_relative_path()) {
            // "games/demo/" names the folder "games/demo".
            resolved = resolved.parent_path();
        }
        std::string key = utf8_text(resolved);
        if (!core::str::is_valid_utf8(key)) {
            // The store is JSON, and every answer of the protocol that names the project is as well. The path
            // as given passed this check; the working directory or the target of a link in it does not.
            return core::error(core::error_code::INVALID_ARGUMENT, "the project path is not valid UTF-8");
        }
        return key;
    }

    trust_state project_trust::state_of(const std::filesystem::path &projectRoot) const {
        const core::result<std::string> key = project_key(projectRoot);
        if (!key.has_value()) {
            return trust_state::UNTRUSTED;
        }
        return state_of_key(key.value());
    }

    bool project_trust::is_trusted(const std::filesystem::path &projectRoot) const {
        return state_of(projectRoot) != trust_state::UNTRUSTED;
    }

    core::result<trust_state> project_trust::trust(
      const std::filesystem::path &projectRoot,
      bool remember,
      std::string_view origin
    ) {
        CONJURE_TRY_ASSIGN(const std::string key, project_key(projectRoot));
        if (!remember || mStoreFile.empty()) {
            trust_for_session(key);
            return state_of_key(key);
        }
        std::vector<trusted_project> projects = remembered();
        std::erase_if(projects, [&key](const trusted_project &project) {
            return project.path == key;
        });
        trusted_project entry = {};
        entry.path = key;
        entry.trustedAt = audit_log::format_time(std::chrono::system_clock::now());
        entry.origin = std::string(origin.substr(0, MAX_ORIGIN_BYTES));
        if (!core::str::is_valid_utf8(entry.origin)) {
            entry.origin.clear();
        }
        projects.push_back(std::move(entry));
        if (projects.size() > MAX_REMEMBERED_PROJECTS) {
            const usize surplus = projects.size() - MAX_REMEMBERED_PROJECTS;
            projects.erase(projects.begin(), projects.begin() + static_cast<std::ptrdiff_t>(surplus));
        }
        const core::result<void> written = write_store(projects);
        if (!written.has_value()) {
            // The answer was "yes": it holds for this session even though it cannot be kept.
            CONJURE_LOG_WARN(
              LOG_EDITOR,
              "project trust: the answer for '{}' cannot be remembered in '{}' ({}); it holds until the editor exits",
              key,
              utf8_text(mStoreFile),
              written.get_error().message()
            );
            trust_for_session(key);
            return state_of_key(key);
        }
        const trust_state state = state_of_key(key);
        if (state != trust_state::UNTRUSTED) {
            return state;
        }
        // The store took the answer and no longer shows it: another editor of this user wrote the file in
        // between (the file comment of project_trust.h). The answer was "yes" all the same.
        CONJURE_LOG_WARN(
          LOG_EDITOR,
          "project trust: the answer for '{}' was written to '{}' and is not there any more; it holds until the "
          "editor exits",
          key,
          utf8_text(mStoreFile)
        );
        trust_for_session(key);
        return trust_state::SESSION;
    }

    core::result<bool> project_trust::revoke(const std::filesystem::path &projectRoot) {
        CONJURE_TRY_ASSIGN(const std::string key, project_key(projectRoot));
        const bool inSession = std::erase(mSession, key) > 0;
        std::vector<trusted_project> projects = remembered();
        const usize removed = std::erase_if(projects, [&key](const trusted_project &project) {
            return project.path == key;
        });
        if (removed == 0) {
            return inSession;
        }
        const core::result<void> written = write_store(projects);
        if (!written.has_value()) {
            return core::error(
              core::error_code::IO_FAILURE,
              std::format(
                "the trust store '{}' cannot be rewritten ({}); '{}' is still trusted there",
                utf8_text(mStoreFile),
                written.get_error().message(),
                key
              )
            );
        }
        return true;
    }

    std::vector<trusted_project> project_trust::remembered() const {
        core::result<std::vector<trusted_project>> projects = read_store();
        if (!projects.has_value()) {
            CONJURE_LOG_WARN(
              LOG_EDITOR,
              "project trust: the trust store '{}' is not used ({}); the projects it names are asked about again",
              utf8_text(mStoreFile),
              projects.get_error().message()
            );
            return {};
        }
        return std::move(projects).value();
    }

    trust_state project_trust::state_of_key(const std::string &key) const {
        if (holds(remembered(), key)) {
            return trust_state::REMEMBERED;
        }
        if (std::find(mSession.begin(), mSession.end(), key) != mSession.end()) {
            return trust_state::SESSION;
        }
        return trust_state::UNTRUSTED;
    }

    core::result<std::vector<trusted_project>> project_trust::read_store() const {
        std::vector<trusted_project> projects = {};
        if (mStoreFile.empty()) {
            return projects;
        }
        // Believed only when no other account could have written it: an entry planted in the file would open a
        // project without the question.
        const core::result<bool> ownerOnly = core::vfs::is_private_file(mStoreFile);
        if (!ownerOnly.has_value()) {
            if (ownerOnly.get_error().generic_code() == core::error_code::NOT_FOUND) {
                return projects;
            }
            return ownerOnly.get_error();
        }
        if (!ownerOnly.value()) {
            return core::error(
              core::error_code::PERMISSION_DENIED,
              "it is not a file that only this user may open, so another account may have written it"
            );
        }
        std::error_code failure = {};
        const std::uintmax_t size = std::filesystem::file_size(mStoreFile, failure);
        if (failure) {
            return core::error(
              core::error_code::IO_FAILURE,
              std::format("its size cannot be read: {}", failure.message())
            );
        }
        if (size > MAX_STORE_BYTES) {
            return core::error(
              core::error_code::OUT_OF_RANGE,
              std::format("it holds {} bytes, more than the {} a trust store may have", size, MAX_STORE_BYTES)
            );
        }
        std::string text(static_cast<usize>(size), '\0');
        std::ifstream stream(mStoreFile, std::ios::binary);
        stream.read(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream.is_open() || static_cast<usize>(stream.gcount()) != text.size()) {
            return core::error(core::error_code::IO_FAILURE, "it cannot be read");
        }
        const core::result<core::json::document> parsed = core::json::document::parse(text);
        if (!parsed.has_value()) {
            return core::error(
              core::error_code::PARSE_FAILURE,
              std::format("it is not JSON: {}", parsed.get_error().message())
            );
        }
        const core::json::value root = parsed.value().root();
        const core::json::value version = root["schema_version"];
        const core::json::value entries = root["projects"];
        if (!root.is_object() || !version.is_integer() || !entries.is_array()) {
            return core::error(core::error_code::PARSE_FAILURE, "it is not a trust store (schema_version, projects)");
        }
        if (version.as_u64() != STORE_SCHEMA_VERSION) {
            return core::error(
              core::error_code::UNSUPPORTED,
              std::format(
                "it has schema version {} and this editor reads version {}",
                version.as_i64(),
                STORE_SCHEMA_VERSION
              )
            );
        }
        for (const core::json::value entry : entries) {
            const std::string_view path = entry["path"].as_string();
            if (path.empty()) {
                // An entry without a path names no project; the others are still good.
                continue;
            }
            trusted_project project = {};
            project.path = std::string(path);
            project.trustedAt = std::string(entry["trustedAt"].as_string());
            project.origin = std::string(entry["origin"].as_string());
            projects.push_back(std::move(project));
        }
        if (projects.size() > MAX_REMEMBERED_PROJECTS) {
            // A file edited by hand: the bound is kept by reading its newest entries, as a write would keep them.
            const usize surplus = projects.size() - MAX_REMEMBERED_PROJECTS;
            projects.erase(projects.begin(), projects.begin() + static_cast<std::ptrdiff_t>(surplus));
        }
        return projects;
    }

    core::result<void> project_trust::write_store(const std::vector<trusted_project> &projects) const {
        core::json::document entries = core::json::document::array();
        for (const trusted_project &project : projects) {
            entries.push_back(core::json::object({
              {"path", project.path},
              {"trustedAt", project.trustedAt},
              {"origin", project.origin},
            }));
        }
        const core::json::document root = core::json::object({
          {"schema_version", STORE_SCHEMA_VERSION},
          {"projects", entries},
        });
        std::string text = root.to_string(true);
        if (text.empty()) {
            // The writer refuses a string that is not UTF-8; an empty file must not take the store's place.
            return core::error(core::error_code::INVALID_ARGUMENT, "an entry is not valid UTF-8");
        }
        text += '\n';
        if (text.size() > MAX_STORE_BYTES) {
            // read_store would refuse the file, and every answer in it would be asked for again.
            return core::error(
              core::error_code::OUT_OF_RANGE,
              std::format(
                "it would hold {} bytes, more than the {} a trust store may have",
                text.size(),
                MAX_STORE_BYTES
              )
            );
        }
        const std::filesystem::path folder = mStoreFile.parent_path();
        if (!folder.empty()) {
            std::error_code failure = {};
            std::filesystem::create_directories(folder, failure);
            if (failure) {
                return core::error(
                  core::error_code::IO_FAILURE,
                  std::format("its folder cannot be created: {}", failure.message())
                );
            }
        }
        const std::span<const char> characters(text.data(), text.size());
        return core::vfs::write_private_file(mStoreFile, std::as_bytes(characters));
    }

    void project_trust::trust_for_session(const std::string &key) {
        if (std::find(mSession.begin(), mSession.end(), key) == mSession.end()) {
            mSession.push_back(key);
        }
    }

    core::result<std::filesystem::path> default_trust_store_file() {
        CONJURE_TRY_ASSIGN(const std::filesystem::path directory, user_state_directory());
        return directory / std::filesystem::path(project_trust::STORE_FILE_NAME);
    }
}
