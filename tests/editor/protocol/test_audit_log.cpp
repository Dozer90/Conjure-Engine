#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/vfs/native_file_system.h"
#include "conjure/editor/protocol/audit_log.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// audit_log.h: what a record holds, what it never holds (tokens), how a line and the files are bounded, and
// what happens to the things that can be in the way of the file.

using namespace conjure;
using namespace conjure::editor;

namespace {
    constexpr std::string_view FULL_TOKEN = "full-7f3a9c41d2";
    constexpr std::string_view INSPECT_TOKEN = "look-0b6e55a8c1";
    constexpr std::array<std::string_view, 2> SECRETS = {FULL_TOKEN, INSPECT_TOKEN};
    // What a record holds in place of arguments that are too long (MAX_ARGS_BYTES is 2 KiB).
    constexpr std::string_view OMITTED_NOTE = "[omitted: more than 2048 bytes]";

    /// <summary>
    /// What the arguments of an oversized request are recorded as.
    /// </summary>
    enum class bounded_form : u8 {
        FIRST_MEMBERS,      // an object cut to its first members
        NOTE,               // the note that they were too long
        CUT_TEXT            // a string cut to its first bytes
    };

    [[nodiscard]] std::chrono::system_clock::time_point time_at(i64 secondsSinceEpoch) {
        return std::chrono::system_clock::time_point(std::chrono::seconds(secondsSinceEpoch));
    }

    /// <summary>
    /// An entry of a TCP client with full control; the arguments are views into 'args', which must outlive it.
    /// </summary>
    [[nodiscard]] audit_entry entry_of(std::string_view command, const core::json::document &args) {
        audit_entry entry = {};
        entry.command = command;
        entry.args = args.root();
        entry.clientId = 3;
        entry.transport = transport_kind::TCP;
        entry.access = access_level::FULL_CONTROL;
        return entry;
    }

    [[nodiscard]] core::json::document parsed(std::string_view line) {
        core::result<core::json::document> document = core::json::document::parse(line);
        REQUIRE_MESSAGE(document.has_value(), testing::describe_error(document));
        return std::move(document).value();
    }

    /// <summary>
    /// format_line with the two test tokens as secrets, parsed back.
    /// </summary>
    [[nodiscard]] core::json::document formatted(const audit_entry &entry) {
        const std::string line = audit_log::format_line(entry, SECRETS, time_at(0));
        CHECK(line.size() <= audit_log::MAX_LINE_BYTES);
        CHECK(line.find('\n') == std::string::npos);
        return parsed(line);
    }

    [[nodiscard]] usize size_of(const std::filesystem::path &file) {
        std::error_code failure = {};
        const std::uintmax_t size = std::filesystem::file_size(file, failure);
        REQUIRE_FALSE(failure);
        return static_cast<usize>(size);
    }

    /// <summary>
    /// Writes 'text' to 'file' as the editor would have: owner-only, so that the log keeps it when it opens;
    /// an ordinary file on a volume that keeps no owner-only files, where the log keeps that too.
    /// </summary>
    void write_kept_file(const std::filesystem::path &file, std::string_view text) {
        const core::result<void> written =
          core::vfs::write_private_file(file, std::as_bytes(std::span<const char>(text.data(), text.size())));
        if (written.has_value()) {
            return;
        }
        REQUIRE(written.get_error().generic_code() == core::error_code::UNSUPPORTED);
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        REQUIRE(stream.good());
    }

    /// <summary>
    /// Records 'count' commands named "<prefix>.<index>" and flushes after each, checking the bound on both
    /// files every time.
    /// </summary>
    void record_many(audit_log &log, std::string_view prefix, u32 count) {
        const core::json::document args = core::json::object({{"entity", "Player"}, {"enabled", true}});
        for (u32 index = 0; index < count; ++index) {
            CAPTURE(index);
            const std::string command = std::format("{}.{}", prefix, index);
            log.record(entry_of(command, args), SECRETS);
            log.flush();
            REQUIRE(size_of(log.file()) <= log.max_file_bytes());
            if (std::filesystem::exists(log.previous_file())) {
                REQUIRE(size_of(log.previous_file()) <= log.max_file_bytes());
            }
        }
    }
}

TEST_SUITE("editor/protocol/audit_log") {

    TEST_CASE("audit_log - format_time writes UTC to the second") {
        CHECK(audit_log::format_time(time_at(0)) == "1970-01-01T00:00:00Z");
        CHECK(audit_log::format_time(time_at(1794906902)) == "2026-11-17T09:15:02Z");
        SUBCASE("the last second of a leap day, and the day after a February without one") {
            CHECK(audit_log::format_time(time_at(951868799)) == "2000-02-29T23:59:59Z");
            CHECK(audit_log::format_time(time_at(4107542400)) == "2100-03-01T00:00:00Z");
        }
        SUBCASE("a fraction of a second is dropped, also before 1970") {
            const std::chrono::system_clock::time_point late = time_at(59) + std::chrono::milliseconds(999);
            CHECK(audit_log::format_time(late) == "1970-01-01T00:00:59Z");
            const std::chrono::system_clock::time_point early = time_at(-1) + std::chrono::milliseconds(500);
            CHECK(audit_log::format_time(early) == "1969-12-31T23:59:59Z");
        }
    }

    TEST_CASE("audit_log - format_line names the time, the client, its transport and access, the command and the "
              "outcome") {
        const core::json::document args = core::json::object({{"entity", "Player"}, {"count", 3}});
        audit_entry entry = entry_of("entity.get", args);

        SUBCASE("a command that succeeded") {
            entry.access = access_level::INSPECT;
            const std::string line = audit_log::format_line(entry, SECRETS, time_at(1794906902));
            CHECK(
              line == R"({"time":"2026-11-17T09:15:02Z","clientId":3,"transport":"tcp","access":"inspect",)"
                      R"("cmd":"entity.get","args":{"entity":"Player","count":3},"ok":true})"
            );
        }
        SUBCASE("a command that failed carries the code's name") {
            entry.ok = false;
            entry.error = protocol_error_code::UNAUTHORISED;
            const core::json::document record = formatted(entry);
            CHECK_FALSE(record.root()["ok"].as_bool(true));
            CHECK(record.root()["error"].as_string() == "UNAUTHORISED");
            CHECK_FALSE(record.root().contains("async"));
            CHECK_FALSE(record.root().contains("batch"));
        }
        SUBCASE("an asynchronous command has no outcome yet") {
            entry.async = true;
            const core::json::document record = formatted(entry);
            CHECK(record.root()["ok"].as_bool(false));
            CHECK(record.root()["async"].as_bool(false));
            CHECK_FALSE(record.root().contains("error"));
        }
        SUBCASE("a child of a batch is marked") {
            entry.batchChild = true;
            entry.transport = transport_kind::LOOPBACK;
            const core::json::document record = formatted(entry);
            CHECK(record.root()["batch"].as_bool(false));
            CHECK(record.root()["transport"].as_string() == "loopback");
            CHECK(record.root()["access"].as_string() == "full_control");
        }
        SUBCASE("a request without arguments records an empty object") {
            entry.args = core::json::value();
            CHECK(formatted(entry).root()["args"].is_object());
            CHECK(formatted(entry).root()["args"].size() == 0);
        }
    }

    TEST_CASE("audit_log - format_line never writes the value of a member named token") {
        SUBCASE("session.hello, right or wrong token") {
            const std::array<std::string_view, 3> presented = {FULL_TOKEN, INSPECT_TOKEN, "a-mistyped-token"};
            for (const std::string_view token : presented) {
                CAPTURE(token);
                const core::json::document args = core::json::object({{"token", token}, {"clientName", "agent"}});
                const std::string line = audit_log::format_line(entry_of("session.hello", args), SECRETS, time_at(0));
                CHECK(line.find(token) == std::string::npos);
                const core::json::document record = parsed(line);
                CHECK(record.root()["args"]["token"].as_string() == audit_log::WITHHELD);
                CHECK(record.root()["args"]["clientName"].as_string() == "agent");
            }
        }
        SUBCASE("under another command name, nested, and when it is not a string") {
            const core::json::document args = core::json::object({
              {"token", 12345},
              {"nested", core::json::object({{"token", "a-mistyped-token"}})},
              {"list", core::json::array({core::json::object({{"token", core::json::array({"deep-token"})}})})},
            });
            const std::string line = audit_log::format_line(entry_of("sesion.helo", args), SECRETS, time_at(0));
            CHECK(line.find("12345") == std::string::npos);
            CHECK(line.find("a-mistyped-token") == std::string::npos);
            CHECK(line.find("deep-token") == std::string::npos);
            const core::json::document record = parsed(line);
            CHECK(record.root()["cmd"].as_string() == "sesion.helo");
            CHECK(record.root()["args"]["token"].as_string() == audit_log::WITHHELD);
            CHECK(record.root()["args"]["nested"]["token"].as_string() == audit_log::WITHHELD);
            CHECK(record.root()["args"]["list"][0]["token"].as_string() == audit_log::WITHHELD);
        }
    }

    TEST_CASE("audit_log - format_line keeps a token out of every text of the request") {
        const std::string full = std::string(FULL_TOKEN);
        const std::string inspect = std::string(INSPECT_TOKEN);

        SUBCASE("in a string value, alone or inside other text, either token") {
            const core::json::document args = core::json::object({
              {"clientName", full},
              {"note", "my token is " + inspect + ", twice: " + inspect},
              {"list", core::json::array({full})},
            });
            const std::string line = audit_log::format_line(entry_of("session.ping", args), SECRETS, time_at(0));
            CHECK(line.find(full) == std::string::npos);
            CHECK(line.find(inspect) == std::string::npos);
            const core::json::document record = parsed(line);
            CHECK(record.root()["args"]["clientName"].as_string() == audit_log::TOKEN_MARK);
            CHECK(record.root()["args"]["note"].as_string() == "my token is [token], twice: [token]");
            CHECK(record.root()["args"]["list"][0].as_string() == audit_log::TOKEN_MARK);
        }
        SUBCASE("in a key") {
            core::json::document args = core::json::document::object();
            args.set("key-" + full, "value");
            const std::string line = audit_log::format_line(entry_of("session.ping", args), SECRETS, time_at(0));
            CHECK(line.find(full) == std::string::npos);
            CHECK(parsed(line).root()["args"]["key-[token]"].as_string() == "value");
        }
        SUBCASE("in the command name") {
            const core::json::document args = core::json::document::object();
            const std::string command = "session.hello " + full;
            const std::string line = audit_log::format_line(entry_of(command, args), SECRETS, time_at(0));
            CHECK(line.find(full) == std::string::npos);
            CHECK(parsed(line).root()["cmd"].as_string() == "session.hello [token]");
        }
        SUBCASE("a token that straddles the place where a long text is cut leaves no part behind") {
            // The first six bytes of the token sit just before the cut: cutting first would keep them.
            const std::string padding(audit_log::MAX_TEXT_BYTES - 6, 'a');
            const core::json::document args = core::json::object({{"note", padding + full + "tail"}});
            const std::string line = audit_log::format_line(entry_of("session.ping", args), SECRETS, time_at(0));
            CHECK(line.find(full.substr(0, 6)) == std::string::npos);
            CHECK(parsed(line).root()["args"]["note"].as_string() == padding + "[token...");
        }
        SUBCASE("a number that spells an all-digit token withholds the arguments as a whole") {
            const std::array<std::string_view, 2> digits = {"8675309", ""};
            const core::json::document args = core::json::object({{"frames", 8675309}, {"name", "kept?"}});
            const std::string line = audit_log::format_line(entry_of("editor.wait_frames", args), digits, time_at(0));
            CHECK(line.find("8675309") == std::string::npos);
            CHECK(parsed(line).root()["args"].as_string() == audit_log::WITHHELD);
        }
        SUBCASE("an empty secret is no secret: nothing is replaced") {
            const std::array<std::string_view, 2> none = {"", ""};
            const core::json::document args = core::json::object({{"name", "Player"}});
            const std::string line = audit_log::format_line(entry_of("entity.find", args), none, time_at(0));
            CHECK(parsed(line).root()["args"]["name"].as_string() == "Player");
            CHECK(parsed(line).root()["cmd"].as_string() == "entity.find");
        }
    }

    TEST_CASE("audit_log - format_line bounds every part of a line") {
        SUBCASE("a long string is cut on a code point boundary") {
            // 'é' is two bytes: MAX_TEXT_BYTES falls in the middle of one when the text starts with one 'a'.
            std::string text = "a";
            for (usize index = 0; index < audit_log::MAX_TEXT_BYTES; ++index) {
                text += "\xC3\xA9";
            }
            const core::json::document args = core::json::object({{"source", text}});
            const core::json::document record = formatted(entry_of("script.write", args));
            const std::string_view kept = record.root()["args"]["source"].as_string();
            REQUIRE(kept.size() == audit_log::MAX_TEXT_BYTES - 1 + 3);
            CHECK(kept.substr(kept.size() - 5) == "\xC3\xA9...");
            CHECK(kept.front() == 'a');
        }
        SUBCASE("a text of exactly the limit is kept whole") {
            const std::string text(audit_log::MAX_TEXT_BYTES, 'x');
            const core::json::document args = core::json::object({{"source", text}});
            CHECK(formatted(entry_of("script.write", args)).root()["args"]["source"].as_string() == text);
        }
        SUBCASE("a long array keeps its first items and says how many it dropped") {
            core::json::document list = core::json::document::array();
            for (u32 index = 0; index < audit_log::MAX_ARRAY_ITEMS + 5; ++index) {
                list.push_back(index);
            }
            core::json::document args = core::json::document::object();
            args.set("entities", list);
            const core::json::document record = formatted(entry_of("entity.destroy_many", args));
            const core::json::value kept = record.root()["args"]["entities"];
            REQUIRE(kept.size() == audit_log::MAX_ARRAY_ITEMS + 1);
            CHECK(kept[audit_log::MAX_ARRAY_ITEMS - 1].as_u64() == audit_log::MAX_ARRAY_ITEMS - 1);
            CHECK(kept[audit_log::MAX_ARRAY_ITEMS].as_string() == "... 5 more");
        }
        SUBCASE("nesting beyond the limit is cut") {
            core::json::document nested = core::json::object({{"leaf", "deep"}});
            for (usize level = 0; level < audit_log::MAX_DEPTH + 2; ++level) {
                core::json::document outer = core::json::document::object();
                outer.set("inner", nested);
                nested = std::move(outer);
            }
            const core::json::document record = formatted(entry_of("component.set", nested));
            core::json::value cursor = record.root()["args"];
            for (usize level = 0; level < audit_log::MAX_DEPTH; ++level) {
                CAPTURE(level);
                REQUIRE(cursor.is_object());
                cursor = cursor["inner"];
            }
            CHECK(cursor.as_string() == "...");
        }
        SUBCASE("a wide object keeps its first members and says how many it dropped") {
            core::json::document args = core::json::document::object();
            for (u32 index = 0; index < audit_log::MAX_OBJECT_MEMBERS + 5; ++index) {
                args.set(std::format("k{:02}", index), index);
            }
            const core::json::document record = formatted(entry_of("component.set", args));
            const core::json::value kept = record.root()["args"];
            REQUIRE(kept.is_object());
            REQUIRE(kept.size() == audit_log::MAX_OBJECT_MEMBERS + 1);
            CHECK(kept.contains("k00"));
            CHECK(kept[std::format("k{:02}", audit_log::MAX_OBJECT_MEMBERS - 1)].is_integer());
            CHECK_FALSE(kept.contains(std::format("k{:02}", audit_log::MAX_OBJECT_MEMBERS)));
            CHECK(kept["..."].as_string() == "5 more");
        }
        SUBCASE("an object of exactly the member limit is kept whole") {
            core::json::document args = core::json::document::object();
            for (u32 index = 0; index < audit_log::MAX_OBJECT_MEMBERS; ++index) {
                args.set(std::format("k{:02}", index), index);
            }
            const core::json::document record = formatted(entry_of("component.set", args));
            const core::json::value kept = record.root()["args"];
            CHECK(kept.size() == audit_log::MAX_OBJECT_MEMBERS);
            CHECK_FALSE(kept.contains("..."));
        }
        SUBCASE("arguments that are still too long are replaced by a note") {
            core::json::document args = core::json::document::object();
            for (u32 index = 0; index < 30; ++index) {
                args.set(std::format("member{}", index), std::string(100, 'v'));
            }
            const core::json::document record = formatted(entry_of("component.set_many", args));
            CHECK(record.root()["args"].as_string() == OMITTED_NOTE);
        }
        SUBCASE("a number that spells a token inside arguments that are too long is omitted with them") {
            const std::array<std::string_view, 2> digits = {"8675309", ""};
            core::json::document args = core::json::document::object();
            args.set("frames", 8675309);
            for (u32 index = 0; index < 30; ++index) {
                args.set(std::format("member{}", index), std::string(100, 'v'));
            }
            const std::string line = audit_log::format_line(entry_of("editor.wait_frames", args), digits, time_at(0));
            CHECK(line.find("8675309") == std::string::npos);
            CHECK(parsed(line).root()["args"].as_string() == OMITTED_NOTE);
        }
        SUBCASE("a long command name is cut") {
            const std::string command(audit_log::MAX_COMMAND_BYTES + 50, 'c');
            const core::json::document args = core::json::document::object();
            const core::json::document record = formatted(entry_of(command, args));
            CHECK(record.root()["cmd"].as_string() == std::string(audit_log::MAX_COMMAND_BYTES, 'c') + "...");
        }
        SUBCASE("the worst case stays below MAX_LINE_BYTES") {
            // Control characters take six bytes each once escaped, and the arguments sit just under their limit.
            const std::string command(audit_log::MAX_COMMAND_BYTES, '\x01');
            core::json::document args = core::json::document::object();
            for (u32 index = 0; index < 11; ++index) {
                args.set(std::format("m{:02}", index), std::string(audit_log::MAX_TEXT_BYTES, 'v'));
            }
            audit_entry entry = entry_of(command, args);
            entry.clientId = 4294967295u;
            entry.transport = transport_kind::LOOPBACK;
            entry.ok = false;
            entry.error = protocol_error_code::MESSAGE_TOO_LARGE;
            entry.async = true;
            entry.batchChild = true;
            const std::string line = audit_log::format_line(entry, SECRETS, time_at(1794906902));
            CHECK(line.size() <= audit_log::MAX_LINE_BYTES);
            CHECK(parsed(line).root()["args"].is_object());
        }
    }

    TEST_CASE("regression - code sweep F187 audit_log a request of any width, length or depth becomes one bounded "
              "line") {
        // What an inspect-only client can send on one line of 16 MiB. The documents are parsed from text:
        // building one member by member would itself cost the square of its width.
        std::string text = "";
        bounded_form expectation = bounded_form::NOTE;
        SUBCASE("a hundred thousand members with small values: the first are kept, the rest counted") {
            text = "{";
            for (u32 index = 0; index < 100000; ++index) {
                text += std::format("{}\"k{}\":1", index == 0 ? "" : ",", index);
            }
            text += "}";
            expectation = bounded_form::FIRST_MEMBERS;
        }
        SUBCASE("a hundred thousand members with long values: a note") {
            const std::string value(100, 'v');
            text = "{";
            for (u32 index = 0; index < 100000; ++index) {
                text += std::format("{}\"k{}\":\"{}\"", index == 0 ? "" : ",", index, value);
            }
            text += "}";
        }
        SUBCASE("arrays of arrays, sixteen wide at every level the log keeps and a million items in all: a note") {
            std::string level = "[1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]";
            for (u32 depth = 0; depth < 4; ++depth) {
                std::string outer = "[";
                for (u32 index = 0; index < 16; ++index) {
                    outer += index == 0 ? "" : ",";
                    outer += level;
                }
                outer += "]";
                level = std::move(outer);
            }
            text = "{\"items\":" + level + "}";
        }
        SUBCASE("one string of four mebibytes that ends in a token: cut long before the token") {
            text = "{\"note\":\"" + std::string(4 * MIB, 'a') + std::string(FULL_TOKEN) + "\"}";
            expectation = bounded_form::CUT_TEXT;
        }
        const core::json::document args = parsed(text);
        const std::string line = audit_log::format_line(entry_of("session.ping", args), SECRETS, time_at(0));
        CHECK(line.size() <= audit_log::MAX_LINE_BYTES);
        CHECK(line.find(FULL_TOKEN) == std::string::npos);
        const core::json::document record = parsed(line);
        const core::json::value kept = record.root()["args"];
        switch (expectation) {
            case bounded_form::FIRST_MEMBERS:
                REQUIRE(kept.is_object());
                CHECK(kept.size() == audit_log::MAX_OBJECT_MEMBERS + 1);
                CHECK(kept["k0"].as_u64() == 1);
                CHECK(kept["..."].as_string() == std::format("{} more", 100000 - audit_log::MAX_OBJECT_MEMBERS));
                break;
            case bounded_form::NOTE:
                CHECK(kept.as_string() == OMITTED_NOTE);
                break;
            case bounded_form::CUT_TEXT:
                CHECK(kept["note"].as_string() == std::string(audit_log::MAX_TEXT_BYTES, 'a') + "...");
                break;
        }
    }

    TEST_CASE("audit_log - nothing is recorded without a folder, and nothing is created before the first record") {
        const testing::temp_directory project = {};
        audit_log log = {};
        const core::json::document args = core::json::document::object();
        CHECK(log.file().empty());
        CHECK(log.previous_file().empty());
        log.record(entry_of("session.ping", args), SECRETS);
        log.flush();
        CHECK(log.recorded_count() == 0);
        CHECK_FALSE(log.is_open());

        const std::filesystem::path folder = project.path() / ".conjure";
        log.set_directory(folder);
        CHECK(log.directory() == folder);
        CHECK(log.file() == folder / "ai_audit.log");
        CHECK(log.previous_file() == folder / "ai_audit.1.log");
        CHECK_FALSE(std::filesystem::exists(folder));

        log.record(entry_of("session.ping", args), SECRETS);
        CHECK(log.is_open());
        CHECK(log.recorded_count() == 1);
        log.flush();
        const std::vector<core::json::document> records = editor_test::read_json_lines(log.file());
        REQUIRE(records.size() == 1);
        CHECK(records[0].root()["cmd"].as_string() == "session.ping");
        CHECK(records[0].root()["time"].as_string().size() == std::string_view("1970-01-01T00:00:00Z").size());
        CHECK(records[0].root()["time"].as_string().ends_with("Z"));

        log.set_directory({});
        CHECK_FALSE(log.is_open());
        log.record(entry_of("session.ping", args), SECRETS);
        CHECK(log.recorded_count() == 1);
    }

    TEST_CASE("audit_log - a log that is there is appended to, across close and across editors") {
        const testing::temp_directory project = {};
        const core::json::document args = core::json::document::object();
        {
            audit_log first = {};
            first.set_directory(project.path());
            first.record(entry_of("first.one", args), SECRETS);
            first.close();
            CHECK_FALSE(first.is_open());
            first.record(entry_of("first.two", args), SECRETS);
            CHECK(first.is_open());
        }
        audit_log second = {};
        second.set_directory(project.path());
        second.record(entry_of("second.one", args), SECRETS);
        second.flush();
        const std::vector<core::json::document> records = editor_test::read_json_lines(second.file());
        REQUIRE(records.size() == 3);
        CHECK(records[0].root()["cmd"].as_string() == "first.one");
        CHECK(records[1].root()["cmd"].as_string() == "first.two");
        CHECK(records[2].root()["cmd"].as_string() == "second.one");
    }

    TEST_CASE("audit_log - a full file becomes the previous one, and the two never exceed twice the limit") {
        const testing::temp_directory project = {};
        audit_log log = {};
        log.set_directory(project.path());
        SUBCASE("a limit below one line is raised to one line") {
            log.set_max_file_bytes(1);
            CHECK(log.max_file_bytes() == audit_log::MAX_LINE_BYTES);
            log.set_max_file_bytes(0);
            CHECK(log.max_file_bytes() == audit_log::MAX_LINE_BYTES);
        }
        SUBCASE("the files stay bounded through several turns and the newest record is always the last line") {
            log.set_max_file_bytes(audit_log::MAX_LINE_BYTES);
            // About 150 bytes a record: 200 of them fill the file several times over.
            record_many(log, "turn", 200);
            CHECK(log.recorded_count() == 200);
            REQUIRE(std::filesystem::exists(log.previous_file()));
            const std::vector<core::json::document> current = editor_test::read_json_lines(log.file());
            const std::vector<core::json::document> previous = editor_test::read_json_lines(log.previous_file());
            REQUIRE_FALSE(current.empty());
            REQUIRE_FALSE(previous.empty());
            CHECK(current.back().root()["cmd"].as_string() == "turn.199");
            // The two files hold one unbroken run of the most recent records.
            const usize kept = current.size() + previous.size();
            CHECK(kept < 200);
            CHECK(previous.front().root()["cmd"].as_string() == std::format("turn.{}", 200 - kept));
            CHECK(current.front().root()["cmd"].as_string() == std::format("turn.{}", 200 - current.size()));
        }
        SUBCASE("a lowered limit removes the file that is larger, with the next record") {
            record_many(log, "wide", 40);
            CHECK_FALSE(std::filesystem::exists(log.previous_file()));
            REQUIRE(size_of(log.file()) > audit_log::MAX_LINE_BYTES);
            log.set_max_file_bytes(audit_log::MAX_LINE_BYTES);
            CHECK_FALSE(log.is_open());
            const core::json::document args = core::json::document::object();
            log.record(entry_of("narrow.0", args), SECRETS);
            log.flush();
            CHECK_FALSE(std::filesystem::exists(log.previous_file()));
            const std::vector<core::json::document> current = editor_test::read_json_lines(log.file());
            REQUIRE(current.size() == 1);
            CHECK(current[0].root()["cmd"].as_string() == "narrow.0");
        }
    }

    TEST_CASE("audit_log - files found above the limit when the log opens are removed, not kept") {
        const testing::temp_directory project = {};
        const std::string oversized(audit_log::MAX_LINE_BYTES + 1, 'x');
        const std::string fitting = "{\"cmd\":\"earlier\"}\n";
        const core::json::document args = core::json::document::object();
        audit_log log = {};
        log.set_directory(project.path());
        log.set_max_file_bytes(audit_log::MAX_LINE_BYTES);

        SUBCASE("both files too large") {
            CONJURE_REQUIRE_OK(project.write_text("ai_audit.log", oversized));
            CONJURE_REQUIRE_OK(project.write_text("ai_audit.1.log", oversized));
            log.record(entry_of("fresh", args), SECRETS);
            log.flush();
            CHECK_FALSE(std::filesystem::exists(log.previous_file()));
            const std::vector<core::json::document> records = editor_test::read_json_lines(log.file());
            REQUIRE(records.size() == 1);
            CHECK(records[0].root()["cmd"].as_string() == "fresh");
        }
        SUBCASE("files within the limit are kept and appended to") {
            write_kept_file(project.file("ai_audit.log"), fitting);
            write_kept_file(project.file("ai_audit.1.log"), fitting);
            log.record(entry_of("fresh", args), SECRETS);
            log.flush();
            CHECK(editor_test::read_text_file(log.previous_file()) == fitting);
            const std::vector<core::json::document> records = editor_test::read_json_lines(log.file());
            REQUIRE(records.size() == 2);
            CHECK(records[0].root()["cmd"].as_string() == "earlier");
            CHECK(records[1].root()["cmd"].as_string() == "fresh");
        }
        SUBCASE("a file of exactly the limit is kept; the next record starts a new one") {
            const std::string exact(audit_log::MAX_LINE_BYTES, 'x');
            write_kept_file(project.file("ai_audit.log"), exact);
            log.record(entry_of("fresh", args), SECRETS);
            log.flush();
            CHECK(editor_test::read_text_file(log.previous_file()) == exact);
            CHECK(editor_test::read_json_lines(log.file()).size() == 1);
        }
    }

    TEST_CASE("audit_log - the file it creates can be opened by the user running the editor only") {
        const testing::temp_directory project = {};
        if (!editor_test::owner_only_files_available(project.path())) {
            return;
        }
        audit_log log = {};
        log.set_directory(project.path() / ".conjure");
        log.set_max_file_bytes(audit_log::MAX_LINE_BYTES);
        record_many(log, "private", 60);
        REQUIRE(std::filesystem::exists(log.previous_file()));
        // The file made by the first record, and the one a full file made way for.
        const core::result<bool> currentIsPrivate = core::vfs::is_private_file(log.file());
        CONJURE_REQUIRE_OK(currentIsPrivate);
        CHECK(currentIsPrivate.value());
        const core::result<bool> previousIsPrivate = core::vfs::is_private_file(log.previous_file());
        CONJURE_REQUIRE_OK(previousIsPrivate);
        CHECK(previousIsPrivate.value());
    }

    TEST_CASE("regression - code sweep F187 audit_log a file others may open is replaced by an owner-only one, "
              "never appended to") {
        testing::log_capture logs = {};
        const testing::temp_directory project = {};
        if (!editor_test::owner_only_files_available(project.path())) {
            return;
        }
        const std::filesystem::path folder = project.path() / ".conjure";
        std::filesystem::create_directories(folder);
        const std::filesystem::path victim = project.file("victim.txt");
        SUBCASE("a file another account left readable by others") {
            CONJURE_REQUIRE_OK(project.write_text(".conjure/ai_audit.log", R"({"cmd":"planted"})" "\n"));
        }
        SUBCASE("a hard link to a file of someone else's") {
            CONJURE_REQUIRE_OK(project.write_text("victim.txt", "untouched"));
            std::error_code linkError = {};
            std::filesystem::create_hard_link(victim, folder / "ai_audit.log", linkError);
            REQUIRE_MESSAGE(!linkError, linkError.message());
        }
        // An ordinary file inherits the folder's access list on Windows; on POSIX the mode bits are said aloud,
        // whatever the umask.
        std::filesystem::permissions(
          folder / "ai_audit.log",
          std::filesystem::perms::group_read | std::filesystem::perms::others_read,
          std::filesystem::perm_options::add
        );
        const core::result<bool> before = core::vfs::is_private_file(folder / "ai_audit.log");
        CONJURE_REQUIRE_OK(before);
        REQUIRE_FALSE(before.value());

        audit_log log = {};
        log.set_directory(folder);
        const core::json::document args = core::json::document::object();
        log.record(entry_of("fresh", args), SECRETS);
        log.flush();
        CHECK(log.recorded_count() == 1);
        const std::vector<core::json::document> records = editor_test::read_json_lines(log.file());
        REQUIRE(records.size() == 1);
        CHECK(records[0].root()["cmd"].as_string() == "fresh");
        const core::result<bool> after = core::vfs::is_private_file(log.file());
        CONJURE_REQUIRE_OK(after);
        CHECK(after.value());
        CHECK(logs.contains(core::log_level::WARN, "could be opened by other users"));
        if (std::filesystem::exists(victim)) {
            CHECK(editor_test::read_text_file(victim) == "untouched");
        }
        // The folder goes with this scope: the log lets go of its file first.
        log.close();
    }

    TEST_CASE("audit_log - a symbolic link planted at the path is replaced, never written through") {
        const testing::temp_directory project = {};
        const std::filesystem::path victim = project.file("victim.txt");
        CONJURE_REQUIRE_OK(project.write_text("victim.txt", "untouched"));
        const std::filesystem::path folder = project.path() / ".conjure";
        std::filesystem::create_directories(folder);
        std::error_code linkError = {};
        std::filesystem::create_symlink(victim, folder / "ai_audit.log", linkError);
        if (linkError) {
            // Windows creates symbolic links only for administrators or in developer mode.
            MESSAGE("this account cannot create a symbolic link (" << linkError.message() << "): the case did not run");
            return;
        }
        audit_log log = {};
        log.set_directory(folder);
        const core::json::document args = core::json::document::object();
        log.record(entry_of("session.ping", args), SECRETS);
        log.flush();
        CHECK(editor_test::read_text_file(victim) == "untouched");
        std::error_code ignored = {};
        CHECK(std::filesystem::is_regular_file(std::filesystem::symlink_status(log.file(), ignored)));
        CHECK(editor_test::read_json_lines(log.file()).size() == 1);
    }

    TEST_CASE("audit_log - a folder where the file belongs is reported once and the editor carries on unrecorded") {
        testing::log_capture logs = {};
        const testing::temp_directory project = {};
        // Not empty, so that nothing can simply take it away.
        std::filesystem::create_directories(project.path() / "ai_audit.log" / "occupied");
        audit_log log = {};
        log.set_directory(project.path());
        const core::json::document args = core::json::document::object();
        log.record(entry_of("first", args), SECRETS);
        log.record(entry_of("second", args), SECRETS);
        log.flush();
        CHECK(log.recorded_count() == 0);
        CHECK_FALSE(log.is_open());
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "the audit log"));
        CHECK(logs.contains(core::log_level::WARN, "cannot be written"));
        CHECK(std::filesystem::is_directory(project.path() / "ai_audit.log" / "occupied"));

        SUBCASE("another folder is a new start") {
            const testing::temp_directory other = {};
            log.set_directory(other.path());
            log.record(entry_of("third", args), SECRETS);
            log.flush();
            CHECK(log.recorded_count() == 1);
            CHECK(editor_test::read_json_lines(log.file()).size() == 1);
            CHECK(logs.count(core::log_level::WARN) == 1);
            // The folder goes with this scope: the log lets go of its file first.
            log.close();
        }
    }

    TEST_CASE("audit_log - when the previous file's name cannot be taken the full file is emptied in place and the "
              "bound holds") {
        const testing::temp_directory project = {};
        std::filesystem::create_directories(project.path() / "ai_audit.1.log" / "occupied");
        audit_log log = {};
        log.set_directory(project.path());
        log.set_max_file_bytes(audit_log::MAX_LINE_BYTES);
        const core::json::document args = core::json::object({{"entity", "Player"}, {"enabled", true}});
        for (u32 index = 0; index < 120; ++index) {
            CAPTURE(index);
            const std::string command = std::format("blocked.{}", index);
            log.record(entry_of(command, args), SECRETS);
            log.flush();
            REQUIRE(size_of(log.file()) <= log.max_file_bytes());
        }
        CHECK(log.recorded_count() == 120);
        CHECK(std::filesystem::is_directory(project.path() / "ai_audit.1.log" / "occupied"));
        const std::vector<core::json::document> current = editor_test::read_json_lines(log.file());
        REQUIRE_FALSE(current.empty());
        CHECK(current.size() < 120);
        CHECK(current.back().root()["cmd"].as_string() == "blocked.119");
    }
}
