#include "editor/core/protocol/stdio_transport.h"

#include <array>
#include <istream>
#include <ostream>
#include <utility>

namespace conjure::editor {

    stdio_transport::stdio_transport(std::istream &input, std::ostream &output, bool threaded, usize lineLimit) :
        mState(std::make_shared<reader_state>()),
        mOutput(&output),
        mThreaded(threaded) {
        mState->input = &input;
        mState->splitter = line_splitter(lineLimit == 0 ? MAX_MESSAGE_BYTES : lineLimit);
    }

    stdio_transport::~stdio_transport() {
        stop();
    }

    core::result<void> stdio_transport::start() {
        if (mStarted) {
            return core::result<void>::success();
        }
        mStarted = true;
        if (mThreaded) {
            // The lambda owns a reference to the shared state and nothing of the transport itself.
            std::shared_ptr<reader_state> shared = mState;
            mReader = std::make_unique<core::jobs::dedicated_thread>("editor-stdin", [shared](std::stop_token stop) {
                while (!stop.stop_requested() && read_chunk(*shared)) {
                }
                const std::lock_guard<std::mutex> lock(shared->mutex);
                shared->eof = true;
            });
        }
        return core::result<void>::success();
    }

    void stdio_transport::stop() {
        if (!mStarted) {
            return;
        }
        mStarted = false;
        if (mReader != nullptr) {
            mReader->request_stop();
            // The thread may be blocked in a read on a terminal: it ends when the stream closes. Joining here
            // would hang the editor's exit, so the thread is released and joins with the process.
            bool finished = !mReader->joinable();
            if (!finished) {
                const std::lock_guard<std::mutex> lock(mState->mutex);
                finished = mState->eof;
            }
            if (finished) {
                mReader.reset();
            } else {
                // Intentionally leaked: the OS reclaims the thread with the process (documented limitation of
                // a blocking stdin reader). The thread only reaches the shared state, which it keeps alive.
                [[maybe_unused]] core::jobs::dedicated_thread *blocked = mReader.release();
            }
        }
    }

    void stdio_transport::poll(
      std::vector<u32> &outOpened,
      std::vector<inbound_line> &outLines,
      std::vector<u32> &outClosed
    ) {
        if (!mStarted) {
            return;
        }
        if (!mOpened) {
            mOpened = true;
            mClientId = allocate_client_id();
            outOpened.push_back(mClientId);
        }
        if (mDisconnected) {
            if (!mClosedReported) {
                mClosedReported = true;
                outClosed.push_back(mClientId);
            }
            return;
        }
        if (!mThreaded) {
            while (read_chunk(*mState)) {
            }
            const std::lock_guard<std::mutex> lock(mState->mutex);
            mState->eof = true;
        }
        std::deque<inbound_line> lines = {};
        bool eof = false;
        {
            const std::lock_guard<std::mutex> lock(mState->mutex);
            lines.swap(mState->lines);
            eof = mState->eof;
        }
        for (inbound_line &line : lines) {
            line.clientId = mClientId;
            outLines.push_back(std::move(line));
        }
        // The close is reported one poll after the end of input so the responses to the last lines still
        // reach the output stream.
        if (eof && !mClosedReported) {
            if (mEofSeen) {
                mClosedReported = true;
                mDisconnected = true;
                outClosed.push_back(mClientId);
            } else {
                mEofSeen = true;
            }
        }
    }

    void stdio_transport::send(u32 clientId, std::string_view line) {
        if (clientId != mClientId || mDisconnected || mOutput == nullptr) {
            return;
        }
        mOutput->write(line.data(), static_cast<std::streamsize>(line.size()));
        mOutput->put('\n');
        mOutput->flush();
    }

    void stdio_transport::disconnect(u32 clientId) {
        if (clientId == mClientId) {
            mDisconnected = true;
        }
    }

    void stdio_transport::flush() {
        if (mOutput != nullptr) {
            mOutput->flush();
        }
    }

    usize stdio_transport::pending_bytes(u32 clientId) const noexcept {
        (void) clientId;
        return 0;
    }

    transport_kind stdio_transport::kind() const noexcept {
        return transport_kind::STDIO;
    }

    usize stdio_transport::client_count() const noexcept {
        return mOpened && !mDisconnected ? 1 : 0;
    }

    bool stdio_transport::read_chunk(reader_state &state) {
        std::istream &input = *state.input;
        if (!input.good()) {
            return false;
        }
        std::array<char, READ_CHUNK_BYTES> chunk = {};
        // getline stops at the newline (extracted, not stored), when the buffer holds READ_CHUNK_BYTES - 1
        // characters (failbit, nothing lost) or at the end of input; the splitter reassembles the pieces and
        // enforces the line limit as the bytes stream in.
        input.getline(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        const usize extracted = static_cast<usize>(input.gcount());
        if (input.bad()) {
            return false;
        }
        if (input.eof()) {
            if (extracted > 0) {
                // A final line without a terminator still counts as a line.
                state.splitter.feed(std::string_view(chunk.data(), extracted));
                state.splitter.feed("\n");
                queue_frames(state);
            }
            return false;
        }
        if (input.fail()) {
            // The buffer filled before a newline: the line continues in the next chunk.
            input.clear();
            state.splitter.feed(std::string_view(chunk.data(), extracted));
            queue_frames(state);
            return true;
        }
        // The newline was extracted and is not in the buffer.
        state.splitter.feed(std::string_view(chunk.data(), extracted > 0 ? extracted - 1 : 0));
        state.splitter.feed("\n");
        queue_frames(state);
        return true;
    }

    void stdio_transport::queue_frames(reader_state &state) {
        std::vector<inbound_line> framed = {};
        std::string line = {};
        for (
          line_splitter::frame kind = state.splitter.next_frame(line);
          kind != line_splitter::frame::NONE;
          kind = state.splitter.next_frame(line)
        ) {
            framed.push_back(inbound_line{std::move(line), 0, kind == line_splitter::frame::OVERSIZED});
            line = {};
        }
        if (framed.empty()) {
            return;
        }
        const std::lock_guard<std::mutex> lock(state.mutex);
        for (inbound_line &entry : framed) {
            state.lines.push_back(std::move(entry));
        }
    }
}
