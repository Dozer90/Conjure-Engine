#include "editor/core/commands/command_support.h"

#include "conjure/editor/entity_ref.h"
#include "conjure/editor/selection_set.h"

#include <format>
#include <utility>

// selection.* (docs/design/04_editor.md 4.4): the selection is guid based; the context emits selection.changed
// from the set's on_changed event, so the commands only mutate it.

namespace conjure::editor::support {

    namespace {
        core::json::document describe_selection(editor_context &context) {
            scene::scene &scene = context.scene();
            core::json::document entities = core::json::document::array();
            for (const core::uuid &guid : context.selection().guids()) {
                const ecs::entity entity = scene.find_guid(guid);
                if (!entity.is_null()) {
                    entities.push_back(entity_summary(scene, entity));
                }
            }
            // The {guids, primary} the selection.changed event carries (primary null when nothing is selected),
            // plus the summaries and the count.
            core::json::document result = context.selection_payload();
            result.set("entities", entities);
            result.set("count", context.selection().size());
            return result;
        }

        core::result<command_outcome> selection_get(command_invocation &invocation) {
            command_outcome outcome = {};
            outcome.result = describe_selection(invocation.context);
            return outcome;
        }

        core::result<command_outcome> selection_set_command(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            const core::json::value list = invocation.args["entities"];
            if (!list.is_array()) {
                return invalid_args("args.entities: expected an array of guids or paths");
            }
            CONJURE_TRY_ASSIGN(
              const std::vector<ecs::entity> entities,
              entity_ref::resolve_many(list, scene, &invocation.errorDetails)
            );
            std::vector<core::uuid> guids = {};
            guids.reserve(entities.size());
            for (const ecs::entity entity : entities) {
                CONJURE_TRY_ASSIGN(const core::uuid guid, entity_ref::guid_of(scene, entity));
                guids.push_back(guid);
            }
            const std::string_view mode = string_or(invocation.args, "mode", "replace");
            if (mode == "replace") {
                context.selection().set(guids);
            } else if (mode == "add") {
                context.selection().add(guids);
            } else if (mode == "remove") {
                context.selection().remove(guids);
            } else {
                return invalid_args("args.mode: expected \"replace\", \"add\" or \"remove\"");
            }
            context.selection().rebuild(scene);
            command_outcome outcome = {};
            outcome.result = describe_selection(context);
            return outcome;
        }

        core::result<command_outcome> selection_clear(command_invocation &invocation) {
            invocation.context.selection().clear();
            command_outcome outcome = {};
            outcome.result = describe_selection(invocation.context);
            return outcome;
        }
    }

    void register_selection_commands(command_registry &registry) {
        const std::string entityRef = std::string(ENTITY_REF_SCHEMA);
        const std::string resultSchema =
          R"({"type":"object","properties":{"guids":{"type":"array"},"entities":{"type":"array"},)"
          R"("count":{"type":"integer"},"primary":{"type":["string","null"]}}})";
        add_command(
          registry,
          make_command(
            "selection.get",
            "The selected entities.",
            std::string(EMPTY_SCHEMA),
            resultSchema,
            command_flags::REQUIRES_SCENE | command_flags::READ_ONLY,
            &selection_get
          )
        );
        add_command(
          registry,
          make_command(
            "selection.set",
            "Replaces, extends or reduces the selection.",
            std::format(
              R"({{"type":"object","properties":{{"entities":{{"type":"array","items":{}}},)"
              R"("mode":{{"type":"string","enum":["replace","add","remove"]}}}},"required":["entities"]}})",
              entityRef
            ),
            resultSchema,
            command_flags::REQUIRES_SCENE,
            &selection_set_command
          )
        );
        add_command(
          registry,
          make_command(
            "selection.clear",
            "Empties the selection.",
            std::string(EMPTY_SCHEMA),
            resultSchema,
            command_flags::REQUIRES_SCENE,
            &selection_clear
          )
        );
    }
}
