#pragma once

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

// The editor error category (docs/design/04_editor.md 2.3, 6). The protocol's stable error strings and the
// editor's own core::error codes are one enumeration: a handler returns make_editor_error(code, message) and
// the dispatcher writes to_string(code) into the response. Errors of other modules (ecs, scene, assets, core)
// that reach a response are mapped through classify_error().

namespace conjure::editor {

    /// <summary>
    /// Every error a protocol response may carry. The names travel on the wire as strings; the numeric
    /// values never do.
    /// </summary>
    enum class protocol_error_code : u16 {
        NONE = 0,
        PARSE_ERROR,            // the line is not JSON
        INVALID_REQUEST,        // the JSON is not a request object (missing cmd, non-object args)
        MESSAGE_TOO_LARGE,      // a line above MAX_MESSAGE_BYTES; the client is disconnected afterwards
        UNKNOWN_COMMAND,        // no descriptor with that name; details.didYouMean lists the closest names
        INVALID_ARGS,           // schema violation or a semantic argument error; the message names the path
        ENTITY_NOT_FOUND,       // an entity reference resolved to no live entity
        COMPONENT_NOT_FOUND,    // the entity does not carry the component
        TYPE_NOT_FOUND,         // no component, asset or script class type with that name
        PATH_NOT_FOUND,         // a reflection path does not resolve, or a file or folder does not exist
        ASSET_NOT_FOUND,        // an asset reference (uuid or asset path) names no asset in the database
        PROJECT_NOT_OPEN,       // the command needs a project
        SCENE_NOT_OPEN,         // the command needs a scene
        WRONG_PLAY_STATE,       // the command is refused in the current play state (details.state)
        NOT_AVAILABLE,          // the feature is compiled out, the backend is null or a tool is missing
        IO_ERROR,               // a file or folder that exists could not be read, or a write failed
        COMPILE_FAILED,         // the managed build or a module build reported errors
        BUSY,                   // too many pending asynchronous requests for the client
        TIMEOUT,                // an asynchronous command did not complete in time
        CANCELLED,              // the client disconnected before an asynchronous reply
        UNAUTHORISED,           // the first request of a client was not a valid session.hello
        INTERNAL,               // an invariant failed; the message says which
        COUNT
    };

    /// <summary>
    /// The wire spelling of a code ("ENTITY_NOT_FOUND"); "" for values outside the enumeration.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(protocol_error_code code) noexcept;

    /// <summary>
    /// The enumerator name for the error category (same text as to_string).
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view editor_error_name(protocol_error_code code) noexcept;

    /// <summary>
    /// Parses a wire spelling (exact, case-sensitive). nullopt for anything else, including "NONE".
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<protocol_error_code> parse_error_code(std::string_view text) noexcept;

    /// <summary>
    /// Every code except NONE and COUNT, in enumeration order (editor.schema lists them).
    /// </summary>
    [[nodiscard]] CONJURE_API std::span<const protocol_error_code> all_error_codes() noexcept;
}

CONJURE_DECLARE_ERROR_CATEGORY(
  conjure::editor::protocol_error_code,
  "editor",
  conjure::editor::editor_error_name
);

namespace conjure::editor {

    /// <summary>
    /// Builds an error of the editor category.
    /// </summary>
    [[nodiscard]] inline core::error make_editor_error(protocol_error_code code, std::string message = {}) {
        return core::error::from(code, std::move(message));
    }

    /// <summary>
    /// The protocol code of any core::error: editor errors map to their own code; scene GUID_NOT_FOUND and
    /// ecs ENTITY_NOT_ALIVE to ENTITY_NOT_FOUND; ecs COMPONENT_NOT_PRESENT to COMPONENT_NOT_FOUND;
    /// UNKNOWN_COMPONENT to TYPE_NOT_FOUND; assets NOT_FOUND to ASSET_NOT_FOUND; core IO_FAILURE and
    /// assets IO_ERROR to IO_ERROR; core INVALID_ARGUMENT, OUT_OF_RANGE, PARSE_FAILURE and ecs
    /// SERIALIZATION_FAILED to INVALID_ARGS; core NOT_FOUND and engine PROJECT_NOT_FOUND (a missing file or
    /// folder, code sweep F656) to PATH_NOT_FOUND; core UNSUPPORTED and NOT_IMPLEMENTED to NOT_AVAILABLE; core
    /// TIMEOUT to TIMEOUT; everything else to INTERNAL.
    /// </summary>
    [[nodiscard]] CONJURE_API protocol_error_code classify_error(const core::error &failure) noexcept;
}
