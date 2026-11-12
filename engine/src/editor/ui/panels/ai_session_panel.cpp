#include "conjure/editor/ui/panels/ai_session_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/log_category.h"
#include "conjure/editor/protocol/protocol_server.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <utility>

namespace conjure::editor::ui {

    ai_session_panel::ai_session_panel() :
        editor_panel("AI Session", false) {}

    ai_session_panel::~ai_session_panel() {
        if (mConnected) {
            mClient.disconnect();
        }
    }

    void ai_session_panel::update(editor_context &context) {
        (void) context;
        if (!mConnected) {
            return;
        }
        for (std::string &line : mClient.receive_all()) {
            append(std::move(line));
        }
        if (!mClient.is_connected()) {
            mConnected = false;
            append("[loopback client disconnected]");
        }
    }

    void ai_session_panel::draw_contents(editor_context &context) {
        protocol_server *server = context.server();
        if (server == nullptr) {
            support::hint_text("No protocol server in this session.");
        } else {
            const std::string status = std::format(
              "{} | port {} | {} client(s) | token {}",
              server->is_running() ? "running" : "stopped",
              server->bound_port(),
              server->client_count(),
              server->token().empty() ? "none" : "set"
            );
            ImGui::TextUnformatted(status.c_str());
            if (!server->port_file().empty()) {
                const std::string portFile = server->port_file().generic_string();
                ImGui::TextDisabled("port file: %s", portFile.c_str());
            }
        }
        ImGui::Separator();
        if (ImGui::BeginChild(
          "transcript",
          ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()),
          ImGuiChildFlags_None,
          ImGuiWindowFlags_HorizontalScrollbar
        )) {
            for (const std::string &line : mTranscript) {
                ImGui::TextUnformatted(line.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
        ImGui::SetNextItemWidth(-80.0f);
        const bool entered = ImGui::InputTextWithHint(
          "##request",
          "{\"id\":1,\"cmd\":\"editor.state\"} or cmd args-json",
          &mInput,
          ImGuiInputTextFlags_EnterReturnsTrue
        );
        // Enter deactivates the field; take the focus back so the next request can be typed straight away.
        if (entered) {
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::SameLine();
        const bool clicked = ImGui::Button("Send");
        if ((entered || clicked) && !mInput.empty() && server != nullptr) {
            send(context, mInput);
            mInput.clear();
        }
    }

    void ai_session_panel::send(editor_context &context, std::string_view line) {
        ensure_connected(context);
        if (!mConnected) {
            append("[cannot send: no loopback connection]");
            return;
        }
        std::string text = std::string(line);
        // Shorthand: "cmd {args}" becomes a request with the next id.
        if (!text.empty() && text.front() != '{') {
            const usize space = text.find(' ');
            const std::string command = text.substr(0, space);
            const std::string args = space == std::string::npos ? std::string("{}") : text.substr(space + 1);
            text = std::format(
              "{{\"id\":{},\"cmd\":\"{}\",\"args\":{}}}",
              mSent + 1,
              command,
              args.empty() ? "{}" : args
            );
        }
        mClient.send(text);
        ++mSent;
        append("> " + text);
    }

    void ai_session_panel::ensure_connected(editor_context &context) {
        if (mConnected) {
            return;
        }
        protocol_server *server = context.server();
        if (server == nullptr) {
            return;
        }
        // The server owns one loopback transport for every in-process client; reconnecting reuses it.
        mClient = server->local_loopback().connect();
        mConnected = true;
        append("[connected through the in-process loopback transport]");
        if (server->token().empty()) {
            return;
        }
        // With --ai-token the first request must be session.hello or the server disconnects. The panel runs in
        // the editor's process, so it presents the token itself; it is never echoed to the transcript.
        const core::json::document hello = core::json::object({
          {"id", "ai_session_panel.hello"},
          {"cmd", "session.hello"},
          {"args", core::json::object({{"token", server->token()}, {"clientName", "AI Session panel"}})},
        });
        mClient.send(hello.to_string());
        append("> session.hello (token hidden)");
    }

    void ai_session_panel::append(std::string line) {
        mTranscript.push_back(std::move(line));
        while (mTranscript.size() > MAX_TRANSCRIPT_LINES) {
            mTranscript.pop_front();
        }
    }
}
