#include "conjure/editor/protocol/line_splitter.h"

#include <utility>

namespace conjure::editor {

    line_splitter::line_splitter(usize limit) :
        mLimit(limit == 0 ? MAX_MESSAGE_BYTES : limit) {}

    line_splitter::~line_splitter() = default;

    void line_splitter::feed(std::span<const std::byte> bytes) {
        feed(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
    }

    void line_splitter::feed(std::string_view text) {
        usize offset = 0;
        while (offset < text.size()) {
            if (mDiscarding) {
                // The rest of an oversized line is dropped up to and including its newline.
                const usize newline = text.find('\n', offset);
                if (newline == std::string_view::npos) {
                    return;
                }
                mDiscarding = false;
                offset = newline + 1;
                continue;
            }
            const usize newline = text.find('\n', offset);
            const usize end = newline == std::string_view::npos ? text.size() : newline;
            const usize chunk = end - offset;
            // Only the line under assembly counts against the limit: the frames already complete are the
            // server's to take, however many of them one feed delivered.
            if (mCurrent.size() + chunk > mLimit) {
                mOverflowed = true;
                mCurrent.clear();
                mFrames.push_back(framed_line{"", true});
                if (newline == std::string_view::npos) {
                    mDiscarding = true;
                    return;
                }
                offset = newline + 1;
                continue;
            }
            mCurrent.append(text.substr(offset, chunk));
            if (newline == std::string_view::npos) {
                return;
            }
            if (!mCurrent.empty() && mCurrent.back() == '\r') {
                mCurrent.pop_back();
            }
            mFrames.push_back(framed_line{std::move(mCurrent), false});
            mCurrent = {};
            offset = newline + 1;
        }
    }

    line_splitter::frame line_splitter::next_frame(std::string &out) {
        if (mFrames.empty()) {
            return frame::NONE;
        }
        framed_line next = std::move(mFrames.front());
        mFrames.pop_front();
        if (next.oversized) {
            out.clear();
            return frame::OVERSIZED;
        }
        out = std::move(next.text);
        return frame::LINE;
    }

    bool line_splitter::next_line(std::string &out) {
        for (frame kind = next_frame(out); kind != frame::NONE; kind = next_frame(out)) {
            if (kind == frame::LINE) {
                return true;
            }
        }
        return false;
    }

    void line_splitter::reset() {
        mFrames.clear();
        mCurrent.clear();
        mOverflowed = false;
        mDiscarding = false;
    }
}
