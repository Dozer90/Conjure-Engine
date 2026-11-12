#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/log.h"
#include "conjure/editor/console_buffer.h"
#include "conjure/editor/log_category.h"

#include <algorithm>
#include <atomic>
#include <format>
#include <string>
#include <string_view>
#include <vector>

// console_buffer.h: the fixed ring the console panel and the log event read from.

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/console_buffer") {

    TEST_CASE("console_buffer - push assigns increasing sequence numbers and read returns them in order") {
        console_buffer buffer(16, 1024);
        buffer.set_frame(7);
        buffer.push(core::log_level::INFO, "editor", "one");
        buffer.push(core::log_level::WARN, "scene", "two");
        CHECK(buffer.size() == 2);
        CHECK(buffer.last_seq() == 2);
        const std::vector<console_record> records = buffer.read({});
        REQUIRE(records.size() == 2);
        CHECK(records[0].seq == 1);
        CHECK(records[0].frame == 7);
        CHECK(records[0].category == "editor");
        CHECK(records[0].text == "one");
        CHECK(records[1].level == core::log_level::WARN);
        CHECK(records[1].category == "scene");
    }

    TEST_CASE("console_buffer - the entry ring wraps and keeps the newest entries") {
        console_buffer buffer(4, 4096);
        for (int index = 0; index < 10; ++index) {
            buffer.push(core::log_level::INFO, "t", std::to_string(index));
        }
        CHECK(buffer.size() == 4);
        const std::vector<console_record> records = buffer.read({});
        REQUIRE(records.size() == 4);
        CHECK(records.front().text == "6");
        CHECK(records.back().text == "9");
        CHECK(buffer.entry_capacity() == 4);
    }

    TEST_CASE("console_buffer - the text ring wraps and drops entries whose text was overwritten") {
        console_buffer buffer(64, 64);
        // The ring may round its capacity up; size the chunks so six of them cannot fit whatever it chose.
        const std::string chunk(buffer.text_capacity() / 4 + 1, 'x');
        for (int index = 0; index < 6; ++index) {
            buffer.push(core::log_level::INFO, "c", chunk);
        }
        const std::vector<console_record> records = buffer.read({});
        CHECK(records.size() < 6);
        CHECK_FALSE(records.empty());
        for (const console_record &record : records) {
            CHECK(record.text == chunk);
            CHECK(record.category == "c");
        }
        CHECK(buffer.text_capacity() >= 64);
    }

    TEST_CASE("regression - F095 console_buffer laps of mixed-size records never return text that is not theirs") {
        // Uneven records leave the previous lap's leftovers at the end of the text ring when it wraps; a second
        // wrap must drop them before the records the new lap overwrites at the start. Every seventh record is
        // empty, which must neither stop the eviction nor lose its own place.
        console_buffer buffer(4096, 4096);
        constexpr usize PUSHES = 5000;
        const auto expectedText = [](usize index) {
            if (index % 7 == 0) {
                return std::string();
            }
            const char letter = static_cast<char>('a' + index % 26);
            return std::format("{}:", index) + std::string(10 + index * 37 % 190, letter);
        };
        usize largest = 0;
        for (usize index = 0; index < PUSHES; ++index) {
            const std::string text = expectedText(index);
            const std::string_view category = index % 7 == 0 ? "" : "c";
            largest = std::max<usize>(largest, std::max<usize>(category.size() + text.size(), 1));
            buffer.push(core::log_level::INFO, category, text);
        }
        console_query everything = {};
        everything.limit = PUSHES;
        const std::vector<console_record> records = buffer.read(everything);
        REQUIRE_FALSE(records.empty());
        CHECK(records.back().seq == PUSHES);
        usize held = 0;
        for (usize position = 0; position < records.size(); ++position) {
            const console_record &record = records[position];
            CAPTURE(record.seq);
            const usize index = static_cast<usize>(record.seq - 1);
            CHECK(record.text == expectedText(index));
            CHECK(record.category == (index % 7 == 0 ? "" : "c"));
            if (position > 0) {
                CHECK(record.seq == records[position - 1].seq + 1);
            }
            held += std::max<usize>(record.category.size() + record.text.size(), 1);
        }
        // Nor may it drop more than it overwrote: the current lap plus the previous lap's survivors span the ring
        // but for the record that did not fit at the end of that lap and the one cut at the write position.
        CHECK(held > buffer.text_capacity() - 2 * largest);
    }

    TEST_CASE("console_buffer - read with sinceSeq after the ring wrapped returns exactly the newer entries") {
        console_buffer buffer(4, 4096);
        for (int index = 0; index < 10; ++index) {
            buffer.push(core::log_level::INFO, "t", std::to_string(index));
        }
        const auto seqsSince = [&buffer](u64 sinceSeq) {
            console_query query = {};
            query.sinceSeq = sinceSeq;
            std::vector<u64> seqs = {};
            for (const console_record &record : buffer.read(query)) {
                seqs.push_back(record.seq);
            }
            return seqs;
        };
        CHECK(seqsSince(7) == std::vector<u64>{8, 9, 10});
        CHECK(seqsSince(2) == std::vector<u64>{7, 8, 9, 10});
        CHECK(seqsSince(0) == std::vector<u64>{7, 8, 9, 10});
        CHECK(seqsSince(10).empty());
        CHECK(seqsSince(100).empty());
        buffer.clear();
        CHECK(seqsSince(0).empty());
        buffer.push(core::log_level::INFO, "t", "after");
        buffer.push(core::log_level::INFO, "t", "clear");
        CHECK(seqsSince(0) == std::vector<u64>{11, 12});
        CHECK(seqsSince(11) == std::vector<u64>{12});
    }

    TEST_CASE("console_buffer - read filters by sequence, level and category, and honours the limit") {
        console_buffer buffer{};
        buffer.push(core::log_level::DEBUG, "a", "debug a");
        buffer.push(core::log_level::INFO, "b", "info b");
        buffer.push(core::log_level::ERR, "a", "error a");
        buffer.push(core::log_level::WARN, "b", "warn b");
        console_query since = {};
        since.sinceSeq = 2;
        CHECK(buffer.read(since).size() == 2);
        console_query level = {};
        level.minLevel = core::log_level::WARN;
        const std::vector<console_record> warnings = buffer.read(level);
        REQUIRE(warnings.size() == 2);
        CHECK(warnings[0].text == "error a");
        console_query category = {};
        category.category = "a";
        CHECK(buffer.read(category).size() == 2);
        console_query limit = {};
        limit.limit = 1;
        REQUIRE(buffer.read(limit).size() == 1);
        CHECK(buffer.read(limit)[0].text == "debug a");
    }

    TEST_CASE("regression - F110 console_buffer a newest read keeps the latest matching entries oldest first") {
        console_buffer buffer{};
        for (int index = 1; index <= 10; ++index) {
            buffer.push(index % 2 == 0 ? core::log_level::WARN : core::log_level::INFO, "t", std::to_string(index));
        }
        const auto seqsOf = [&buffer](const console_query &query) {
            std::vector<u64> seqs = {};
            for (const console_record &record : buffer.read(query)) {
                seqs.push_back(record.seq);
            }
            return seqs;
        };
        console_query oldest = {};
        oldest.limit = 3;
        CHECK(seqsOf(oldest) == std::vector<u64>{1, 2, 3});
        console_query newest = oldest;
        newest.newest = true;
        CHECK(seqsOf(newest) == std::vector<u64>{8, 9, 10});
        // The filters and sinceSeq still apply: the newest three warnings, then only those after seq 7.
        newest.minLevel = core::log_level::WARN;
        CHECK(seqsOf(newest) == std::vector<u64>{6, 8, 10});
        newest.sinceSeq = 7;
        CHECK(seqsOf(newest) == std::vector<u64>{8, 10});
        newest.sinceSeq = 10;
        CHECK(seqsOf(newest).empty());
        // A limit above the matches returns every match.
        console_query all = {};
        all.newest = true;
        CHECK(seqsOf(all).size() == 10);
        CHECK(buffer.read(all).back().text == "10");
    }

    TEST_CASE("console_buffer - clear empties the ring and keeps sequence numbers increasing") {
        console_buffer buffer{};
        buffer.push(core::log_level::INFO, "x", "1");
        buffer.clear();
        CHECK(buffer.size() == 0);
        buffer.push(core::log_level::INFO, "x", "2");
        CHECK(buffer.read({}).front().seq == 2);
    }

    TEST_CASE("console_buffer - the installed sink captures log messages from any thread") {
        testing::log_capture quiet = {};
        console_buffer buffer{};
        buffer.install_sink();
        buffer.install_sink();
        CONJURE_LOG_INFO(LOG_EDITOR, "from the main thread {}", 1);
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = 2;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        std::atomic<int> done = 0;
        jobs.parallel_for(32, 1, [&done](usize begin, usize end) {
            for (usize index = begin; index < end; ++index) {
                CONJURE_LOG_INFO(LOG_EDITOR, "from a worker {}", index);
                ++done;
            }
        });
        CHECK(done.load() == 32);
        jobs.stop();
        console_query query = {};
        query.category = "editor";
        const std::vector<console_record> records = buffer.read(query);
        CHECK(records.size() == 33);
        buffer.remove_sink();
        CONJURE_LOG_INFO(LOG_EDITOR, "after removal");
        CHECK(buffer.read(query).size() == 33);
    }
}
