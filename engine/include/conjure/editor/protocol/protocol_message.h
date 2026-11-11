#pragma once

#include "conjure/editor/editor_error.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <string>
#include <string_view>

// The wire messages (docs/design/04_editor.md D7, 2.3): newline-delimited JSON, UTF-8, at most MAX_MESSAGE_BYTES
// per line. Requests carry a caller id (integer, string or null, echoed verbatim), a command name and an
// object of arguments; responses echo the id and carry either a result or an error; events carry the
// monotonic sequence number and the frame they were emitted in.
//
//   -> {"id":12,"cmd":"entity.create","args":{"name":"Player"}}
//   <- {"id":12,"ok":true,"result":{"guid":"...","revision":42}}
//   <- {"id":13,"ok":false,"error":{"code":"ENTITY_NOT_FOUND","message":"...","details":{}}}
//   <- {"event":"selection.changed","seq":913,"frame":8812,"data":{"guids":["..."]}}

namespace conjure::editor {

    inline constexpr u32 PROTOCOL_VERSION = 1;
    inline constexpr usize MAX_MESSAGE_BYTES = 16 * MIB;

    struct protocol_request {
        core::json::document id = {};                       // integer, string or null; echoed verbatim
        std::string command = "";
        core::json::document args = core::json::document::object();
        u64 coalesceKey = 0;
        u32 clientId = 0;
    };

    struct protocol_response {
        core::json::document id = {};
        core::json::document result = core::json::document::object();      // when ok
        core::json::document errorDetails = core::json::document::object();
        std::string errorMessage = "";
        protocol_error_code errorCode = protocol_error_code::NONE;
        u32 clientId = 0;                                   // routing only, never serialised
        bool ok = false;
    };

    struct protocol_event {
        core::json::document data = core::json::document::object();
        std::string name = "";                              // "log", "selection.changed", ...
        u64 seq = 0;
        u64 frame = 0;
    };

    /// <summary>
    /// Parses one line. PARSE_ERROR when it is not JSON, INVALID_REQUEST when it is not an object with a
    /// string "cmd" and an object (or absent) "args", or when "id" is neither integer, string nor null, or
    /// "coalesceKey" is not an unsigned integer; MESSAGE_TOO_LARGE above MAX_MESSAGE_BYTES. A missing id
    /// yields a null id.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<protocol_request> parse_request(std::string_view line);

    /// <summary>
    /// Parses one line of a response or event (the client side; the headless runner's tests). PARSE_ERROR /
    /// INVALID_REQUEST as above. A line with "event" is an event; one with "ok" is a response.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<protocol_response> parse_response(std::string_view line);
    [[nodiscard]] CONJURE_API core::result<protocol_event> parse_event(std::string_view line);

    /// <summary>
    /// The wire form without the trailing newline: {"id":..,"ok":true,"result":{..}} or
    /// {"id":..,"ok":false,"error":{"code":"..","message":"..","details":{..}}}.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string serialise(const protocol_response &response);

    /// <summary>
    /// {"event":"..","seq":n,"frame":n,"data":{..}} without the trailing newline.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string serialise(const protocol_event &event);

    /// <summary>
    /// {"id":..,"cmd":"..","args":{..}} (plus "coalesceKey" when non-zero) without the trailing newline.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string serialise(const protocol_request &request);

    /// <summary>
    /// A successful response echoing the id.
    /// </summary>
    [[nodiscard]] CONJURE_API protocol_response make_ok_response(
      const core::json::value &id,
      core::json::document result
    );

    /// <summary>
    /// A failed response echoing the id.
    /// </summary>
    [[nodiscard]] CONJURE_API protocol_response make_error_response(
      const core::json::value &id,
      protocol_error_code code,
      std::string message,
      core::json::document details = core::json::document::object()
    );

    /// <summary>
    /// A document holding a null id (the id of a response to an unparseable line).
    /// </summary>
    [[nodiscard]] CONJURE_API core::json::document null_id();

    /// <summary>
    /// A document holding an integer id.
    /// </summary>
    [[nodiscard]] CONJURE_API core::json::document make_id(i64 value);

    /// <summary>
    /// A document holding a string id.
    /// </summary>
    [[nodiscard]] CONJURE_API core::json::document make_id(std::string_view value);
}
