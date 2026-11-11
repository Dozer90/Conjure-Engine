#include "conjure/testing/doctest.h"

#include "conjure/editor/protocol/line_splitter.h"

#include <string>

// line_splitter.h: newline framing with a size limit.

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/protocol/line_splitter") {

    TEST_CASE("line_splitter - a line split across feeds is reassembled") {
        line_splitter splitter{};
        std::string line = {};
        splitter.feed("{\"id\":1,");
        CHECK_FALSE(splitter.next_line(line));
        CHECK(splitter.pending_bytes() == 8);
        splitter.feed("\"cmd\":\"x\"}\n");
        REQUIRE(splitter.next_line(line));
        CHECK(line == "{\"id\":1,\"cmd\":\"x\"}");
        CHECK_FALSE(splitter.next_line(line));
        CHECK(splitter.pending_bytes() == 0);
    }

    TEST_CASE("line_splitter - many lines in one feed come out in order") {
        line_splitter splitter{};
        splitter.feed("a\nb\nc\n");
        std::string line = {};
        REQUIRE(splitter.next_line(line));
        CHECK(line == "a");
        REQUIRE(splitter.next_line(line));
        CHECK(line == "b");
        REQUIRE(splitter.next_line(line));
        CHECK(line == "c");
        CHECK_FALSE(splitter.next_line(line));
    }

    TEST_CASE("line_splitter - CRLF line endings are tolerated and empty lines are delivered") {
        line_splitter splitter{};
        splitter.feed("one\r\n\r\ntwo\n");
        std::string line = {};
        REQUIRE(splitter.next_line(line));
        CHECK(line == "one");
        REQUIRE(splitter.next_line(line));
        CHECK(line.empty());
        REQUIRE(splitter.next_line(line));
        CHECK(line == "two");
    }

    TEST_CASE("line_splitter - an oversized line sets the overflow flag and is discarded whole") {
        line_splitter splitter(8);
        CHECK(splitter.limit() == 8);
        splitter.feed("0123456789");
        CHECK(splitter.overflowed());
        splitter.feed("more\nnext\n");
        std::string line = {};
        REQUIRE(splitter.next_line(line));
        CHECK(line == "next");
        splitter.reset();
        CHECK_FALSE(splitter.overflowed());
        CHECK(splitter.pending_bytes() == 0);
    }

    TEST_CASE("regression - the limit applies per line, not to the lines framed and not yet taken") {
        // Two 20-byte lines in one feed total more than a limit of 32: both are ordinary lines.
        line_splitter splitter(32);
        splitter.feed(std::string(20, 'a') + "\n" + std::string(20, 'b') + "\n");
        CHECK_FALSE(splitter.overflowed());
        CHECK(splitter.frame_count() == 2);
        std::string line = {};
        REQUIRE(splitter.next_line(line));
        CHECK(line == std::string(20, 'a'));
        REQUIRE(splitter.next_line(line));
        CHECK(line == std::string(20, 'b'));
        CHECK_FALSE(splitter.next_line(line));
        // Many short lines in one feed, far beyond the limit in total, all arrive.
        std::string many = {};
        for (int index = 0; index < 50; ++index) {
            many += "line\n";
        }
        splitter.feed(many);
        CHECK_FALSE(splitter.overflowed());
        usize count = 0;
        while (splitter.next_line(line)) {
            ++count;
        }
        CHECK(count == 50);
        // A single line over the limit still overflows, whichever feed completes it.
        splitter.feed(std::string(30, 'c'));
        CHECK_FALSE(splitter.overflowed());
        splitter.feed(std::string(3, 'c') + "\n");
        CHECK(splitter.overflowed());
    }

    TEST_CASE("line_splitter - next_frame reports the oversized marker at its position in the stream") {
        line_splitter splitter(8);
        splitter.feed("ok\n" + std::string(20, 'x') + "\nafter\n");
        std::string line = {};
        CHECK(splitter.next_frame(line) == line_splitter::frame::LINE);
        CHECK(line == "ok");
        CHECK(splitter.next_frame(line) == line_splitter::frame::OVERSIZED);
        CHECK(line.empty());
        CHECK(splitter.next_frame(line) == line_splitter::frame::LINE);
        CHECK(line == "after");
        CHECK(splitter.next_frame(line) == line_splitter::frame::NONE);
        // A marker for an oversized line that has no newline yet precedes the lines that follow it later.
        splitter.feed(std::string(9, 'y'));
        CHECK(splitter.frame_count() == 1);
        splitter.feed("tail\nz\n");
        CHECK(splitter.next_frame(line) == line_splitter::frame::OVERSIZED);
        CHECK(splitter.next_frame(line) == line_splitter::frame::LINE);
        CHECK(line == "z");
        splitter.reset();
        CHECK(splitter.frame_count() == 0);
        CHECK_FALSE(splitter.overflowed());
    }

    TEST_CASE("line_splitter - the default limit is the protocol maximum") {
        const line_splitter splitter{};
        CHECK(splitter.limit() == MAX_MESSAGE_BYTES);
        CHECK(MAX_MESSAGE_BYTES == 16u * 1024u * 1024u);
    }

    TEST_CASE("line_splitter - bytes feed the same way as text") {
        line_splitter splitter{};
        const std::string text = "bytes\n";
        splitter.feed(std::span<const std::byte>(reinterpret_cast<const std::byte *>(text.data()), text.size()));
        std::string line = {};
        REQUIRE(splitter.next_line(line));
        CHECK(line == "bytes");
    }
}
