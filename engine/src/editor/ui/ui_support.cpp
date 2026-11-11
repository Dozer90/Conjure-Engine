#include "editor/ui/ui_support.h"

#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/hash.h"

#include <imgui_internal.h>

#include <format>
#include <utility>

namespace conjure::editor::ui::support {

    core::result<core::json::document> run_command(
      editor_context &context,
      std::string_view name,
      core::json::document args,
      u64 coalesceKey
    ) {
        command_request request = {};
        request.name = std::string(name);
        request.args = std::move(args);
        request.clientId = 0;
        request.coalesceKey = coalesceKey;
        core::result<core::json::document> result = context.dispatcher().execute(request);
        if (!result.has_value()) {
            CONJURE_LOG_WARN(LOG_EDITOR, "ui: {} failed: {}", name, result.get_error().message());
        }
        return result;
    }

    void fire_command(editor_context &context, std::string_view name, core::json::document args) {
        (void) run_command(context, name, std::move(args), 0);
    }

    u64 coalesce_key(std::string_view guid, std::string_view path) {
        u64 hash = core::hash::fnv1a64(guid);
        hash = core::hash::fnv1a64(path, hash);
        return hash == 0 ? 1 : hash;
    }

    std::string error_text(const core::error &error) {
        return std::format("{}: {}", error.code_name(), error.message());
    }

    bool frame_is_open() noexcept {
        const ImGuiContext *current = ImGui::GetCurrentContext();
        if (current == nullptr) {
            return false;
        }
        // NewFrame() has run and Render()/EndFrame() has not: drawing outside that window corrupts ImGui's state.
        return current->WithinFrameScope && ImGui::GetIO().Fonts->IsBuilt();
    }

    void help_marker(const char *text) {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::SetTooltip("%s", text);
        }
    }

    void hint_text(std::string_view text) {
        ImGui::TextDisabled("%.*s", static_cast<int>(text.size()), text.data());
    }
}
