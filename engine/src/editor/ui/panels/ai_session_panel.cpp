#include "conjure/editor/ui/panels/ai_session_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/log_category.h"
#include "conjure/editor/protocol/protocol_server.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <filesystem>
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
            if (!server->token_file().empty()) {
                // Where the tokens are, for whoever sets up a client; a token itself is never drawn.
                const std::string tokenFile = server->token_file().generic_string();
                ImGui::TextDisabled("token file: %s", tokenFile.c_str());
            }
            if (!server->inspect_token_file().empty()) {
                const std::string tokenFile = server->inspect_token_file().generic_string();
                ImGui::TextDisabled("inspect-only token file: %s", tokenFile.c_str());
            }
            const std::filesystem::path auditFile = server->audit().file();
            if (!auditFile.empty()) {
                // Named whether or not it exists yet: the first recorded command creates it.
                const std::string auditText = auditFile.generic_string();
                ImGui::TextDisabled("audit log: %s", auditText.c_str());
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
        // The panel opens with session.hello, as every client should: the server disconnects a client whose
        // first line is not a request, and what is typed into the panel next may be anything. With --ai-token
        // or --ai-inspect-token the hello carries the full-control token (the panel runs in the editor's
        // process, so it presents the token itself: the user at the editor is not an inspect-only client);
        // the tokens the editor generates for its TCP port do not gate the in-process loopback. The token is
        // never echoed to the transcript.
        const bool presentToken = server->requires_token(transport_kind::LOOPBACK);
        core::json::document args = core::json::object({{"clientName", "AI Session panel"}});
        if (presentToken) {
            args.set("token", std::string_view(server->token()));
        }
        const core::json::document hello = core::json::object({
          {"id", "ai_session_panel.hello"},
          {"cmd", "session.hello"},
          {"args", args},
        });
        mClient.send(hello.to_string());
        append(presentToken ? "> session.hello (token hidden)" : "> session.hello");
    }

    void ai_session_panel::append(std::string line) {
        mTranscript.push_back(std::move(line));
        while (mTranscript.size() > MAX_TRANSCRIPT_LINES) {
            mTranscript.pop_front();
        }
    }
}
