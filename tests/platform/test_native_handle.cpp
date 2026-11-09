#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "platform_test_support.h"

#include "conjure/platform/native_handle.h"
#include "conjure/platform/window.h"

#include <memory>
#include <type_traits>

using namespace conjure;
using namespace conjure::platform;
using namespace conjure::platform::test_support;

TEST_SUITE("platform/native_handle") {

    TEST_CASE("native_window_handle - defaults are NONE with null pointers") {
        static_assert(std::is_trivially_copyable_v<native_window_handle>);
        const native_window_handle handle = {};
        CHECK(handle.kind == native_handle_kind::NONE);
        CHECK(handle.window == nullptr);
        CHECK(handle.display == nullptr);
        CHECK(handle.x11Window == 0);
    }

    TEST_CASE("native_handle_kind - enumerators are stable") {
        CHECK(static_cast<u8>(native_handle_kind::NONE) == 0);
        CHECK(static_cast<u8>(native_handle_kind::WINDOWS) == 1);
        CHECK(static_cast<u8>(native_handle_kind::COCOA) == 2);
        CHECK(static_cast<u8>(native_handle_kind::X11) == 3);
        CHECK(static_cast<u8>(native_handle_kind::WAYLAND) == 4);
    }

    TEST_CASE("native_window_handle - equality compares every field") {
        native_window_handle first = {};
        native_window_handle second = {};
        CHECK(first == second);
        int marker = 0;
        second.window = &marker;
        CHECK_FALSE(first == second);
        second.window = nullptr;
        second.x11Window = 42;
        CHECK_FALSE(first == second);
        second.x11Window = 0;
        second.kind = native_handle_kind::X11;
        CHECK_FALSE(first == second);
    }

    TEST_CASE("native_window_handle - the null window has no native handle") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();

        const native_window_handle handle = created->native_handle();
        CHECK(handle.kind == native_handle_kind::NONE);
        CHECK(handle.window == nullptr);
        CHECK(handle.display == nullptr);
        CHECK(handle.x11Window == 0);
        CHECK(handle == native_window_handle());
    }
}
