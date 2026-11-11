#include "conjure/editor/ui/panels/export_dialog.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/protocol/protocol_message.h"

#include "conjure/engine/project.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace conjure::editor::ui {

    namespace {
        /// <summary>
        /// Shared between the dialog and the callbacks it registers: survives the dialog so a late response
        /// never touches a dead panel.
        /// </summary>
        struct export_state {
            std::optional<protocol_response> response = std::nullopt;
            std::string message = "";
            f32 fraction = 0.0f;
            u32 clientId = 0;
            bool registered = false;
        };

        std::shared_ptr<export_state> &state_slot() {
            static std::shared_ptr<export_state> slot = nullptr;
            return slot;
        }
    }

    export_dialog::export_dialog() :
        editor_panel("Export", false) {}

    export_dialog::~export_dialog() = default;

    void export_dialog::update(editor_context &context) {
        std::shared_ptr<export_state> &state = state_slot();
        if (!mRunning || state == nullptr) {
            return;
        }
        if (!state->message.empty()) {
            mStatus = state->message;
            mProgress = state->fraction;
        }
        if (!state->response.has_value()) {
            return;
        }
        const protocol_response &response = *state->response;
        if (response.ok) {
            mStatus = std::format(
              "exported {} asset(s), {} byte(s) to {}",
              response.result.root()["assetCount"].as_u64(0),
              response.result.root()["bytes"].as_u64(0),
              response.result.root()["directory"].as_string("")
            );
            mProgress = 1.0f;
        } else {
            mStatus = std::format("export failed: {} {}", to_string(response.errorCode), response.errorMessage);
        }
        if (state->registered) {
            context.unregister_response_sink(state->clientId);
            context.events().unregister_client(state->clientId);
            state->registered = false;
        }
        mRunning = false;
        state.reset();
    }

    void export_dialog::draw_contents(editor_context &context) {
        if (!context.has_project()) {
            support::hint_text("Open a project to export it.");
            return;
        }
        if (mDirectory.empty() && context.project() != nullptr) {
            mDirectory = (context.project()->root_directory() / "export").generic_string();
        }
        ImGui::BeginDisabled(mRunning);
        ImGui::InputText("Output directory", &mDirectory);
        ImGui::InputText("Configuration", &mConfiguration);
        if (ImGui::Button("Export")) {
            start_export(context);
        }
        ImGui::EndDisabled();
        if (mRunning) {
            ImGui::SameLine();
            ImGui::ProgressBar(mProgress, ImVec2(-1.0f, 0.0f));
        }
        if (!mStatus.empty()) {
            ImGui::TextWrapped("%s", mStatus.c_str());
        }
    }

    void export_dialog::start_export(editor_context &context) {
        if (mRunning || mDirectory.empty() || !context.has_project()) {
            return;
        }
        std::shared_ptr<export_state> state = std::make_shared<export_state>();
        state->clientId = context.events().register_client([state](const protocol_event &event) {
            if (
              event.name == to_string(
                editor_event::PROGRESS
              ) && event.data.root()["operation"].as_string("") == "project.export"
            ) {
                state->message = std::string(event.data.root()["message"].as_string(""));
                state->fraction = event.data.root()["fraction"].as_f32(0.0f);
            }
        });
        context.events().set_subscriptions(state->clientId, event_bit(editor_event::PROGRESS));
        context.register_response_sink(state->clientId, [state](const protocol_response &response) {
            state->response = response;
        });
        state->registered = true;
        protocol_request request = {};
        request.id = make_id("export");
        request.command = "project.export";
        request.args = core::json::object({{"directory", mDirectory}, {"config", mConfiguration}});
        request.clientId = state->clientId;
        mRunning = true;
        mProgress = 0.0f;
        mStatus = "exporting...";
        state_slot() = state;
        std::optional<protocol_response> immediate = context.dispatcher().dispatch(request);
        if (immediate.has_value()) {
            state->response = std::move(immediate);
        }
    }
}
