#pragma once

#include "conjure/editor/protocol/protocol_message.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <deque>
#include <span>
#include <string>
#include <string_view>

// Newline framing (docs/design/04_editor.md D7, 6): bytes arrive in arbitrary chunks; complete lines come out
// one at a time with a trailing '\r' removed. A single line that grows past the limit without a newline is
// replaced by an OVERSIZED marker at its position in the stream and sets the overflow flag: the transport
// answers MESSAGE_TOO_LARGE and disconnects the client; the rest of the oversized line is discarded until the
// next newline. The limit applies to one line only, never to the complete lines that were framed but not yet
// taken.

namespace conjure::editor {

    class CONJURE_API line_splitter {
        public:
            /// <summary>
            /// What next_frame() hands out: a complete line, the marker of a line that exceeded the limit, or
            /// nothing yet.
            /// </summary>
            enum class frame : u8 {
                NONE = 0,
                LINE,
                OVERSIZED
            };

        private:
            struct framed_line {
                std::string text = "";
                bool oversized = false;
            };

            std::deque<framed_line> mFrames = {};           // complete lines and markers, in arrival order
            std::string mCurrent = "";                      // the incomplete line being assembled
            usize mLimit = MAX_MESSAGE_BYTES;
            bool mOverflowed = false;
            bool mDiscarding = false;                       // skipping the rest of an oversized line

        public:
            explicit line_splitter(usize limit = MAX_MESSAGE_BYTES);
            ~line_splitter();

            line_splitter(const line_splitter &) = default;
            line_splitter &operator=(const line_splitter &) = default;
            line_splitter(line_splitter &&) noexcept = default;
            line_splitter &operator=(line_splitter &&) noexcept = default;

        public:
            /// <summary>
            /// Appends received bytes.
            /// </summary>
            void feed(std::span<const std::byte> bytes);
            void feed(std::string_view text);

            /// <summary>
            /// Takes the next frame in arrival order: LINE fills 'out' with the text (terminator removed),
            /// OVERSIZED marks where a line exceeded the limit ('out' is cleared), NONE when nothing is
            /// complete yet.
            /// </summary>
            [[nodiscard]] frame next_frame(std::string &out);

            /// <summary>
            /// Takes the next complete line (without its terminator), skipping OVERSIZED markers. False when
            /// none is complete.
            /// </summary>
            [[nodiscard]] bool next_line(std::string &out);

            /// <summary>
            /// True once a line exceeded the limit. Stays set until reset().
            /// </summary>
            [[nodiscard]] bool overflowed() const noexcept {
                return mOverflowed;
            }

            /// <summary>
            /// Bytes of the incomplete line held back.
            /// </summary>
            [[nodiscard]] usize pending_bytes() const noexcept {
                return mCurrent.size();
            }

            /// <summary>
            /// Complete lines and markers waiting to be taken.
            /// </summary>
            [[nodiscard]] usize frame_count() const noexcept {
                return mFrames.size();
            }

            [[nodiscard]] usize limit() const noexcept {
                return mLimit;
            }

            /// <summary>
            /// Forgets the buffered bytes, the waiting frames and the overflow flag.
            /// </summary>
            void reset();
    };
}
