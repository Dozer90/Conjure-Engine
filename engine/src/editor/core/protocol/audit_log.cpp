#include "conjure/editor/protocol/audit_log.h"

#include "conjure/editor/log_category.h"

#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/vfs/native_file_system.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <system_error>

namespace conjure::editor {

    namespace {
        constexpr std::string_view TOKEN_MEMBER = "token";
        constexpr std::string_view CUT_MARK = "...";

        constexpr std::string_view OMITTED = "[omitted: more than {} bytes]";

        /// <summary>
        /// The length of the secret that starts at 'offset' of 'text', 0 when none does. Every secret is
        /// compared in full at every call (constant_time_starts_at), so the time taken says nothing about how
        /// much of a secret the text holds there.
        /// </summary>
        [[nodiscard]] usize secret_length_at(
          std::string_view text,
          usize offset,
          std::span<const std::string_view> secrets
        ) noexcept {
            usize length = 0;
            for (const std::string_view secret : secrets) {
                const bool here = constant_time_starts_at(text, offset, secret);
                if (here && length == 0) {
                    length = secret.size();
                }
            }
            return length;
        }

        /// <summary>
        /// 'text' with every secret in it replaced by the token mark, cut to 'maxBytes' on a code point
        /// boundary (a split character would be invalid UTF-8, which the JSON writer refuses) with "..." in
        /// place of what was cut. The text is read from its start and only as far as the kept part needs:
        /// a secret is replaced before anything of it is kept, so a cut cannot leave the head of one behind,
        /// and the work is bounded by 'maxBytes' and the secrets' lengths, whatever the length of the text.
        /// </summary>
        [[nodiscard]] std::string clean_text(
          std::string_view text,
          std::span<const std::string_view> secrets,
          usize maxBytes
        ) {
            std::string cleaned = {};
            cleaned.reserve(std::min(text.size(), maxBytes) + audit_log::TOKEN_MARK.size() + CUT_MARK.size());
            usize offset = 0;
            while (offset < text.size() && cleaned.size() <= maxBytes) {
                const usize secretLength = secret_length_at(text, offset, secrets);
                if (secretLength > 0) {
                    cleaned += audit_log::TOKEN_MARK;
                    offset += secretLength;
                    continue;
                }
                cleaned.push_back(text[offset]);
                ++offset;
            }
            if (cleaned.size() <= maxBytes) {
                return cleaned;
            }
            usize cut = maxBytes;
            while (cut > 0 && (static_cast<unsigned char>(cleaned[cut]) & 0xC0u) == 0x80u) {
                --cut;
            }
            cleaned.resize(cut);
            cleaned += CUT_MARK;
            return cleaned;
        }

        /// <summary>
        /// What cleaned_value has kept so far, counted in the bytes it will take at least once serialised.
        /// Past MAX_ARGS_BYTES the arguments are recorded as a note whatever else they hold, so the copy
        /// stops there: the work of a record does not grow with the request.
        /// </summary>
        struct cleaning_budget {
            usize bytes = 0;

            [[nodiscard]] bool spent() const noexcept {
                return bytes > audit_log::MAX_ARGS_BYTES;
            }
        };

        /// <summary>
        /// A copy of 'source' that is safe and small enough to record: texts cleaned, members named "token"
        /// withheld, long arrays, wide objects and deep nesting cut. Returns early, with a copy that must not
        /// be used, once 'budget' is spent.
        /// </summary>
        [[nodiscard]] core::json::document cleaned_value(
          const core::json::value &source,
          std::span<const std::string_view> secrets,
          usize depth,
          cleaning_budget &budget
        ) {
            if (source.is_string()) {
                const std::string cleaned = clean_text(source.as_string(), secrets, audit_log::MAX_TEXT_BYTES);
                budget.bytes += cleaned.size() + 2;
                const core::json::init_value text = cleaned;
                return text.to_document();
            }
            if (!source.is_object() && !source.is_array()) {
                budget.bytes += 1;
                return core::json::document::from_value(source);
            }
            if (depth >= audit_log::MAX_DEPTH) {
                budget.bytes += CUT_MARK.size() + 2;
                const core::json::init_value cut = CUT_MARK;
                return cut.to_document();
            }
            budget.bytes += 2;
            if (source.is_object()) {
                core::json::document object = core::json::document::object();
                usize kept = 0;
                for (const core::json::member entry : source.members()) {
                    if (budget.spent()) {
                        break;
                    }
                    if (kept == audit_log::MAX_OBJECT_MEMBERS) {
                        // Setting a member looks through the members already there, so an object copied whole
                        // would cost the square of its width.
                        object.set(CUT_MARK, std::string_view(std::format("{} more", source.size() - kept)));
                        break;
                    }
                    const std::string key = clean_text(entry.key, secrets, audit_log::MAX_TEXT_BYTES);
                    budget.bytes += key.size() + 3;
                    ++kept;
                    if (entry.key == TOKEN_MEMBER) {
                        budget.bytes += audit_log::WITHHELD.size() + 2;
                        object.set(key, audit_log::WITHHELD);
                        continue;
                    }
                    object.set(key, cleaned_value(entry.item, secrets, depth + 1, budget));
                }
                return object;
            }
            core::json::document array = core::json::document::array();
            usize index = 0;
            for (const core::json::value item : source) {
                if (budget.spent()) {
                    break;
                }
                if (index == audit_log::MAX_ARRAY_ITEMS) {
                    array.push_back(std::string_view(std::format("... {} more", source.size() - index)));
                    break;
                }
                array.push_back(cleaned_value(item, secrets, depth + 1, budget));
                ++index;
            }
            return array;
        }

        /// <summary>
        /// True when 'text' holds one of the secrets. Every secret is searched for, each with
        /// constant_time_contains.
        /// </summary>
        [[nodiscard]] bool contains_secret(std::string_view text, std::span<const std::string_view> secrets) noexcept {
            bool found = false;
            for (const std::string_view secret : secrets) {
                const bool here = constant_time_contains(text, secret);
                found = found || here;
            }
            return found;
        }

        /// <summary>
        /// True when a regular file is at 'path' itself: a symbolic link is not followed and is not one.
        /// </summary>
        [[nodiscard]] bool is_regular_file_itself(const std::filesystem::path &path) {
            std::error_code failure = {};
            const std::filesystem::file_status status = std::filesystem::symlink_status(path, failure);
            return !failure && std::filesystem::is_regular_file(status);
        }
    }

    audit_log::audit_log() = default;

    audit_log::~audit_log() {
        close();
    }

    void audit_log::set_directory(const std::filesystem::path &directory) {
        close();
        mDirectory = directory;
        mSize = 0;
        mFailed = false;
    }

    std::filesystem::path audit_log::file() const {
        if (mDirectory.empty()) {
            return {};
        }
        return mDirectory / std::filesystem::path(FILE_NAME);
    }

    std::filesystem::path audit_log::previous_file() const {
        if (mDirectory.empty()) {
            return {};
        }
        return mDirectory / std::filesystem::path(PREVIOUS_FILE_NAME);
    }

    void audit_log::set_max_file_bytes(usize bytes) {
        // Closed, so that the next record opens the log again and applies the new limit to the files that
        // are there.
        close();
        mMaxFileBytes = std::max(bytes, MAX_LINE_BYTES);
    }

    void audit_log::record(const audit_entry &entry, std::span<const std::string_view> secrets) {
        if (mDirectory.empty() || mFailed) {
            return;
        }
        if (!mStream.is_open() && !open()) {
            return;
        }
        std::string line = format_line(entry, secrets, std::chrono::system_clock::now());
        line += '\n';
        if (mSize + line.size() > mMaxFileBytes && !rotate()) {
            return;
        }
        mStream.write(line.data(), static_cast<std::streamsize>(line.size()));
        if (!mStream) {
            fail("a write failed");
            return;
        }
        mSize += line.size();
        ++mRecordedCount;
    }

    void audit_log::flush() {
        if (!mStream.is_open()) {
            return;
        }
        mStream.flush();
        if (!mStream) {
            fail("a write failed");
        }
    }

    void audit_log::close() {
        if (mStream.is_open()) {
            mStream.close();
        }
    }

    std::string audit_log::format_line(
      const audit_entry &entry,
      std::span<const std::string_view> secrets,
      std::chrono::system_clock::time_point time
    ) {
        core::json::document line = core::json::object({
          {"time", format_time(time)},
          {"clientId", entry.clientId},
          {"transport", to_string(entry.transport)},
          {"access", to_string(entry.access)},
          {"cmd", clean_text(entry.command, secrets, MAX_COMMAND_BYTES)},
        });
        cleaning_budget budget = {};
        const core::json::document args =
          entry.args.is_valid() ? cleaned_value(entry.args, secrets, 0, budget) : core::json::document::object();
        const std::string argsText = budget.spent() ? std::string() : args.to_string();
        if (budget.spent() || argsText.size() > MAX_ARGS_BYTES) {
            // Decided before the search below, which is therefore never handed more than MAX_ARGS_BYTES.
            line.set("args", std::string_view(std::format(OMITTED, MAX_ARGS_BYTES)));
        } else if (contains_secret(argsText, secrets)) {
            // What the text cleaning cannot reach: a number that spells an all-digit token, for one.
            line.set("args", WITHHELD);
        } else {
            line.set("args", args);
        }
        line.set("ok", entry.ok);
        if (!entry.ok) {
            line.set("error", to_string(entry.error));
        }
        if (entry.async) {
            line.set("async", true);
        }
        if (entry.batchChild) {
            line.set("batch", true);
        }
        return line.to_string();
    }

    std::string audit_log::format_time(std::chrono::system_clock::time_point time) {
        // The calendar types and no chrono formatter: the formatters arrived late in two of the three
        // standard libraries the engine builds with.
        const std::chrono::sys_seconds seconds = std::chrono::floor<std::chrono::seconds>(time);
        const std::chrono::sys_days day = std::chrono::floor<std::chrono::days>(seconds);
        const std::chrono::year_month_day date = std::chrono::year_month_day(day);
        const std::chrono::hh_mm_ss<std::chrono::seconds> timeOfDay = std::chrono::hh_mm_ss(seconds - day);
        return std::format(
          "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
          static_cast<int>(date.year()),
          static_cast<unsigned>(date.month()),
          static_cast<unsigned>(date.day()),
          timeOfDay.hours().count(),
          timeOfDay.minutes().count(),
          timeOfDay.seconds().count()
        );
    }

    bool audit_log::open() {
        std::error_code ignored = {};
        std::filesystem::create_directories(mDirectory, ignored);
        // Files beyond the limit were not written under it: keeping them would break the bound.
        for (const std::filesystem::path &candidate : {file(), previous_file()}) {
            if (!is_regular_file_itself(candidate)) {
                continue;
            }
            std::error_code unreadable = {};
            const std::uintmax_t candidateSize = std::filesystem::file_size(candidate, unreadable);
            if (!unreadable && candidateSize > mMaxFileBytes) {
                std::filesystem::remove(candidate, ignored);
            }
        }
        const std::filesystem::path current = file();
        if (!is_regular_file_itself(current)) {
            return start_file(current);
        }
        // A file others may open (one another account made, or a hard link to one) must not receive the records:
        // an owner-only file takes its place, and its records go with it. Only a volume without owner-only
        // files, where write_private_file writes nothing, keeps appending to what it has.
        const core::result<bool> isPrivate = core::vfs::is_private_file(current);
        if (!isPrivate.has_value() || !isPrivate.value()) {
            const core::result<void> replaced = core::vfs::write_private_file(current, std::span<const std::byte>());
            if (replaced.has_value()) {
                CONJURE_LOG_WARN(
                  LOG_EDITOR,
                  "ai protocol: the audit log '{}' could be opened by other users; a new one took its place",
                  current.generic_string()
                );
            } else if (replaced.get_error().generic_code() != core::error_code::UNSUPPORTED) {
                fail(replaced.get_error().message());
                return false;
            }
        }
        std::error_code unreadable = {};
        const std::uintmax_t size = std::filesystem::file_size(current, unreadable);
        mStream.open(current, std::ios::binary | std::ios::app);
        if (unreadable || !mStream.is_open()) {
            // Without its size the file cannot be kept within the limit.
            fail("the file cannot be opened");
            return false;
        }
        mSize = static_cast<usize>(size);
        return true;
    }

    bool audit_log::start_file(const std::filesystem::path &target) {
        // An empty owner-only file that takes the place of whatever is at the path: nothing, or a symbolic
        // link someone planted there, which must not be written through.
        const core::result<void> created = core::vfs::write_private_file(target, std::span<const std::byte>());
        if (!created.has_value()) {
            if (created.get_error().generic_code() != core::error_code::UNSUPPORTED) {
                fail(created.get_error().message());
                return false;
            }
            // A volume without owner-only files: an ordinary file is what it has. remove() takes a link
            // itself away, never its target.
            std::error_code ignored = {};
            std::filesystem::remove(target, ignored);
        }
        mStream.open(target, std::ios::binary | std::ios::app);
        if (!mStream.is_open()) {
            fail("the file cannot be opened");
            return false;
        }
        mSize = 0;
        return true;
    }

    bool audit_log::rotate() {
        close();
        const std::filesystem::path current = file();
        std::error_code failure = {};
        std::filesystem::rename(current, previous_file(), failure);
        if (!failure) {
            return start_file(current);
        }
        // The move was refused: a folder has the other name, or a program holds one of the two files open in a
        // way that forbids it (a reader on Windows). The bound matters more than these records, so the full
        // file is emptied where it is; that needs no move, and the file keeps the permissions it has.
        mStream.open(current, std::ios::binary | std::ios::trunc);
        if (!mStream.is_open()) {
            fail("the full file can neither be moved nor emptied");
            return false;
        }
        mSize = 0;
        return true;
    }

    void audit_log::fail(std::string_view reason) {
        close();
        mFailed = true;
        CONJURE_LOG_WARN(
          LOG_EDITOR,
          "ai protocol: the audit log '{}' cannot be written ({}); commands are not recorded until a project "
          "is opened",
          file().generic_string(),
          reason
        );
    }
}
