#include "conjure/testing/doctest.h"

#include "conjure/rhi/native_window.h"

#include <string_view>
#include <type_traits>

using namespace conjure;
using namespace conjure::rhi;

TEST_SUITE("rhi/native_window") {

    TEST_CASE("native_window - a default handle is headless") {
        const native_window window = {};
        CHECK(window.kind == native_window_kind::NONE);
        CHECK(window.window == nullptr);
        CHECK(window.display == nullptr);
        CHECK(window.x11Window == 0);
        CHECK(window.is_headless());
    }

    TEST_CASE("native_window - any other kind is not headless") {
        native_window window = {};
        window.kind = native_window_kind::WINDOWS;
        CHECK_FALSE(window.is_headless());
        window.kind = native_window_kind::X11;
        window.x11Window = 42;
        CHECK_FALSE(window.is_headless());
    }

    TEST_CASE("native_window_kind - to_string names every kind and out-of-range values empty") {
        static_assert(std::is_same_v<decltype(to_string(native_window_kind::NONE)), std::string_view>);
        CHECK(to_string(native_window_kind::NONE) == "NONE");
        CHECK(to_string(native_window_kind::WINDOWS) == "WINDOWS");
        CHECK(to_string(native_window_kind::COCOA) == "COCOA");
        CHECK(to_string(native_window_kind::X11) == "X11");
        CHECK(to_string(native_window_kind::WAYLAND) == "WAYLAND");
        CHECK(to_string(static_cast<native_window_kind>(99)).empty());
    }

    TEST_CASE("native_window - kind is one byte, x11Window is 64 bits and the kind values follow 11_platform.md 2.4") {
        // 11_platform.md 2.4 layout. The engine converts platform::native_window_handle with a switch on the
        // kind and a field-by-field copy (engine/src/engine/subsystems/rhi_subsystem.cpp), so these asserts
        // pin rhi's own layout, not platform's.
        static_assert(sizeof(native_window::kind) == 1);
        static_assert(sizeof(native_window::x11Window) == 8);
        static_assert(static_cast<u8>(native_window_kind::WINDOWS) == 1);
        static_assert(static_cast<u8>(native_window_kind::COCOA) == 2);
        static_assert(static_cast<u8>(native_window_kind::X11) == 3);
        static_assert(static_cast<u8>(native_window_kind::WAYLAND) == 4);
        CHECK(true);
    }
}
