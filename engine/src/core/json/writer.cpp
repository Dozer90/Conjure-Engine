#include "conjure/core/json/writer.h"

#include "core/json/json_internal.h"

#include "conjure/core/assert.h"
#include "conjure/core/platform_detect.h"

#include <array>
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::json;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

writer::writer() {}

writer::~writer() {}


/*===========================================================================*/
/*===========================================================================*/
// Structure

void writer::begin_object() {
    if (!begin_value()) {
        return;
    }
    mBuffer.push_back('{');
    mStack.push_back(frame{scope::OBJECT, false, true});
}

void writer::end_object() {
    if (mStack.empty() || mStack.back().kind != scope::OBJECT) {
        CONJURE_ASSERT(false, "json::writer: end_object() without a matching begin_object()");
        return;
    }
    if (!mStack.back().expectKey) {
        // A key without a value would leave `{"k":}`; null keeps the text well formed.
        CONJURE_ASSERT(false, "json::writer: end_object() while a key is waiting for its value");
        mBuffer.append("null");
    }
    mStack.pop_back();
    mBuffer.push_back('}');
}

void writer::begin_array() {
    if (!begin_value()) {
        return;
    }
    mBuffer.push_back('[');
    mStack.push_back(frame{scope::ARRAY, false, false});
}

void writer::end_array() {
    if (mStack.empty() || mStack.back().kind != scope::ARRAY) {
        CONJURE_ASSERT(false, "json::writer: end_array() without a matching begin_array()");
        return;
    }
    mStack.pop_back();
    mBuffer.push_back(']');
}

void writer::key(std::string_view name) {
    if (mStack.empty() || mStack.back().kind != scope::OBJECT) {
        CONJURE_ASSERT(false, "json::writer: key() is only valid inside an object");
        return;
    }

    frame &top = mStack.back();
    if (!top.expectKey) {
        CONJURE_ASSERT(false, "json::writer: key('{}') follows another key without a value", name);
        return;
    }

    if (top.hasItems) {
        mBuffer.push_back(',');
    }
    top.hasItems = true;
    top.expectKey = false;

    append_string(name);
    mBuffer.push_back(':');
}


/*===========================================================================*/
/*===========================================================================*/
// Scalars

void writer::value(bool item) {
    if (!begin_value()) {
        return;
    }
    mBuffer.append(item ? "true" : "false");
}

void writer::value(i32 item) {
    value(static_cast<i64>(item));
}

void writer::value(u32 item) {
    value(static_cast<u64>(item));
}

void writer::value(i64 item) {
    if (!begin_value()) {
        return;
    }
    std::array<char, 24> digits = {};
    const std::to_chars_result converted = std::to_chars(digits.data(), digits.data() + digits.size(), item);
    mBuffer.append(digits.data(), static_cast<usize>(converted.ptr - digits.data()));
}

void writer::value(u64 item) {
    if (!begin_value()) {
        return;
    }
    std::array<char, 24> digits = {};
    const std::to_chars_result converted = std::to_chars(digits.data(), digits.data() + digits.size(), item);
    mBuffer.append(digits.data(), static_cast<usize>(converted.ptr - digits.data()));
}

void writer::value(f32 item) {
    value(static_cast<f64>(item));
}

void writer::value(f64 item) {
    if (!begin_value()) {
        return;
    }
    if (!std::isfinite(item)) {
        CONJURE_ASSERT(false, "json::writer: {} is not representable in JSON; writing null", item);
        mBuffer.append("null");
        return;
    }
    std::array<char, 32> digits = {};
    const usize length = detail::format_real(item, digits.data());
    mBuffer.append(digits.data(), length);
}

void writer::value(std::string_view item) {
    if (!begin_value()) {
        return;
    }
    append_string(item);
}

void writer::value(const char *item) {
    if (item == nullptr) {
        value(nullptr);
        return;
    }
    value(std::string_view(item));
}

void writer::value(std::nullptr_t) {
    if (!begin_value()) {
        return;
    }
    mBuffer.append("null");
}

void writer::write_raw_json(std::string_view rawJson) {
#if CONJURE_DEBUG
    yyjson_doc *parsed = yyjson_read(rawJson.data(), rawJson.size(), YYJSON_READ_NOFLAG);
    CONJURE_ASSERT(parsed != nullptr, "json::writer: write_raw_json() received invalid JSON: '{}'", rawJson);
    yyjson_doc_free(parsed);
#endif
    if (!begin_value()) {
        return;
    }
    mBuffer.append(rawJson);
}


/*===========================================================================*/
/*===========================================================================*/
// Completion

std::string writer::finish() {
    CONJURE_ASSERT(mStack.empty(), "json::writer: finish() with {} container(s) still open", mStack.size());
    CONJURE_ASSERT(mRootWritten, "json::writer: finish() before any value was written");

    std::string output = std::move(mBuffer);
    reset();
    return output;
}

void writer::reset() {
    mBuffer.clear();
    mStack.clear();
    mRootWritten = false;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

bool writer::begin_value() {
    if (mStack.empty()) {
        if (mRootWritten) {
            CONJURE_ASSERT(false, "json::writer: a document has exactly one root value");
            return false;
        }
        mRootWritten = true;
        return true;
    }

    frame &top = mStack.back();
    if (top.kind == scope::OBJECT) {
        if (top.expectKey) {
            CONJURE_ASSERT(false, "json::writer: a value inside an object must follow key()");
            return false;
        }
        top.expectKey = true;
        return true;
    }

    if (top.hasItems) {
        mBuffer.push_back(',');
    }
    top.hasItems = true;
    return true;
}

void writer::append_string(std::string_view text) {
    // Mirrors yyjson's default escaping so writer output matches document::to_canonical_string():
    // quote, backslash and the C0 controls are escaped (short forms where JSON has them, upper-case
    // \u00XX otherwise); '/', DEL and every byte above 0x7F pass through untouched. The bytes are
    // therefore trusted to be UTF-8; document::to_string() would refuse what is written here.
#if CONJURE_DEBUG
    CONJURE_ASSERT(detail::is_valid_utf8(text), "json::writer: string holds invalid UTF-8; writing it as-is");
#endif
    constexpr char HEX_DIGITS[] = "0123456789ABCDEF";

    mBuffer.push_back('"');
    if (text.empty()) {
        // A default-constructed view has no data pointer to append from.
        mBuffer.push_back('"');
        return;
    }

    usize runStart = 0;
    for (usize index = 0; index < text.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(text[index]);
        if (character >= 0x20 && character != '"' && character != '\\') {
            continue;
        }

        mBuffer.append(text.data() + runStart, index - runStart);
        runStart = index + 1;

        switch (character) {
            case '"': {
                mBuffer.append("\\\"");
                break;
            }
            case '\\': {
                mBuffer.append("\\\\");
                break;
            }
            case '\b': {
                mBuffer.append("\\b");
                break;
            }
            case '\t': {
                mBuffer.append("\\t");
                break;
            }
            case '\n': {
                mBuffer.append("\\n");
                break;
            }
            case '\f': {
                mBuffer.append("\\f");
                break;
            }
            case '\r': {
                mBuffer.append("\\r");
                break;
            }
            default: {
                mBuffer.append("\\u00");
                mBuffer.push_back(HEX_DIGITS[character >> 4]);
                mBuffer.push_back(HEX_DIGITS[character & 0x0F]);
                break;
            }
        }
    }

    mBuffer.append(text.data() + runStart, text.size() - runStart);
    mBuffer.push_back('"');
}
