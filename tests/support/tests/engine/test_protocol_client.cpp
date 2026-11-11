#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/protocol_client.h"

#include "conjure/core/error.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::testing;

namespace {
    /// <summary>
    /// A fake editor server: echoes the id, answers "session.ping" with pong, "entity.create" with a guid
    /// (after streaming a scene.changed event) and anything else with UNKNOWN_COMMAND; garbage is answered
    /// with a PARSE_ERROR response whose id is null, as the real server does (04_editor.md 2.3).
    /// </summary>
    void fake_server(std::string_view line, std::vector<std::string> &outLines) {
        const result<json::document> parsed = json::document::parse(line);
        if (!parsed.has_value()) {
            outLines.emplace_back(
              R"({"id":null,"ok":false,"error":{"code":"PARSE_ERROR","message":"not JSON","details":{}}})"
            );
            return;
        }

        const json::value root = parsed->root();
        const json::value id = root["id"];
        const std::string_view command = root["cmd"].as_string();
        json::document response = json::document::object();
        response.set("id", id);

        if (command == "session.ping") {
            response.set("ok", true);
            response.set("result", json::object({{"pong", true}, {"echo", root["args"]["value"]}}));
            outLines.push_back(response.to_string());
            return;
        }
        if (command == "entity.create") {
            json::document event = json::object({
                {"event", "scene.changed"},
                {"seq", 1},
                {"frame", 7},
                {"data", json::object({{"revision", 42}, {"label", "entity.create"}})},
            });
            outLines.push_back(event.to_string());
            response.set("ok", true);
            response.set("result", json::object({{"guid", "a9e2c0d4-0000-4000-8000-000000000001"}}));
            outLines.push_back(response.to_string());
            return;
        }

        response.set("ok", false);
        response.set("error", json::object({
            {"code", "UNKNOWN_COMMAND"},
            {"message", std::format("No command '{}'", command)},
            {"details", json::object({{"didYouMean", json::array({"session.ping"})}})},
        }));
        outLines.push_back(response.to_string());
    }

    [[nodiscard]] std::unique_ptr<scripted_protocol_channel> make_channel(scripted_protocol_channel::handler handler) {
        return std::make_unique<scripted_protocol_channel>(std::move(handler));
    }
}

TEST_SUITE("testing/protocol_client") {

    TEST_CASE("protocol_client - send frames the request and matches the echoed id") {
        std::unique_ptr<scripted_protocol_channel> channel = make_channel(&fake_server);
        const scripted_protocol_channel &wire = *channel;
        protocol_client client(std::move(channel));

        const result<json::document> reply = client.send("session.ping", json::object({{"value", 5}}));
        CONJURE_REQUIRE_OK(reply);
        CHECK(protocol_client::response_ok(reply.value()));
        CHECK(reply->root()["result"]["pong"].as_bool());
        CHECK(reply->root()["result"]["echo"].as_i64() == 5);
        CHECK(reply->root()["id"].as_u64() == client.last_request_id());
        CHECK(client.request_count() == 1);
        CHECK(client.events().empty());

        REQUIRE(wire.sent_lines().size() == 1);
        const result<json::document> sent = json::document::parse(wire.sent_lines()[0]);
        CONJURE_REQUIRE_OK(sent);
        CHECK(sent->root()["cmd"].as_string() == "session.ping");
        CHECK(sent->root()["args"]["value"].as_i64() == 5);
        CHECK(sent->root()["id"].as_u64() == 1);

        // Ids increase per request; the argument-less overload sends an empty object.
        const result<json::document> second = client.send("session.ping");
        CONJURE_REQUIRE_OK(second);
        CHECK(client.last_request_id() == 2);
        const result<json::document> sentSecond = json::document::parse(wire.sent_lines()[1]);
        CONJURE_REQUIRE_OK(sentSecond);
        CHECK(sentSecond->root()["args"].is_object());
        CHECK(sentSecond->root()["args"].size() == 0);
    }

    TEST_CASE("protocol_client - events between the request and the response are buffered") {
        protocol_client client(make_channel(&fake_server));

        const result<json::document> reply = client.send("entity.create", json::object({{"name", "Player"}}));
        CONJURE_REQUIRE_OK(reply);
        CHECK(protocol_client::response_ok(reply.value()));
        CHECK(reply->root()["result"]["guid"].as_string() == "a9e2c0d4-0000-4000-8000-000000000001");

        REQUIRE(client.events().size() == 1);
        CHECK(client.events()[0].root()["event"].as_string() == "scene.changed");
        CHECK(client.events()[0].root()["data"]["revision"].as_i64() == 42);

        client.clear_events();
        CHECK(client.events().empty());
    }

    TEST_CASE("protocol_client - an error response is returned as a document") {
        protocol_client client(make_channel(&fake_server));

        const result<json::document> reply = client.send("no.such.command");
        CONJURE_REQUIRE_OK(reply);
        CHECK_FALSE(protocol_client::response_ok(reply.value()));
        CHECK(protocol_client::error_code_of(reply.value()) == "UNKNOWN_COMMAND");
        CHECK(protocol_client::error_message_of(reply.value()) == "No command 'no.such.command'");
        CHECK(reply->root()["error"]["details"]["didYouMean"][0].as_string() == "session.ping");
    }

    TEST_CASE("protocol_client - send_raw garbage returns the error document") {
        protocol_client client(make_channel(&fake_server));

        const result<json::document> reply = client.send_raw("{this is not json");
        CONJURE_REQUIRE_OK(reply);
        CHECK_FALSE(protocol_client::response_ok(reply.value()));
        CHECK(protocol_client::error_code_of(reply.value()) == "PARSE_ERROR");
        CHECK(reply->root()["id"].is_null());
        CHECK(client.request_count() == 1);
        CHECK(client.last_request_id() == 0);
    }

    TEST_CASE("protocol_client - a closed channel fails with INVALID_STATE") {
        protocol_client client(make_channel(&fake_server));
        CHECK(client.is_connected());

        client.disconnect();

        CHECK_FALSE(client.is_connected());
        CONJURE_CHECK_ERROR(client.send("session.ping"), error_code::INVALID_STATE);
        CONJURE_CHECK_ERROR(client.send_raw("{}"), error_code::INVALID_STATE);
        CONJURE_CHECK_ERROR(client.receive(), error_code::INVALID_STATE);
    }

    TEST_CASE("protocol_client - a server that never answers fails with TIMEOUT") {
        protocol_client client(make_channel([](std::string_view, std::vector<std::string> &) {}));

        CONJURE_CHECK_ERROR(client.send("session.ping"), error_code::TIMEOUT);
        CONJURE_CHECK_ERROR(client.receive(), error_code::TIMEOUT);
    }

    TEST_CASE("protocol_client - a server that only streams events fails with TIMEOUT and keeps the events") {
        protocol_client client(make_channel([](std::string_view, std::vector<std::string> &outLines) {
            outLines.push_back(R"({"event":"log","seq":1,"frame":1,"data":{}})");
            outLines.push_back(R"({"event":"log","seq":2,"frame":1,"data":{}})");
        }));

        CONJURE_CHECK_ERROR(client.send("session.ping"), error_code::TIMEOUT);
        CHECK(client.events().size() == 2);
    }

    TEST_CASE("protocol_client - a non-JSON line from the server is a PARSE_FAILURE") {
        protocol_client client(make_channel([](std::string_view, std::vector<std::string> &outLines) {
            outLines.push_back("hello there");
        }));

        CONJURE_CHECK_ERROR(client.send("session.ping"), error_code::PARSE_FAILURE);
    }

    TEST_CASE("protocol_client - responses for unknown ids are dropped and counted") {
        log_capture logs = {};
        protocol_client client(make_channel([](std::string_view, std::vector<std::string> &outLines) {
            outLines.push_back(R"({"id":999,"ok":true,"result":{}})");
            outLines.push_back(R"({"id":"text","ok":true,"result":{}})");
            outLines.push_back(R"({"id":1,"ok":true,"result":{"right":true}})");
        }));

        const result<json::document> reply = client.send("session.ping");
        CONJURE_REQUIRE_OK(reply);
        CHECK(reply->root()["result"]["right"].as_bool());
        CHECK(client.dropped_response_count() == 2);
        CHECK(logs.count(core::log_level::WARN) == 2);
    }

    TEST_CASE("protocol_client - receive buffers unsolicited events and returns responses") {
        std::unique_ptr<scripted_protocol_channel> channel = make_channel(&fake_server);
        scripted_protocol_channel &wire = *channel;
        protocol_client client(std::move(channel));

        wire.inject_line(R"({"event":"scene.loaded","seq":3,"frame":9,"data":{}})");
        wire.inject_line(R"({"id":77,"ok":true,"result":{}})");
        CHECK(wire.pending_count() == 2);

        const result<std::optional<json::document>> first = client.receive();
        CONJURE_REQUIRE_OK(first);
        CHECK_FALSE(first->has_value());
        REQUIRE(client.events().size() == 1);
        CHECK(client.events()[0].root()["event"].as_string() == "scene.loaded");

        const result<std::optional<json::document>> second = client.receive();
        CONJURE_REQUIRE_OK(second);
        REQUIRE(second->has_value());
        CHECK((*second)->root()["id"].as_u64() == 77);
        CHECK(wire.pending_count() == 0);
    }

    TEST_CASE("protocol_client - helpers read ok, error code and message defensively") {
        const json::document ok = json::object({{"id", 1}, {"ok", true}, {"result", json::object({})}});
        const json::document failed = json::object({
            {"id", 2},
            {"ok", false},
            {"error", json::object({{"code", "BUSY"}, {"message", "later"}})},
        });
        const json::document scalar = json::array({1, 2});
        const json::document empty = {};

        CHECK(protocol_client::response_ok(ok));
        CHECK_FALSE(protocol_client::response_ok(failed));
        CHECK_FALSE(protocol_client::response_ok(scalar));
        CHECK_FALSE(protocol_client::response_ok(empty));
        CHECK(protocol_client::error_code_of(failed) == "BUSY");
        CHECK(protocol_client::error_message_of(failed) == "later");
        CHECK(protocol_client::error_code_of(ok).empty());
        CHECK(protocol_client::error_code_of(scalar).empty());
        CHECK(protocol_client::is_event(json::object({{"event", "x"}}).root()));
        CHECK_FALSE(protocol_client::is_event(ok.root()));
        CHECK_FALSE(protocol_client::is_event(scalar.root()));
    }

    TEST_CASE("scripted_protocol_channel - records sent lines, drains queued lines after close, then fails") {
        scripted_protocol_channel channel([](std::string_view line, std::vector<std::string> &outLines) {
            outLines.push_back(std::string("echo:") + std::string(line));
        });

        CONJURE_REQUIRE_OK(channel.send_line("one"));
        CONJURE_REQUIRE_OK(channel.send_line("two"));
        REQUIRE(channel.sent_lines().size() == 2);
        CHECK(channel.sent_lines()[1] == "two");
        CHECK(channel.pending_count() == 2);
        CHECK(channel.is_open());

        channel.close();
        CHECK_FALSE(channel.is_open());
        CONJURE_CHECK_ERROR(channel.send_line("three"), error_code::INVALID_STATE);

        const result<std::string> first = channel.receive_line();
        CONJURE_REQUIRE_OK(first);
        CHECK(first.value() == "echo:one");
        const result<std::string> second = channel.receive_line();
        CONJURE_REQUIRE_OK(second);
        CHECK(second.value() == "echo:two");
        CONJURE_CHECK_ERROR(channel.receive_line(), error_code::INVALID_STATE);
    }

    TEST_CASE("scripted_protocol_channel - a channel without a handler queues nothing") {
        scripted_protocol_channel channel({});

        CONJURE_REQUIRE_OK(channel.send_line("ping"));
        CHECK(channel.pending_count() == 0);
        CONJURE_CHECK_ERROR(channel.receive_line(), error_code::TIMEOUT);
    }

    TEST_CASE("regression - a handler that sends through its own channel loses and duplicates no lines") {
        // A fake server that forwards: the handler for "outer" sends "inner" through the same channel while
        // the outer send is still collecting its lines.
        scripted_protocol_channel *self = nullptr;
        scripted_protocol_channel channel([&self](std::string_view line, std::vector<std::string> &outLines) {
            if (line == "outer") {
                outLines.emplace_back("outer:first");
                REQUIRE(self != nullptr);
                CONJURE_REQUIRE_OK(self->send_line("inner"));
                outLines.emplace_back("outer:second");
                return;
            }
            outLines.emplace_back(std::string(line) + ":reply");
        });
        self = &channel;

        CONJURE_REQUIRE_OK(channel.send_line("outer"));

        REQUIRE(channel.pending_count() == 3);
        REQUIRE(channel.sent_lines().size() == 2);
        CHECK(channel.sent_lines()[0] == "outer");
        CHECK(channel.sent_lines()[1] == "inner");
        // The inner call completes first, so its reply is queued before the outer call's lines.
        const result<std::string> first = channel.receive_line();
        const result<std::string> second = channel.receive_line();
        const result<std::string> third = channel.receive_line();
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);
        CONJURE_REQUIRE_OK(third);
        CHECK(first.value() == "inner:reply");
        CHECK(second.value() == "outer:first");
        CHECK(third.value() == "outer:second");
        CHECK(channel.pending_count() == 0);
    }
}
