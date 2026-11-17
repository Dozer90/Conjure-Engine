#include "editor/core/commands/command_support.h"

#include "conjure/core/log.h"

// console.* (docs/design/04_editor.md 2.6): reads and clears the console ring.

namespace conjure::editor::support {

    namespace {
        core::result<command_outcome> console_read(command_invocation &invocation) {
            console_query query = {};
            query.sinceSeq = invocation.args["sinceSeq"].as_u64(0);
            CONJURE_TRY_ASSIGN(const u32 limit, u32_or(invocation.args, "limit", 200));
            query.limit = limit;
            if (invocation.args.contains("minLevel")) {
                // "off" parses, and is rejected on purpose: OFF is not a level a line can have, so it cannot
                // be the minimum of a filter (code sweep F209).
                const std::optional<core::log_level> level =
                  core::parse_log_level(string_or(invocation.args, "minLevel", "trace"));
                if (!level.has_value() || *level == core::log_level::OFF) {
                    return invalid_args("args.minLevel: expected trace, debug, info, warn, error or fatal");
                }
                query.minLevel = *level;
            }
            query.category = std::string(string_or(invocation.args, "category", ""));
            const std::vector<console_record> records = invocation.context.console().read(query);
            core::json::document entries = core::json::document::array();
            for (const console_record &record : records) {
                entries.push_back(core::json::object({
                  {"seq", record.seq},
                  {"frame", record.frame},
                  {"level", core::to_string(record.level)},
                  {"category", std::string_view(record.category)},
                  {"message", std::string_view(record.text)},
                }));
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({
              {"entries", entries},
              {"lastSeq", invocation.context.console().last_seq()},
            });
            return outcome;
        }

        core::result<command_outcome> console_clear(command_invocation &invocation) {
            invocation.context.console().clear();
            command_outcome outcome = {};
            outcome.result = core::json::object({{"cleared", true}});
            return outcome;
        }
    }

    void register_console_commands(command_registry &registry) {
        add_command(
          registry,
          make_command(
            "console.read",
            "Log lines after a sequence number, filtered by level and category.",
            R"({"type":"object","properties":{"sinceSeq":{"type":"integer","minimum":0},)"
            R"("minLevel":{"type":"string","enum":["trace","debug","info","warn","error","fatal"]},)"
            R"("category":{"type":"string"},"limit":{"type":"integer","minimum":1}}})",
            R"({"type":"object","properties":{"entries":{"type":"array"},"lastSeq":{"type":"integer"}}})",
            command_flags::READ_ONLY,
            &console_read
          )
        );
        add_command(
          registry,
          make_command(
            "console.clear",
            "Empties the console ring.",
            R"({"type":"object"})",
            R"({"type":"object","properties":{"cleared":{"type":"boolean"}}})",
            command_flags::NONE,
            &console_clear
          )
        );
    }
}
