#pragma once

#include "conjure/editor/editor_command.h"
#include "conjure/editor/editor_context.h"

#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <imgui.h>

#include <string>
#include <string_view>

// Shared helpers of the panels (engine/src/editor/ui): every edit a panel makes goes through the dispatcher as
// a command_request from client 0 (the local UI), so it is validated, recorded for undo and announced exactly
// like a protocol request. Failures are logged as warnings and shown in the panel's status line.

namespace conjure::editor::ui::support {

    /// <summary>
    /// Dispatches a command from the local UI client. A failure is logged (WARN) and returned.
    /// </summary>
    [[nodiscard]] core::result<core::json::document> run_command(
      editor_context &context,
      std::string_view name,
      core::json::document args,
      u64 coalesceKey = 0
    );

    /// <summary>
    /// run_command whose outcome the caller does not inspect (the log carries any failure).
    /// </summary>
    void fire_command(editor_context &context, std::string_view name, core::json::document args);

    /// <summary>
    /// A stable coalesce key for a widget: FNV-1a of the path and the entity guid, never zero.
    /// </summary>
    [[nodiscard]] u64 coalesce_key(std::string_view guid, std::string_view path);

    /// <summary>
    /// The error text of a failed command result ("INVALID_ARGS: args.name ...").
    /// </summary>
    [[nodiscard]] std::string error_text(const core::error &error);

    [[nodiscard]] inline ImVec2 to_imvec(const math::vec2 &value) noexcept {
        return ImVec2(value.x, value.y);
    }

    [[nodiscard]] inline math::vec2 from_imvec(const ImVec2 &value) noexcept {
        return math::vec2(value.x, value.y);
    }

    /// <summary>
    /// True while an ImGui frame is open on the current context (panels draw nothing otherwise).
    /// </summary>
    [[nodiscard]] bool frame_is_open() noexcept;

    /// <summary>
    /// Text with a tooltip when hovered.
    /// </summary>
    void help_marker(const char *text);

    /// <summary>
    /// A disabled-looking text row ("none selected").
    /// </summary>
    void hint_text(std::string_view text);
}
