#include "conjure/editor/command_registry.h"

#include "conjure/editor/editor_error.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/protocol/protocol_message.h"

#include "conjure/core/assert.h"
#include "conjure/core/hash.h"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

namespace conjure::editor {

    namespace {
        // Command names are a few dozen bytes; a longer name is no misspelling of one, and the edit distance
        // costs its length times every command's, so an over-long unknown name gets no suggestions.
        constexpr usize MAX_SUGGESTION_NAME_LENGTH = 128;

        constexpr std::array<std::string_view, static_cast<usize>(editor_event::COUNT)> EVENT_NAMES = {
            "log",
            "selection.changed",
            "play.state",
            "scene.changed",
            "scene.loaded",
            "progress",
            "script.compiled",
            "script.reloaded",
            "asset.imported",
            "module.state",
            "command.executed",
            "client.connected",
            "client.disconnected",
            "events.dropped",
            "warning",
        };

        [[nodiscard]] usize levenshtein(std::string_view left, std::string_view right) {
            std::vector<usize> previous(right.size() + 1, 0);
            std::vector<usize> current(right.size() + 1, 0);
            for (usize column = 0; column <= right.size(); ++column) {
                previous[column] = column;
            }
            for (usize row = 1; row <= left.size(); ++row) {
                current[0] = row;
                for (usize column = 1; column <= right.size(); ++column) {
                    const usize substitution = previous[column - 1] + (left[row - 1] == right[column - 1] ? 0u : 1u);
                    current[column] = std::min({previous[column] + 1, current[column - 1] + 1, substitution});
                }
                std::swap(previous, current);
            }
            return previous[right.size()];
        }
    }

    command_registry::command_registry() = default;

    command_registry::~command_registry() = default;

    core::result<void> command_registry::add(command_descriptor descriptor) {
        if (descriptor.name.empty()) {
            return core::make_error(protocol_error_code::INVALID_ARGS, "a command needs a name");
        }
        if (!descriptor.handler) {
            return core::make_error(
              protocol_error_code::INVALID_ARGS,
              std::format("command '{}' has no handler", descriptor.name)
            );
        }
        if (find_entry(descriptor.name) != nullptr) {
            return core::make_error(
              protocol_error_code::INVALID_ARGS,
              std::format("command '{}' is already registered", descriptor.name)
            );
        }
        core::result<compiled_schema> argsSchema = compiled_schema::compile(descriptor.argsSchema);
        if (!argsSchema.has_value()) {
            return core::make_error(
              protocol_error_code::INVALID_ARGS,
              std::format("command '{}': args {}", descriptor.name, argsSchema.get_error().message())
            );
        }
        core::result<compiled_schema> resultSchema = compiled_schema::compile(descriptor.resultSchema);
        if (!resultSchema.has_value()) {
            return core::make_error(
              protocol_error_code::INVALID_ARGS,
              std::format("command '{}': result {}", descriptor.name, resultSchema.get_error().message())
            );
        }
        if (has_flag(descriptor.flags, command_flags::ON_DISK)) {
            const bool undoable = has_flag(descriptor.flags, command_flags::MUTATES) &&
              !has_flag(descriptor.flags, command_flags::NO_UNDO);
            if (!undoable) {
                return core::make_error(
                  protocol_error_code::INVALID_ARGS,
                  std::format(
                    "command '{}': the on_disk flag needs mutates and excludes no_undo (it promises an inverse)",
                    descriptor.name
                  )
                );
            }
        }
        const bool readOnly = has_flag(descriptor.flags, command_flags::READ_ONLY);
        if (readOnly && has_flag(descriptor.flags, command_flags::MUTATES)) {
            return core::make_error(
              protocol_error_code::INVALID_ARGS,
              std::format(
                "command '{}': the read_only flag excludes mutates (it opens the command to inspect-only clients)",
                descriptor.name
              )
            );
        }
        if (has_flag(descriptor.flags, command_flags::SYNC_IN_BATCH)) {
            const bool batchable = has_flag(descriptor.flags, command_flags::ASYNC) &&
              !has_flag(descriptor.flags, command_flags::NOT_IN_BATCH);
            if (!batchable) {
                return core::make_error(
                  protocol_error_code::INVALID_ARGS,
                  std::format(
                    "command '{}': the sync_in_batch flag needs async and excludes not_in_batch (it promises an "
                    "answer inside a batch)",
                    descriptor.name
                  )
                );
            }
        } else if (has_flag(descriptor.flags, command_flags::ASYNC)) {
            descriptor.flags |= command_flags::NOT_IN_BATCH;
        }

        entry added = {};
        added.descriptor = std::move(descriptor);
        added.argsSchema = std::move(argsSchema).value();
        added.resultSchema = std::move(resultSchema).value();
        const auto position = std::lower_bound(
          mEntries.begin(),
          mEntries.end(),
          added.descriptor.name,
          [](const entry &existing, const std::string &name) {
              return existing.descriptor.name < name;
          }
        );
        mEntries.insert(position, std::move(added));
        mSchemaHashStale = true;
        return core::result<void>::success();
    }

    bool command_registry::remove(std::string_view name) {
        const auto position = std::find_if(mEntries.begin(), mEntries.end(), [name](const entry &existing) {
            return existing.descriptor.name == name;
        });
        if (position == mEntries.end()) {
            return false;
        }
        mEntries.erase(position);
        mSchemaHashStale = true;
        return true;
    }

    usize command_registry::remove_owned_by(std::string_view owner) {
        const usize before = mEntries.size();
        std::erase_if(mEntries, [owner](const entry &existing) {
            return existing.descriptor.owner == owner;
        });
        if (mEntries.size() != before) {
            mSchemaHashStale = true;
        }
        return before - mEntries.size();
    }

    const command_descriptor *command_registry::find(std::string_view name) const noexcept {
        const entry *found = find_entry(name);
        return found == nullptr ? nullptr : &found->descriptor;
    }

    const compiled_schema *command_registry::args_schema(std::string_view name) const noexcept {
        const entry *found = find_entry(name);
        return found == nullptr ? nullptr : &found->argsSchema;
    }

    std::vector<const command_descriptor *> command_registry::descriptors() const {
        std::vector<const command_descriptor *> list = {};
        list.reserve(mEntries.size());
        for (const entry &existing : mEntries) {
            list.push_back(&existing.descriptor);
        }
        return list;
    }

    std::vector<std::string> command_registry::names() const {
        std::vector<std::string> list = {};
        list.reserve(mEntries.size());
        for (const entry &existing : mEntries) {
            list.push_back(existing.descriptor.name);
        }
        return list;
    }

    std::vector<std::string> command_registry::closest_names(std::string_view name, usize count) const {
        if (count == 0 || name.size() > MAX_SUGGESTION_NAME_LENGTH) {
            return {};
        }
        struct scored {
            usize distance = 0;
            std::string_view name = "";
        };
        std::vector<scored> scores = {};
        scores.reserve(mEntries.size());
        for (const entry &existing : mEntries) {
            if (has_flag(existing.descriptor.flags, command_flags::UI_ONLY)) {
                continue;
            }
            scores.push_back(scored{levenshtein(name, existing.descriptor.name), existing.descriptor.name});
        }
        std::sort(scores.begin(), scores.end(), [](const scored &left, const scored &right) {
            if (left.distance != right.distance) {
                return left.distance < right.distance;
            }
            return left.name < right.name;
        });
        std::vector<std::string> result = {};
        for (usize index = 0; index < scores.size() && index < count; ++index) {
            result.emplace_back(scores[index].name);
        }
        return result;
    }

    core::json::document command_registry::describe(const command_descriptor &descriptor) const {
        const entry *found = find_entry(descriptor.name);
        core::json::document description = core::json::document::object();
        description.set("name", std::string_view(descriptor.name));
        description.set("summary", std::string_view(descriptor.summary));
        description.set("flags", command_flag_names(descriptor.flags));
        description.set("owner", std::string_view(descriptor.owner));
        if (found != nullptr) {
            description.set("args", found->argsSchema.schema());
            description.set("result", found->resultSchema.schema());
        } else {
            core::result<core::json::document> args = core::json::document::parse(descriptor.argsSchema);
            core::result<core::json::document> result = core::json::document::parse(descriptor.resultSchema);
            description.set("args", args.has_value() ? args.value() : core::json::document::object());
            description.set("result", result.has_value() ? result.value() : core::json::document::object());
        }
        return description;
    }

    core::json::document command_registry::describe_all(bool includeUiOnly) const {
        core::json::document catalogue = core::json::document::object();
        catalogue.set("protocolVersion", PROTOCOL_VERSION);
        core::json::document commands = core::json::document::array();
        for (const entry &existing : mEntries) {
            if (!includeUiOnly && has_flag(existing.descriptor.flags, command_flags::UI_ONLY)) {
                continue;
            }
            commands.push_back(describe(existing.descriptor));
        }
        catalogue.set("commands", commands);
        core::json::document events = core::json::document::array();
        for (const std::string_view name : EVENT_NAMES) {
            events.push_back(name);
        }
        catalogue.set("events", events);
        core::json::document codes = core::json::document::array();
        for (const protocol_error_code code : all_error_codes()) {
            codes.push_back(to_string(code));
        }
        catalogue.set("errorCodes", codes);
        catalogue.set("schemaHash", std::format("{:016x}", schema_hash()));
        return catalogue;
    }

    u64 command_registry::schema_hash() const {
        if (mSchemaHashStale) {
            recompute_hash();
        }
        return mSchemaHash;
    }

    const command_registry::entry *command_registry::find_entry(std::string_view name) const noexcept {
        const auto position = std::lower_bound(
          mEntries.begin(),
          mEntries.end(),
          name,
          [](const entry &existing, std::string_view wanted) {
              return std::string_view(existing.descriptor.name) < wanted;
          }
        );
        if (position == mEntries.end() || position->descriptor.name != name) {
            return nullptr;
        }
        return &*position;
    }

    void command_registry::recompute_hash() const {
        // The hash covers names, flags, owners and schemas but not itself: it is computed over the
        // catalogue without the schemaHash member. Main thread only, like every other member, so the lazy
        // update of the mutable cache needs no lock.
        core::json::document catalogue = core::json::document::object();
        core::json::document commands = core::json::document::array();
        for (const entry &existing : mEntries) {
            if (has_flag(existing.descriptor.flags, command_flags::UI_ONLY)) {
                continue;
            }
            commands.push_back(describe(existing.descriptor));
        }
        catalogue.set("commands", commands);
        catalogue.set("protocolVersion", PROTOCOL_VERSION);
        mSchemaHash = core::hash::fnv1a64(catalogue.to_canonical_string());
        mSchemaHashStale = false;
    }

    std::span<const std::string_view> protocol_event_names() noexcept {
        return EVENT_NAMES;
    }
}
