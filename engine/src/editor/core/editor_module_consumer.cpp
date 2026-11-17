#include "conjure/editor/editor_module_consumer.h"

#include "conjure/editor/command_desc.h"
#include "conjure/editor/editor_command.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/play_controller.h"

#include "conjure/modules/module_context.h"
#include "conjure/modules/module_error.h"
#include "conjure/modules/module_registry.h"

#include <algorithm>
#include <format>
#include <utility>

namespace conjure::editor {

    namespace {
        // The owner of the built-in commands (support::add_command) and panels (editor_workspace): a module of that
        // name would take them with it when it unloads.
        constexpr std::string_view BUILTIN_OWNER = "editor";

        [[nodiscard]] std::string_view text_or(const char *text, std::string_view fallback) noexcept {
            return text == nullptr ? fallback : std::string_view(text);
        }

        [[nodiscard]] command_descriptor make_descriptor(const command_desc &desc, std::string_view owner) {
            command_descriptor descriptor = {};
            descriptor.name = std::string(text_or(desc.name, ""));
            descriptor.summary = std::string(text_or(desc.summary, ""));
            descriptor.argsSchema = std::string(text_or(desc.argsSchema, "{}"));
            descriptor.resultSchema = std::string(text_or(desc.resultSchema, "{}"));
            // ON_DISK promises an inverse to keep while playing; a module command has none (below).
            descriptor.flags = desc.flags & ~command_flags::ON_DISK;
            if (has_flag(desc.flags, command_flags::MUTATES)) {
                // The record's handler returns a result document only: it can never supply an inverse, so a
                // module mutation bumps the revision but is not undoable. And a command that says it mutates
                // is not one for inspect-only clients, whatever else it says (the registry refuses the pair).
                descriptor.flags |= command_flags::NO_UNDO;
                descriptor.flags &= ~command_flags::READ_ONLY;
            }
            descriptor.owner = std::string(owner);
            const command_handler_fn handler = desc.handler;
            void *userData = desc.userData;
            descriptor.handler = [handler, userData](command_invocation &invocation) -> core::result<command_outcome> {
                if (handler == nullptr) {
                    return make_editor_error(protocol_error_code::NOT_AVAILABLE, "the module command has no handler");
                }
                core::result<core::json::document> result = handler(invocation.args, userData);
                if (!result.has_value()) {
                    return core::error(result.get_error());
                }
                command_outcome outcome = {};
                outcome.result = std::move(result).value();
                if (!outcome.result.root().is_object()) {
                    outcome.result = core::json::document::object();
                }
                return outcome;
            };
            return descriptor;
        }
    }

    editor_module_consumer::editor_module_consumer(command_registry &registry, const play_controller *play) :
        mRegistry(&registry),
        mPlay(play) {}

    editor_module_consumer::~editor_module_consumer() = default;

    core::string_id editor_module_consumer::name() const noexcept {
        return core::string_id("editor");
    }

    core::result<void> editor_module_consumer::validate(
      modules::module_id id,
      const modules::module_registration_set &set,
      const modules::module_registry &registry
    ) {
        const std::string_view moduleName = registry.manifest(id).name;
        if (moduleName == BUILTIN_OWNER) {
            return modules::make_module_error(
              modules::module_error::REGISTRATION_CONFLICT,
              "editor: the module name 'editor' is reserved for the editor's built-in commands and panels"
            );
        }
        const core::small_vector<const command_desc *, 8> commands = modules::extensions_of<command_desc>(set);
        for (const command_desc *desc : commands) {
            const std::string_view commandName = text_or(desc->name, "");
            if (commandName.empty()) {
                return modules::make_module_error(
                  modules::module_error::REGISTRATION_CONFLICT,
                  std::format("editor: module '{}' registers a command without a name", moduleName)
                );
            }
            if (mRegistry->find(commandName) != nullptr) {
                return modules::make_module_error(
                  modules::module_error::REGISTRATION_CONFLICT,
                  std::format("editor: command '{}' of module '{}' is already registered", commandName, moduleName)
                );
            }
            const core::result<compiled_schema> args = compiled_schema::compile(text_or(desc->argsSchema, "{}"));
            if (!args.has_value()) {
                return modules::make_module_error(
                  modules::module_error::REGISTRATION_CONFLICT,
                  std::format("editor: command '{}': {}", commandName, args.get_error().message())
                );
            }
            const core::result<compiled_schema> result = compiled_schema::compile(text_or(desc->resultSchema, "{}"));
            if (!result.has_value()) {
                return modules::make_module_error(
                  modules::module_error::REGISTRATION_CONFLICT,
                  std::format("editor: command '{}': result {}", commandName, result.get_error().message())
                );
            }
        }
        const core::small_vector<const panel_desc *, 8> panels = modules::extensions_of<panel_desc>(set);
        for (const panel_desc *desc : panels) {
            if (text_or(desc->title, "").empty()) {
                return modules::make_module_error(
                  modules::module_error::REGISTRATION_CONFLICT,
                  std::format("editor: module '{}' registers a panel without a title", moduleName)
                );
            }
        }
        return core::result<void>::success();
    }

    void editor_module_consumer::attach(
      modules::module_id id,
      modules::module_context &context,
      const modules::module_registration_set &set
    ) {
        const std::string owner = context.manifest().name;
        for (const command_desc *desc : modules::extensions_of<command_desc>(set)) {
            const core::result<void> added = mRegistry->add(make_descriptor(*desc, owner));
            if (!added.has_value()) {
                CONJURE_LOG_WARN(LOG_EDITOR, "module '{}': {}", owner, added.get_error().message());
            }
        }
        bool panelsChanged = false;
        for (const panel_desc *desc : modules::extensions_of<panel_desc>(set)) {
            module_panel_entry entry = {};
            entry.owner = owner;
            entry.title = std::string(text_or(desc->title, ""));
            entry.desc = *desc;
            entry.module = id;
            mPanels.push_back(std::move(entry));
            panelsChanged = true;
        }
        mAttached.push_back(id);
        mModuleNames.push_back(owner);
        if (panelsChanged) {
            mOnPanelsChanged.emit();
        }
    }

    core::result<void> editor_module_consumer::can_detach(modules::module_id id, modules::unload_policy policy) {
        (void) policy;
        if (mPlay != nullptr && !mPlay->is_editing()) {
            return modules::make_module_error(
              modules::module_error::UNLOAD_BLOCKED,
              std::format("editor: module {} cannot unload while the editor plays", modules::to_string(id))
            );
        }
        return core::result<void>::success();
    }

    void editor_module_consumer::detach(modules::module_id id, modules::reload_context *reload) {
        (void) reload;
        const auto position = std::find(mAttached.begin(), mAttached.end(), id);
        if (position == mAttached.end()) {
            return;
        }
        const usize index = static_cast<usize>(position - mAttached.begin());
        const std::string owner = mModuleNames[index];
        // validate() refuses the reserved name; the check here keeps the built-ins even if that ever changes.
        if (owner != BUILTIN_OWNER) {
            mRegistry->remove_owned_by(owner);
        }
        const usize before = mPanels.size();
        std::erase_if(mPanels, [id](const module_panel_entry &entry) {
            return entry.module == id;
        });
        mAttached.erase(position);
        mModuleNames.erase(mModuleNames.begin() + static_cast<isize>(index));
        if (mPanels.size() != before) {
            mOnPanelsChanged.emit();
        }
    }

    void editor_module_consumer::restore(modules::module_id id, const modules::reload_context &reload) {
        // Commands and panels are stateless records: attach() re-created them already.
        (void) id;
        (void) reload;
    }

    u32 editor_module_consumer::live_use_count(modules::module_id id) const {
        (void) id;
        return 0;
    }
}
