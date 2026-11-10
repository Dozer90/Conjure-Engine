#pragma once

#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"

#include <nethost.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>

#include <filesystem>
#include <string>
#include <string_view>

// Private: the .NET hosting headers and the char_t conversions (docs/design/03_scripting.md 3.1 step 2).
// char_t is wchar_t on Windows and char elsewhere; std::filesystem::path::string_type matches it on both,
// so paths cross with path::native() and only ASCII literals and log messages are converted by hand.
// Nothing outside engine/src/scripting includes this header (the hosting headers never leak, R30).

namespace conjure::scripting::hostfxr {

    using host_string = std::filesystem::path::string_type;

    static_assert(sizeof(typename host_string::value_type) == sizeof(char_t), "path::string_type must match char_t");

    /// <summary>
    /// Hosting status codes (dotnet/runtime src/native/corehost/error_codes.h; not shipped in the pack).
    /// </summary>
    enum class host_status : u32 {
        SUCCESS = 0,
        SUCCESS_HOST_ALREADY_INITIALIZED = 0x00000001,
        SUCCESS_DIFFERENT_RUNTIME_PROPERTIES = 0x00000002,
        INVALID_ARG_FAILURE = 0x80008081,
        CORE_HOST_LIB_LOAD_FAILURE = 0x80008082,
        CORE_HOST_LIB_MISSING_FAILURE = 0x80008083,
        CORE_HOST_ENTRY_POINT_FAILURE = 0x80008084,
        CORE_CLR_INIT_FAILURE = 0x80008089,
        INVALID_CONFIG_FILE = 0x80008093,
        FRAMEWORK_MISSING_FAILURE = 0x80008096,
        HOST_API_FAILED = 0x80008097,
        HOST_API_BUFFER_TOO_SMALL = 0x80008098,
        HOST_API_UNSUPPORTED_VERSION = 0x800080A2,
        HOST_INVALID_STATE = 0x800080A3,
        HOST_PROPERTY_NOT_FOUND = 0x800080A4,
        HOST_API_UNSUPPORTED_SCENARIO = 0x800080A6
    };

    [[nodiscard]] constexpr bool is_success(int32_t code) noexcept {
        return code >= 0;
    }

    /// <summary>
    /// A path as the hosting API wants it.
    /// </summary>
    [[nodiscard]] inline host_string to_host_string(const std::filesystem::path &path) {
        return path.native();
    }

    /// <summary>
    /// An ASCII literal (type names, property names) as a host string.
    /// </summary>
    [[nodiscard]] inline host_string host_text(std::string_view ascii) {
        host_string text = {};
        text.reserve(ascii.size());
        for (const char character : ascii) {
            text.push_back(static_cast<typename host_string::value_type>(static_cast<unsigned char>(character)));
        }
        return text;
    }

    /// <summary>
    /// A host string (UTF-16 on Windows, UTF-8 elsewhere) as UTF-8 for log messages.
    /// </summary>
    [[nodiscard]] inline std::string narrow(const char_t *text) {
        if (text == nullptr) {
            return {};
        }
        if constexpr (sizeof(char_t) == 1) {
            return std::string(reinterpret_cast<const char *>(text));
        } else {
            std::string utf8 = {};
            const auto *units = reinterpret_cast<const char16_t *>(text);
            for (usize index = 0; units[index] != 0; ++index) {
                u32 codepoint = units[index];
                const bool highSurrogate = codepoint >= 0xD800 && codepoint <= 0xDBFF;
                if (highSurrogate && units[index + 1] >= 0xDC00 && units[index + 1] <= 0xDFFF) {
                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (static_cast<u32>(units[index + 1]) - 0xDC00);
                    ++index;
                }
                if (codepoint < 0x80) {
                    utf8.push_back(static_cast<char>(codepoint));
                } else if (codepoint < 0x800) {
                    utf8.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                    utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                } else if (codepoint < 0x10000) {
                    utf8.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                    utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                    utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                } else {
                    utf8.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
                    utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                    utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                    utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                }
            }
            return utf8;
        }
    }
}
