#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <string>
#include <string_view>

// The secrets of the AI control protocol (docs/design/04_editor.md 3.12, D23, D25): what a token grants, how
// one is generated, where it is written for clients, and the comparisons that do not leak it: every place
// that holds a token against text a client sent uses one of the constant_time_ functions below. The protocol
// server owns the tokens; nothing here logs or stores one.

namespace conjure::editor {

    /// <summary>
    /// What a connected client may do (decision D25 of 04_editor.md). The levels are ordered: a higher one
    /// includes everything below it, so callers compare with &lt; and &gt;=. Which level a command asks for is
    /// required_access() in conjure/editor/editor_command.h.
    /// </summary>
    enum class access_level : u8 {
        NONE = 0,           // nothing: no token the server knows was presented
        INSPECT = 1,        // the commands flagged READ_ONLY: looking at the editor, changing nothing
        FULL_CONTROL = 2    // every command
    };

    /// <summary>
    /// The wire spelling of a level, as session.hello and the audit log write it: "none", "inspect",
    /// "full_control".
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(access_level level) noexcept;

    /// <summary>
    /// Random bytes in a generated token.
    /// </summary>
    inline constexpr usize ACCESS_TOKEN_BYTES = 32;

    /// <summary>
    /// Characters of a generated token: two lower-case hexadecimal digits per byte.
    /// </summary>
    inline constexpr usize ACCESS_TOKEN_LENGTH = ACCESS_TOKEN_BYTES * 2;

    /// <summary>
    /// A new token: ACCESS_TOKEN_BYTES bytes from std::random_device("/dev/urandom"), written as
    /// ACCESS_TOKEN_LENGTH lower-case hexadecimal digits. Where the bytes come from depends on the standard
    /// library, and the device is named so that each of the three supported ones reads the operating system's
    /// generator: libstdc++ reads the device /dev/urandom (asked for nothing, it would take the processor's
    /// RDSEED or RDRAND instruction on x86 before the kernel); libc++ reads that device, or calls getentropy
    /// or arc4random where it was built for those, and accepts this name in all three builds; the Microsoft
    /// library ignores the name and calls rand_s, which is the system generator. The C++ standard promises
    /// none of this, so a port to another library must check it. A host whose generator cannot be opened or
    /// read makes std::random_device throw, which engine code never catches: the process ends. Thread-safe.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string generate_access_token();

    /// <summary>
    /// True when the two texts are equal. Every byte of 'presented' is compared whatever the outcome so
    /// far, so the time taken does not tell a client how long a prefix of its guess was right. The length
    /// of 'expected' is not hidden (a generated token always has ACCESS_TOKEN_LENGTH characters). Two
    /// empty texts are equal.
    /// </summary>
    [[nodiscard]] CONJURE_API bool constant_time_equals(std::string_view presented, std::string_view expected) noexcept;

    /// <summary>
    /// True when 'token' starts at byte 'offset' of 'text'. The comparison is constant_time_equals over the
    /// token's length, so the time taken tells nothing about how much of the token the text has there.
    /// False for an empty token and where the text is too short to hold it.
    /// </summary>
    [[nodiscard]] CONJURE_API bool constant_time_starts_at(
      std::string_view text,
      usize offset,
      std::string_view token
    ) noexcept;

    /// <summary>
    /// True when 'text' contains 'token'. Unlike a substring search, which stops at the first byte that
    /// differs and so takes longer the more of the token a text holds, every position of the text is
    /// compared over the token's whole length (constant_time_starts_at) and none ends the loop early: the
    /// time depends on the two lengths alone. That costs text.size() * token.size() steps, so a caller
    /// bounds the text it hands over. An empty token is contained in nothing.
    /// </summary>
    [[nodiscard]] CONJURE_API bool constant_time_contains(std::string_view text, std::string_view token) noexcept;

    /// <summary>
    /// The name of the file under &lt;project&gt;/.conjure that holds the token of a level, next to ai_port:
    /// "ai_token" for FULL_CONTROL, "ai_token_inspect" for INSPECT. NONE has no token and no file ("").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view token_file_name(access_level level) noexcept;
}
