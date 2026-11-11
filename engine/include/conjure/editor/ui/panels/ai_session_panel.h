#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/protocol/loopback_transport.h"
#include "conjure/editor/ui/editor_panel.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <deque>
#include <string>
#include <string_view>

// The AI session panel: the server's port and clients, a request line sent through an in-process loopback
// client, and the transcript of responses and events that came back.

namespace conjure::editor::ui {

    class CONJURE_API ai_session_panel final : public editor_panel {
        public:
            static constexpr usize MAX_TRANSCRIPT_LINES = 500;

        private:
            loopback_client mClient = {};
            std::deque<std::string> mTranscript = {};
            std::string mInput = "";
            u32 mSent = 0;
            bool mConnected = false;

        public:
            ai_session_panel();
            ~ai_session_panel() override;

        public:
            void update(editor_context &context) override;
            void draw_contents(editor_context &context) override;

            /// <summary>
            /// Sends one request line through the loopback client (connecting on first use).
            /// </summary>
            void send(editor_context &context, std::string_view line);

            [[nodiscard]] const std::deque<std::string> &transcript() const noexcept {
                return mTranscript;
            }

            [[nodiscard]] u32 sent_count() const noexcept {
                return mSent;
            }

            [[nodiscard]] bool is_connected() const noexcept {
                return mConnected;
            }

        private:
            void ensure_connected(editor_context &context);
            void append(std::string line);
    };
}
