#include "conjure/testing/doctest.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/undo_stack.h"

#include <string>
#include <string_view>

// undo_stack.h: recording, cursor movement, transactions, coalescing and the budget.

using namespace conjure;
using namespace conjure::editor;

namespace {
    command_request replay_of(std::string_view name, i64 value = 0) {
        command_request request = {};
        request.name = std::string(name);
        request.args = core::json::object({{"value", value}});
        return request;
    }

    command_inverse inverse_of(std::string_view name, i64 value = 0) {
        command_inverse inverse = {};
        inverse.name = std::string(name);
        inverse.args = core::json::object({{"value", value}});
        return inverse;
    }

    void record_simple(undo_stack &stack, std::string_view label, u32 clientId = 1, u64 coalesceKey = 0) {
        stack.record(replay_of("test.set"), inverse_of("test.set"), label, clientId, coalesceKey);
    }
}

TEST_SUITE("editor/undo_stack") {

    TEST_CASE("undo_stack - records entries and pops them for undo in reverse, then redo forwards") {
        undo_stack stack = {};
        CHECK_FALSE(stack.can_undo());
        CHECK_FALSE(stack.can_redo());
        record_simple(stack, "first");
        record_simple(stack, "second");
        CHECK(stack.size() == 2);
        CHECK(stack.cursor() == 2);
        CHECK(stack.can_undo());
        std::optional<undo_entry> popped = stack.pop_for_undo();
        REQUIRE(popped.has_value());
        CHECK(popped->label == "second");
        CHECK(stack.cursor() == 1);
        CHECK(stack.can_redo());
        popped = stack.pop_for_undo();
        REQUIRE(popped.has_value());
        CHECK(popped->label == "first");
        CHECK_FALSE(stack.can_undo());
        CHECK_FALSE(stack.pop_for_undo().has_value());
        popped = stack.pop_for_redo();
        REQUIRE(popped.has_value());
        CHECK(popped->label == "first");
        CHECK(stack.cursor() == 1);
    }

    TEST_CASE("undo_stack - a new record after an undo discards the redo tail") {
        undo_stack stack = {};
        record_simple(stack, "a");
        record_simple(stack, "b");
        (void) stack.pop_for_undo();
        CHECK(stack.can_redo());
        record_simple(stack, "c");
        CHECK_FALSE(stack.can_redo());
        CHECK(stack.size() == 2);
        const std::vector<const undo_entry *> entries = stack.entries();
        REQUIRE(entries.size() == 2);
        CHECK(entries[0]->label == "a");
        CHECK(entries[1]->label == "c");
    }

    TEST_CASE("undo_stack - a transaction groups records into one entry with inverses in reverse order") {
        undo_stack stack = {};
        CONJURE_REQUIRE_OK(stack.begin_transaction("Move", 1));
        CHECK(stack.has_transaction(1));
        stack.record(replay_of("test.set", 1), inverse_of("test.set", 10), "set 1", 1, 0);
        stack.record(replay_of("test.set", 2), inverse_of("test.set", 20), "set 2", 1, 0);
        CHECK(stack.size() == 0);
        CONJURE_REQUIRE_OK(stack.end_transaction(1));
        CHECK_FALSE(stack.has_transaction(1));
        REQUIRE(stack.size() == 1);
        const undo_entry *entry = stack.entries().front();
        CHECK(entry->label == "Move");
        REQUIRE(entry->undo.size() == 2);
        REQUIRE(entry->redo.size() == 2);
        // Inverses are stored in recording order; history.undo applies them from the back.
        CHECK(entry->undo[0].args.root()["value"].as_i64() == 10);
        CHECK(entry->undo[1].args.root()["value"].as_i64() == 20);
        CHECK(entry->redo[0].args.root()["value"].as_i64() == 1);
        CHECK(entry->redo[1].args.root()["value"].as_i64() == 2);
        CHECK(entry->single_command_name().empty());
    }

    TEST_CASE("undo_stack - transactions are isolated per client and misuse is INVALID_REQUEST") {
        undo_stack stack = {};
        CONJURE_REQUIRE_OK(stack.begin_transaction("one", 1));
        CONJURE_REQUIRE_OK(stack.begin_transaction("two", 2));
        CONJURE_CHECK_ERROR(stack.begin_transaction("again", 1), protocol_error_code::INVALID_REQUEST);
        record_simple(stack, "client 1 edit", 1);
        record_simple(stack, "client 3 edit", 3);
        CHECK(stack.size() == 1);
        CONJURE_REQUIRE_OK(stack.end_transaction(2));
        CHECK(stack.size() == 1);
        CONJURE_REQUIRE_OK(stack.end_transaction(1));
        CHECK(stack.size() == 2);
        CONJURE_CHECK_ERROR(stack.end_transaction(1), protocol_error_code::INVALID_REQUEST);
        CONJURE_CHECK_ERROR(stack.end_transaction(9), protocol_error_code::INVALID_REQUEST);
    }

    TEST_CASE("undo_stack - an empty transaction leaves no entry") {
        undo_stack stack = {};
        CONJURE_REQUIRE_OK(stack.begin_transaction("nothing", 1));
        CONJURE_REQUIRE_OK(stack.end_transaction(1));
        CHECK(stack.size() == 0);
    }

    TEST_CASE("undo_stack - the same coalesce key keeps the first inverse and the last replay") {
        undo_stack stack = {};
        stack.record(replay_of("test.set", 1), inverse_of("test.set", 0), "drag", 1, 77);
        stack.record(replay_of("test.set", 2), inverse_of("test.set", 1), "drag", 1, 77);
        stack.record(replay_of("test.set", 3), inverse_of("test.set", 2), "drag", 1, 77);
        REQUIRE(stack.size() == 1);
        const undo_entry *entry = stack.entries().front();
        REQUIRE(entry->undo.size() == 1);
        REQUIRE(entry->redo.size() == 1);
        CHECK(entry->undo[0].args.root()["value"].as_i64() == 0);
        CHECK(entry->redo[0].args.root()["value"].as_i64() == 3);
        CHECK(entry->single_command_name() == "test.set");
    }

    TEST_CASE("regression - records inside an open transaction coalesce by key like top-level ones") {
        undo_stack stack = {};
        CONJURE_REQUIRE_OK(stack.begin_transaction("drag", 1));
        // A gizmo drag: the same command with the same key every frame, bracketed by the transaction.
        for (i64 frame = 1; frame <= 300; ++frame) {
            stack.record(
              replay_of("component.set_many", frame),
              inverse_of("component.set_many", frame - 1),
              "",
              1,
              42
            );
        }
        // A different key or command inside the same transaction is a separate step.
        stack.record(replay_of("entity.rename", 1), inverse_of("entity.rename", 0), "", 1, 43);
        stack.record(replay_of("component.set_many", 7), inverse_of("component.set_many", 6), "", 1, 0);
        CONJURE_REQUIRE_OK(stack.end_transaction(1));
        REQUIRE(stack.size() == 1);
        const undo_entry *entry = stack.entries().front();
        CHECK(entry->label == "drag");
        REQUIRE(entry->undo.size() == 3);
        REQUIRE(entry->redo.size() == 3);
        CHECK(entry->undo[0].args.root()["value"].as_i64() == 0);
        CHECK(entry->redo[0].args.root()["value"].as_i64() == 300);
        CHECK(entry->redo[0].coalesceKey == 42);
        CHECK(entry->redo[1].name == "entity.rename");
        CHECK(entry->redo[2].coalesceKey == 0);
    }

    TEST_CASE("undo_stack - coalescing never crosses command names, clients or a zero key") {
        undo_stack stack = {};
        stack.record(replay_of("test.set"), inverse_of("test.set"), "a", 1, 5);
        stack.record(replay_of("test.other"), inverse_of("test.other"), "b", 1, 5);
        stack.record(replay_of("test.other"), inverse_of("test.other"), "c", 2, 5);
        stack.record(replay_of("test.other"), inverse_of("test.other"), "d", 2, 0);
        stack.record(replay_of("test.other"), inverse_of("test.other"), "e", 2, 0);
        CHECK(stack.size() == 5);
    }

    TEST_CASE("undo_stack - the byte budget evicts the oldest entries") {
        undo_stack stack = {};
        stack.set_byte_budget(1);
        record_simple(stack, "a");
        record_simple(stack, "b");
        // A single entry above the budget stays; the older one went.
        CHECK(stack.size() == 1);
        CHECK(stack.entries().front()->label == "b");
        CHECK(stack.cursor() == 1);
        CHECK(stack.byte_size() == stack.entries().front()->byteSize);
        CHECK(undo_stack::measure(*stack.entries().front()) == stack.entries().front()->byteSize);
    }

    TEST_CASE("undo_stack - the depth cap evicts the oldest entries") {
        undo_stack stack = {};
        stack.set_max_depth(3);
        for (int index = 0; index < 5; ++index) {
            record_simple(stack, "entry");
        }
        CHECK(stack.size() == 3);
        CHECK(stack.cursor() == 3);
        CHECK(stack.max_depth() == 3);
    }

    TEST_CASE("undo_stack - a frozen stack ignores records and pushes until it thaws") {
        undo_stack stack = {};
        stack.set_frozen(true);
        CHECK(stack.frozen());
        record_simple(stack, "ignored while frozen");
        CHECK(stack.size() == 0);
        undo_entry entry = {};
        entry.label = "pushed";
        entry.undo.push_back(inverse_of("test.set"));
        entry.redo.push_back(replay_of("test.set"));
        stack.push(std::move(entry));
        CHECK(stack.size() == 0);
        stack.set_frozen(false);
        record_simple(stack, "after thaw");
        CHECK(stack.size() == 1);
        stack.clear();
        CHECK(stack.size() == 0);
        CHECK(stack.cursor() == 0);
        CHECK(stack.byte_size() == 0);
    }

    TEST_CASE("undo_stack - discard_at_cursor drops the entry a failed undo or redo left behind") {
        undo_stack stack = {};
        record_simple(stack, "a");
        record_simple(stack, "b");
        (void) stack.pop_for_undo();
        CHECK(stack.discard_at_cursor(true));
        CHECK(stack.size() == 1);
        CHECK_FALSE(stack.can_redo());
        (void) stack.pop_for_undo();
        (void) stack.pop_for_redo();
        CHECK(stack.discard_at_cursor(false));
        CHECK(stack.size() == 0);
        CHECK_FALSE(stack.discard_at_cursor(false));
    }

    TEST_CASE("transaction_scope - ends the transaction when it leaves scope") {
        undo_stack stack = {};
        {
            transaction_scope scope(stack, "scoped", 4);
            CHECK(scope.active());
            record_simple(stack, "x", 4);
            CHECK(stack.size() == 0);
        }
        CHECK(stack.size() == 1);
        CHECK_FALSE(stack.has_transaction(4));
    }
}
