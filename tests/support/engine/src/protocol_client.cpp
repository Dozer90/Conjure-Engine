#include "conjure/testing/protocol_client.h"

#include "conjure/testing/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/log.h"

#include <optional>
#include <utility>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// protocol_channel

protocol_channel::protocol_channel() {}

protocol_channel::~protocol_channel() {}


/*===========================================================================*/
/*===========================================================================*/
// scripted_protocol_channel

scripted_protocol_channel::scripted_protocol_channel(handler serverHandler) :
    mHandler(std::move(serverHandler)) {}

scripted_protocol_channel::~scripted_protocol_channel() {}

result<void> scripted_protocol_channel::send_line(std::string_view line) {
    if (!mOpen) {
        return make_error(error_code::INVALID_STATE, "the protocol channel is closed");
    }
    mSent.emplace_back(line);
    if (!mHandler) {
        return result<void>::success();
    }

    // A list per call: a handler that sends through this same channel again (a fake server forwarding a
    // request) must not clear or grow the list the outer call is still reading.
    std::vector<std::string> produced = {};
    mHandler(line, produced);
    for (std::string &reply : produced) {
        mInbound.push_back(std::move(reply));
    }
    return result<void>::success();
}

result<std::string> scripted_protocol_channel::receive_line() {
    if (mInbound.empty()) {
        if (!mOpen) {
            return make_error(error_code::INVALID_STATE, "the protocol channel is closed");
        }
        return make_error(error_code::TIMEOUT, "the scripted server produced no line for the request");
    }
    std::string line = std::move(mInbound.front());
    mInbound.pop_front();
    return line;
}

bool scripted_protocol_channel::is_open() const noexcept {
    return mOpen;
}

void scripted_protocol_channel::close() {
    mOpen = false;
}

void scripted_protocol_channel::inject_line(std::string_view line) {
    mInbound.emplace_back(line);
}


/*===========================================================================*/
/*===========================================================================*/
// protocol_client

protocol_client::protocol_client(std::unique_ptr<protocol_channel> channel) :
    mChannel(std::move(channel)) {
    CONJURE_ASSERT(mChannel != nullptr, "protocol_client needs a channel");
}

protocol_client::~protocol_client() {}

result<json::document> protocol_client::send(std::string_view command, const json::document &params) {
    if (mChannel == nullptr || !mChannel->is_open()) {
        return make_error(error_code::INVALID_STATE, "protocol_client is not connected");
    }

    const u64 id = mNextId;
    ++mNextId;
    mLastRequestId = id;
    ++mRequestCount;

    json::document request = json::document::object();
    request.set("id", id);
    request.set("cmd", command);
    request.set("args", params);
    CONJURE_TRY(mChannel->send_line(request.to_string()));
    return await_response(id, false);
}

result<json::document> protocol_client::send(std::string_view command) {
    return send(command, json::document::object());
}

result<json::document> protocol_client::send_raw(std::string_view rawMessage) {
    if (mChannel == nullptr || !mChannel->is_open()) {
        return make_error(error_code::INVALID_STATE, "protocol_client is not connected");
    }
    ++mRequestCount;
    CONJURE_TRY(mChannel->send_line(rawMessage));
    return await_response(0, true);
}

void protocol_client::clear_events() {
    mEvents.clear();
}

result<std::optional<json::document>> protocol_client::receive() {
    if (mChannel == nullptr) {
        return make_error(error_code::INVALID_STATE, "protocol_client is not connected");
    }
    CONJURE_TRY_ASSIGN(const std::string line, mChannel->receive_line());
    result<json::document> parsed = json::document::parse(line);
    if (!parsed.has_value()) {
        return make_error(
          error_code::PARSE_FAILURE,
          "the server wrote a line that is not JSON ({}): {}",
          parsed.get_error().message(),
          line
        );
    }
    json::document message = std::move(parsed).value();
    if (is_event(message.root())) {
        mEvents.push_back(std::move(message));
        return std::optional<json::document>(std::nullopt);
    }
    return std::optional<json::document>(std::move(message));
}

bool protocol_client::is_connected() const noexcept {
    return mChannel != nullptr && mChannel->is_open();
}

void protocol_client::disconnect() {
    if (mChannel != nullptr) {
        mChannel->close();
    }
}

bool protocol_client::is_event(const json::value &message) noexcept {
    return message.is_object() && message.contains("event");
}

bool protocol_client::response_ok(const json::document &response) noexcept {
    const json::value root = response.root();
    return root.is_object() && root["ok"].as_bool(false);
}

std::string_view protocol_client::error_code_of(const json::document &response) noexcept {
    const json::value root = response.root();
    if (!root.is_object()) {
        return {};
    }
    return root["error"]["code"].as_string({});
}

std::string_view protocol_client::error_message_of(const json::document &response) noexcept {
    const json::value root = response.root();
    if (!root.is_object()) {
        return {};
    }
    return root["error"]["message"].as_string({});
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

result<json::document> protocol_client::await_response(u64 id, bool anyId) {
    for (u32 attempt = 0; attempt < MAX_LINES_PER_REQUEST; ++attempt) {
        CONJURE_TRY_ASSIGN(const std::string line, mChannel->receive_line());
        result<json::document> parsed = json::document::parse(line);
        if (!parsed.has_value()) {
            return make_error(
              error_code::PARSE_FAILURE,
              "the server wrote a line that is not JSON ({}): {}",
              parsed.get_error().message(),
              line
            );
        }

        json::document message = std::move(parsed).value();
        const json::value root = message.root();
        if (is_event(root)) {
            mEvents.push_back(std::move(message));
            continue;
        }
        if (anyId) {
            return message;
        }

        // Ids travel as the integer the client sent; a string or null id can only belong to another
        // client or to a request without an id, neither of which this client ever sends.
        if (root.is_object()) {
            const json::value echoed = root["id"];
            if (echoed.is_valid() && echoed.is_integer() && echoed.as_u64(0) == id) {
                return message;
            }
        }
        ++mDroppedResponses;
        CONJURE_LOG_WARN(LOG_TESTING, "protocol_client: dropped a response for an unknown request: {}", line);
    }
    return make_error(
      error_code::TIMEOUT,
      "no response for request {} within {} inbound lines",
      id,
      MAX_LINES_PER_REQUEST
    );
}
