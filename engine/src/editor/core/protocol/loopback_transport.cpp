#include "conjure/editor/protocol/loopback_transport.h"

#include "conjure/editor/protocol/line_splitter.h"

#include <algorithm>
#include <utility>

namespace conjure::editor {

    /// <summary>
    /// Shared between a loopback_client and the transport. Every member is guarded by the mutex.
    /// </summary>
    struct loopback_client::state {
        mutable std::mutex mutex = {};
        line_splitter inbound{};                         // client -> server, split into lines by the server
        std::deque<std::string> outbound = {};              // server -> client
        usize outboundBytes = 0;
        u32 clientId = 0;
        bool clientOpen = true;                             // client side still attached
        bool serverOpen = true;                             // server side still attached
        bool accepted = false;
    };

    /*===========================================================================*/
    /*===========================================================================*/
    // loopback_client

    loopback_client::loopback_client() = default;
    loopback_client::~loopback_client() = default;

    loopback_client::loopback_client(std::shared_ptr<state> shared) noexcept :
        mState(std::move(shared)) {}

    void loopback_client::send(std::string_view line) {
        if (mState == nullptr) {
            return;
        }
        const std::lock_guard<std::mutex> lock(mState->mutex);
        if (!mState->clientOpen) {
            return;
        }
        mState->inbound.feed(line);
        if (line.empty() || line.back() != '\n') {
            mState->inbound.feed("\n");
        }
    }

    void loopback_client::send_raw(std::string_view bytes) {
        if (mState == nullptr) {
            return;
        }
        const std::lock_guard<std::mutex> lock(mState->mutex);
        if (mState->clientOpen) {
            mState->inbound.feed(bytes);
        }
    }

    std::optional<std::string> loopback_client::receive() {
        if (mState == nullptr) {
            return std::nullopt;
        }
        const std::lock_guard<std::mutex> lock(mState->mutex);
        if (mState->outbound.empty()) {
            return std::nullopt;
        }
        std::string line = std::move(mState->outbound.front());
        mState->outbound.pop_front();
        mState->outboundBytes -= std::min(mState->outboundBytes, line.size() + 1);
        return line;
    }

    std::vector<std::string> loopback_client::receive_all() {
        std::vector<std::string> lines = {};
        while (std::optional<std::string> line = receive()) {
            lines.push_back(std::move(*line));
        }
        return lines;
    }

    usize loopback_client::pending() const {
        if (mState == nullptr) {
            return 0;
        }
        const std::lock_guard<std::mutex> lock(mState->mutex);
        return mState->outbound.size();
    }

    void loopback_client::disconnect() {
        if (mState == nullptr) {
            return;
        }
        const std::lock_guard<std::mutex> lock(mState->mutex);
        mState->clientOpen = false;
    }

    bool loopback_client::is_connected() const {
        if (mState == nullptr) {
            return false;
        }
        const std::lock_guard<std::mutex> lock(mState->mutex);
        return mState->clientOpen && mState->serverOpen;
    }

    u32 loopback_client::client_id() const {
        if (mState == nullptr) {
            return 0;
        }
        const std::lock_guard<std::mutex> lock(mState->mutex);
        return mState->accepted ? mState->clientId : 0;
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // loopback_transport

    loopback_transport::loopback_transport(usize lineLimit) :
        mLineLimit(lineLimit == 0 ? MAX_MESSAGE_BYTES : lineLimit) {}

    loopback_transport::~loopback_transport() {
        stop();
    }

    loopback_client loopback_transport::connect() {
        std::shared_ptr<loopback_client::state> shared = std::make_shared<loopback_client::state>();
        shared->inbound = line_splitter(mLineLimit);
        mPendingConnects.push_back(shared);
        return loopback_client(shared);
    }

    core::result<void> loopback_transport::start() {
        mStarted = true;
        return core::result<void>::success();
    }

    void loopback_transport::stop() {
        for (const std::shared_ptr<loopback_client::state> &shared : mClients) {
            const std::lock_guard<std::mutex> lock(shared->mutex);
            shared->serverOpen = false;
        }
        for (const std::shared_ptr<loopback_client::state> &shared : mPendingConnects) {
            const std::lock_guard<std::mutex> lock(shared->mutex);
            shared->serverOpen = false;
        }
        mClients.clear();
        mPendingConnects.clear();
        mStarted = false;
    }

    void loopback_transport::poll(
      std::vector<u32> &outOpened,
      std::vector<inbound_line> &outLines,
      std::vector<u32> &outClosed
    ) {
        if (!mStarted) {
            return;
        }
        for (std::shared_ptr<loopback_client::state> &shared : mPendingConnects) {
            const u32 id = allocate_client_id();
            {
                const std::lock_guard<std::mutex> lock(shared->mutex);
                shared->clientId = id;
                shared->accepted = true;
            }
            mClients.push_back(shared);
            outOpened.push_back(id);
        }
        mPendingConnects.clear();

        for (usize index = 0; index < mClients.size();) {
            std::shared_ptr<loopback_client::state> shared = mClients[index];
            bool closed = false;
            {
                const std::lock_guard<std::mutex> lock(shared->mutex);
                // Frames in arrival order: an oversized marker stays where the bad line was.
                std::string line = {};
                for (
                  line_splitter::frame kind = shared->inbound.next_frame(line);
                  kind != line_splitter::frame::NONE;
                  kind = shared->inbound.next_frame(line)
                ) {
                    outLines.push_back(inbound_line{
                      std::move(line),
                      shared->clientId,
                      kind == line_splitter::frame::OVERSIZED,
                    });
                    line = {};
                }
                closed = !shared->clientOpen || !shared->serverOpen;
            }
            if (closed) {
                outClosed.push_back(shared->clientId);
                {
                    const std::lock_guard<std::mutex> lock(shared->mutex);
                    shared->serverOpen = false;
                }
                mClients.erase(mClients.begin() + static_cast<isize>(index));
                continue;
            }
            ++index;
        }
    }

    void loopback_transport::send(u32 clientId, std::string_view line) {
        const std::shared_ptr<loopback_client::state> shared = find(clientId);
        if (shared == nullptr) {
            return;
        }
        const std::lock_guard<std::mutex> lock(shared->mutex);
        shared->outbound.emplace_back(line);
        shared->outboundBytes += line.size() + 1;
    }

    void loopback_transport::disconnect(u32 clientId) {
        const std::shared_ptr<loopback_client::state> shared = find(clientId);
        if (shared == nullptr) {
            return;
        }
        const std::lock_guard<std::mutex> lock(shared->mutex);
        shared->serverOpen = false;
    }

    void loopback_transport::flush() {
        // Lines are delivered at send(); nothing is buffered on the server side.
    }

    usize loopback_transport::pending_bytes(u32 clientId) const noexcept {
        const std::shared_ptr<loopback_client::state> shared = find(clientId);
        if (shared == nullptr) {
            return 0;
        }
        const std::lock_guard<std::mutex> lock(shared->mutex);
        return shared->outboundBytes;
    }

    transport_kind loopback_transport::kind() const noexcept {
        return transport_kind::LOOPBACK;
    }

    usize loopback_transport::client_count() const noexcept {
        return mClients.size();
    }

    std::shared_ptr<loopback_client::state> loopback_transport::find(u32 clientId) const noexcept {
        for (const std::shared_ptr<loopback_client::state> &shared : mClients) {
            const std::lock_guard<std::mutex> lock(shared->mutex);
            if (shared->clientId == clientId) {
                return shared;
            }
        }
        return nullptr;
    }
}
