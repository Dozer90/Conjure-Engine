#include "conjure/testing/doctest.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/command_desc.h"

#include <type_traits>

// command_desc.h: the ABI-stable command descriptor modules register and the command flags.

using namespace conjure;
using namespace conjure::editor;

namespace {
    core::result<core::json::document> echo_handler(const core::json::value &args, void *userData) {
        int *calls = static_cast<int *>(userData);
        ++*calls;
        return core::json::document::from_value(args);
    }
}

TEST_SUITE("editor/command_desc") {

    TEST_CASE("command_desc - is trivially copyable and names the extension point") {
        static_assert(std::is_trivially_copyable_v<command_desc>);
        CHECK(command_desc::EXTENSION_POINT == core::string_id("editor.command"));
        const command_desc desc = {};
        CHECK(desc.name == nullptr);
        CHECK(desc.flags == command_flags::NONE);
        CHECK(desc.handler == nullptr);
    }

    TEST_CASE("command_flags - combine and test as bit flags") {
        const command_flags flags = command_flags::MUTATES | command_flags::ASYNC;
        CHECK(has_flag(flags, command_flags::MUTATES));
        CHECK(has_flag(flags, command_flags::ASYNC));
        CHECK_FALSE(has_flag(flags, command_flags::NO_UNDO));
        CHECK(static_cast<u16>(command_flags::NO_UNDO) == 256);
        CHECK(static_cast<u16>(command_flags::NOT_IN_BATCH) == 128);
    }

    TEST_CASE("command_flags - ON_DISK is its own bit and can be taken out of a set") {
        CHECK(static_cast<u16>(command_flags::ON_DISK) == 512);
        const command_flags file = command_flags::MUTATES | command_flags::REQUIRES_PROJECT | command_flags::ON_DISK;
        CHECK(has_flag(file, command_flags::ON_DISK));
        CHECK_FALSE(has_flag(file, command_flags::NO_UNDO));
        const command_flags stripped = file & ~command_flags::ON_DISK;
        CHECK_FALSE(has_flag(stripped, command_flags::ON_DISK));
        CHECK(stripped == (command_flags::MUTATES | command_flags::REQUIRES_PROJECT));
    }

    TEST_CASE("command_flags - READ_ONLY is its own bit, above every other flag") {
        CHECK(static_cast<u16>(command_flags::READ_ONLY) == 1024);
        const command_flags query = command_flags::REQUIRES_SCENE | command_flags::READ_ONLY;
        CHECK(has_flag(query, command_flags::READ_ONLY));
        CHECK_FALSE(has_flag(query, command_flags::MUTATES));
        CHECK((query & ~command_flags::READ_ONLY) == command_flags::REQUIRES_SCENE);
    }

    TEST_CASE("command_desc - the C handler receives the arguments and the user data") {
        int calls = 0;
        command_desc desc = {};
        desc.name = "test.echo";
        desc.summary = "echoes";
        desc.handler = &echo_handler;
        desc.userData = &calls;
        const core::json::document args = core::json::object({{"x", 1}});
        const core::result<core::json::document> result = desc.handler(args.root(), desc.userData);
        CONJURE_REQUIRE_OK(result);
        CHECK(result.value().root()["x"].as_i64() == 1);
        CHECK(calls == 1);
    }
}
