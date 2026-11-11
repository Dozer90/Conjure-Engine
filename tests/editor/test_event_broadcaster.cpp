#include "conjure/testing/doctest.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/event_broadcaster.h"

#include <string>
#include <vector>

// event_broadcaster.h: per-client subscriptions, log levels and monotonic sequence numbers.

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/event_broadcaster") {

    TEST_CASE("editor_event - every event has a unique name that parses back") {
        for (u32 index = 0; index < static_cast<u32>(editor_event::COUNT); ++index) {
            const editor_event event = static_cast<editor_event>(index);
            const std::string_view name = to_string(event);
            CHECK_FALSE(name.empty());
            CHECK(parse_editor_event(name) == event);
        }
        CHECK(to_string(editor_event::SCENE_CHANGED) == "scene.changed");
        CHECK_FALSE(parse_editor_event("nope").has_value());
        CHECK(event_bit(editor_event::LOG) == 1);
    }

    TEST_CASE("default_subscriptions - excludes log and command.executed") {
        const u64 mask = default_subscriptions();
        CHECK((mask & event_bit(editor_event::LOG)) == 0);
        CHECK((mask & event_bit(editor_event::COMMAND_EXECUTED)) == 0);
        CHECK((mask & event_bit(editor_event::SCENE_CHANGED)) != 0);
        CHECK((mask & event_bit(editor_event::WARNING)) != 0);
    }

    TEST_CASE("event_broadcaster - delivers to subscribed clients only") {
        event_broadcaster events = {};
        std::vector<std::string> first = {};
        std::vector<std::string> second = {};
        const u32 one = events.register_client([&first](const protocol_event &event) {
            first.push_back(event.name);
        });
        const u32 two = events.register_client([&second](const protocol_event &event) {
            second.push_back(event.name);
        });
        CHECK(one != two);
        CHECK(one != 0);
        events.set_subscriptions(two, event_bit(editor_event::PLAY_STATE));
        CHECK(events.subscriptions(two) == event_bit(editor_event::PLAY_STATE));
        (void) events.emit(editor_event::SCENE_CHANGED, core::json::document::object());
        (void) events.emit(editor_event::PLAY_STATE, core::json::document::object());
        REQUIRE(first.size() == 2);
        REQUIRE(second.size() == 1);
        CHECK(second[0] == "play.state");
        events.unregister_client(one);
        CHECK_FALSE(events.has_client(one));
        CHECK(events.client_count() == 1);
        (void) events.emit(editor_event::SCENE_CHANGED, core::json::document::object());
        CHECK(first.size() == 2);
    }

    TEST_CASE("event_broadcaster - log events honour the subscription and the minimum level") {
        event_broadcaster events = {};
        std::vector<protocol_event> received = {};
        const u32 id = events.register_client([&received](const protocol_event &event) {
            received.push_back(event);
        });
        (void) events.emit_log(core::log_level::WARN, core::json::object({{"text", "ignored: not subscribed"}}));
        CHECK(received.empty());
        events.set_subscriptions(id, default_subscriptions() | event_bit(editor_event::LOG));
        events.set_min_log_level(id, core::log_level::WARN);
        CHECK(events.min_log_level(id) == core::log_level::WARN);
        (void) events.emit_log(core::log_level::INFO, core::json::object({{"text", "below"}}));
        (void) events.emit_log(core::log_level::ERR, core::json::object({{"text", "above"}}));
        REQUIRE(received.size() == 1);
        CHECK(received[0].name == "log");
        CHECK(received[0].data.root()["text"].as_string() == "above");
    }

    TEST_CASE("event_broadcaster - sequence numbers increase across every emission and carry the frame") {
        event_broadcaster events = {};
        std::vector<u64> seqs = {};
        (void) events.register_client([&seqs](const protocol_event &event) {
            seqs.push_back(event.seq);
        });
        events.set_frame(12);
        const u64 a = events.emit(editor_event::SCENE_CHANGED, core::json::document::object());
        const u64 b = events.emit(editor_event::PROGRESS, core::json::document::object());
        CHECK(b > a);
        CHECK(events.next_seq() > b);
        CHECK(events.emitted_count() == 2);
        CHECK(events.frame() == 12);
        REQUIRE(seqs.size() == 2);
        CHECK(seqs[0] == a);
        CHECK(seqs[1] == b);
    }

    TEST_CASE("event_broadcaster - emit_to targets one client regardless of its subscriptions") {
        event_broadcaster events = {};
        int targeted = 0;
        int other = 0;
        const u32 target = events.register_client([&targeted](const protocol_event &) {
            ++targeted;
        });
        (void) events.register_client([&other](const protocol_event &) {
            ++other;
        });
        events.set_subscriptions(target, 0);
        (void) events.emit_to(target, editor_event::EVENTS_DROPPED, core::json::object({{"count", 3}}));
        CHECK(targeted == 1);
        CHECK(other == 0);
        (void) events.emit_to(999, editor_event::EVENTS_DROPPED, core::json::document::object());
        CHECK(targeted == 1);
    }

    TEST_CASE("event_broadcaster - a sink may unregister its own client while an event is delivered") {
        event_broadcaster events = {};
        u32 self = 0;
        int calls = 0;
        self = events.register_client([&events, &self, &calls](const protocol_event &) {
            ++calls;
            events.unregister_client(self);
        });
        (void) events.emit(editor_event::SCENE_CHANGED, core::json::document::object());
        (void) events.emit(editor_event::SCENE_CHANGED, core::json::document::object());
        CHECK(calls == 1);
        CHECK(events.client_count() == 0);
        const std::vector<u32> ids = events.client_ids();
        CHECK(ids.empty());
    }

    TEST_CASE("event_broadcaster - register_client with an explicit id replaces the sink") {
        event_broadcaster events = {};
        int calls = 0;
        events.register_client(5, {});
        CHECK(events.has_client(5));
        events.register_client(5, [&calls](const protocol_event &) {
            ++calls;
        });
        CHECK(events.client_count() == 1);
        (void) events.emit(editor_event::WARNING, core::json::document::object());
        CHECK(calls == 1);
    }
}
