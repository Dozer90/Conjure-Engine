#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/log.h"
#include "conjure/editor/console_buffer.h"
#include "conjure/editor/log_category.h"

#include <atomic>
#include <string>
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
