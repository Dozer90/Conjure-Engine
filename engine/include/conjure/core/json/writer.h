#pragma once

#include "conjure/core/types.h"

#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Streaming JSON writer.
//
//   json::writer out = {};
//   out.begin_object();
//   out.key("name");  out.value("player");
//   out.key("tags");  out.begin_array();  out.value(1);  out.value(2);  out.end_array();
//   out.end_object();
//   std::string text = out.finish();        // {"name":"player","tags":[1,2]}
//
// The output is minified and byte-identical to document::to_canonical_string() for the same data
// when keys are written in sorted order: same escaping, same shortest round-trip reals, integers
// as plain digits. Structural misuse (a key outside an object, a value where a key is expected,
// an unbalanced end, a second root) asserts and is ignored so the buffer stays well formed.
//
// Strings and keys must be valid UTF-8: the writer copies bytes as they are (escaping only what
// JSON requires), so invalid bytes produce text that does not parse. Debug builds assert on them.

namespace conjure::core::json {

    /// <summary>
    /// Builds minified JSON text into a std::string without an intermediate document.
    /// </summary>
    class writer {
        private:
            enum class scope : u8 {
                OBJECT,
                ARRAY
            };

            struct frame {
                scope kind = scope::OBJECT;
                bool hasItems = false;      // a comma is needed before the next item
                bool expectKey = true;      // objects only: the next call must be key()
            };

            std::string mBuffer = {};
            std::vector<frame> mStack = {};
            bool mRootWritten = false;

        public:
            writer();
            ~writer();

            writer(const writer &) = default;
            writer &operator=(const writer &) = default;
            writer(writer &&) noexcept = default;
            writer &operator=(writer &&) noexcept = default;

        public:
            /// <summary>
            /// Opens an object as the root, an array element or the value of the pending key.
            /// </summary>
            void begin_object();

            /// <summary>
            /// Closes the innermost object. Asserts when the innermost scope is not an object or a
            /// key is pending.
            /// </summary>
            void end_object();

            /// <summary>
            /// Opens an array as the root, an array element or the value of the pending key.
            /// </summary>
            void begin_array();

            /// <summary>
            /// Closes the innermost array. Asserts when the innermost scope is not an array.
            /// </summary>
            void end_array();

            /// <summary>
            /// Writes a member key. Asserts outside an object or when a key is already pending.
            /// </summary>
            void key(std::string_view name);

            /// <summary>
            /// Writes a scalar as the root, an array element or the value of the pending key.
            /// Non-finite reals assert and are written as null; a null C string writes null, as in
            /// the document API. Strings must be valid UTF-8 (asserted in Debug, written as-is
            /// otherwise). Integral types without an overload of their own widen to i64 or u64.
            /// </summary>
            void value(bool item);
            void value(i32 item);
            void value(u32 item);
            void value(i64 item);
            void value(u64 item);
            void value(f32 item);
            void value(f64 item);
            void value(std::string_view item);
            void value(const char *item);
            void value(std::nullptr_t);

            template <std::integral T>
                requires (!std::same_as<T, bool> && !std::same_as<T, i32> && !std::same_as<T, u32> &&
                  !std::same_as<T, i64> && !std::same_as<T, u64>)
            void value(T item) {
                value(static_cast<std::conditional_t<std::is_signed_v<T>, i64, u64>>(item));
            }

            /// <summary>
            /// Inserts pre-serialised JSON verbatim where a value is expected. Debug builds parse
            /// the text and assert when it is not a single valid JSON value.
            /// </summary>
            void write_raw_json(std::string_view rawJson);

            /// <summary>
            /// Returns the text and resets the writer. Asserts when containers are still open or no
            /// root was written (the partial text is still returned).
            /// </summary>
            [[nodiscard]] std::string finish();

            /// <summary>
            /// Discards everything written so far.
            /// </summary>
            void reset();

            /// <summary>
            /// Number of open containers.
            /// </summary>
            [[nodiscard]] usize depth() const noexcept {
                return mStack.size();
            }

            /// <summary>
            /// True when exactly one root value has been written and every container is closed.
            /// </summary>
            [[nodiscard]] bool is_complete() const noexcept {
                return mRootWritten && mStack.empty();
            }

            /// <summary>
            /// The text written so far (possibly incomplete).
            /// </summary>
            [[nodiscard]] std::string_view text() const noexcept {
                return mBuffer;
            }

        private:
            [[nodiscard]] bool begin_value();
            void append_string(std::string_view text);
    };
}
