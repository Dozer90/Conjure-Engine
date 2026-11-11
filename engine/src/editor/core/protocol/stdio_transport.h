#pragma once

#include "conjure/editor/protocol/line_splitter.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_transport.h"

#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <deque>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// The stdin/stdout transport (docs/design/04_editor.md 3.12): one client for the process lifetime. The input is
// read in fixed-size chunks through a line_splitter, so a line past the limit is discarded as it streams in
// instead of being accumulated first. With a reader thread ("editor-stdin") the chunks are read on that thread
// and the framed lines pushed into a mutex-guarded queue the main thread drains in poll(); without one (tests
// over string streams) poll() reads the stream itself. The reader state is shared with the thread through a
// shared_ptr: a thread blocked in a read when the transport is destroyed (the documented --ai-stdio exit path)
// keeps the state alive and never touches the transport. Responses and events are written and flushed on the
// main thread.

namespace conjure::editor {

    class stdio_transport final : public protocol_transport {
        public:
            static constexpr usize READ_CHUNK_BYTES = 64 * 1024;

        private:
            /// <summary>
            /// What the reading side (the thread, or poll() in synchronous mode) and the transport share.
            /// </summary>
            struct reader_state {
                std::istream *input = nullptr;
                line_splitter splitter{};                   // used by the reading side only
                std::mutex mutex = {};
                std::deque<inbound_line> lines = {};        // guarded by mutex; clientId filled in by poll()
                bool eof = false;                           // guarded by mutex
            };

            std::shared_ptr<reader_state> mState = nullptr;
            std::ostream *mOutput = nullptr;
            std::unique_ptr<core::jobs::dedicated_thread> mReader = nullptr;
            u32 mClientId = 0;
            bool mThreaded = true;
            bool mStarted = false;
            bool mOpened = false;
            bool mEofSeen = false;                           // EOF noticed by a poll; the next poll reports the close
            bool mClosedReported = false;
            bool mDisconnected = false;

        public:
            /// <summary>
            /// 'threaded' starts the reader thread at start(); false reads synchronously in poll().
            /// </summary>
            stdio_transport(
              std::istream &input,
              std::ostream &output,
              bool threaded,
              usize lineLimit = MAX_MESSAGE_BYTES
            );
            ~stdio_transport() override;

        public:
            [[nodiscard]] core::result<void> start() override;
            void stop() override;
            void poll(
              std::vector<u32> &outOpened,
              std::vector<inbound_line> &outLines,
              std::vector<u32> &outClosed
            ) override;
            void send(u32 clientId, std::string_view line) override;
            void disconnect(u32 clientId) override;
            void flush() override;
            [[nodiscard]] usize pending_bytes(u32 clientId) const noexcept override;
            [[nodiscard]] transport_kind kind() const noexcept override;
            [[nodiscard]] usize client_count() const noexcept override;

            /// <summary>
            /// The id of the one client (0 before the first poll()).
            /// </summary>
            [[nodiscard]] u32 client_id() const noexcept {
                return mClientId;
            }

        private:
            /// <summary>
            /// Reads one chunk (at most READ_CHUNK_BYTES, stopping at a newline) into the splitter and queues
            /// the frames it completed. False once the stream is exhausted or broken.
            /// </summary>
            [[nodiscard]] static bool read_chunk(reader_state &state);
            static void queue_frames(reader_state &state);
    };
}
