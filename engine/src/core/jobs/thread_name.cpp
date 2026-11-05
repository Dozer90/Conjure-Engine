#include "core/jobs/thread_name.h"

#include "conjure/core/platform_detect.h"

#include <algorithm>
#include <array>

#if CONJURE_PLATFORM_WINDOWS
    #include <Windows.h>
#else
    #include <pthread.h>
#endif

using namespace conjure;
using namespace conjure::core::jobs::detail;

/*===========================================================================*/
/*===========================================================================*/
// Windows

#if CONJURE_PLATFORM_WINDOWS

namespace {
    std::wstring to_wide(std::string_view text) {
        if (text.empty()) {
            return {};
        }
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0) {
            return {};
        }
        std::wstring wide(static_cast<usize>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
        return wide;
    }

    std::string to_utf8(const wchar_t *text) {
        if (text == nullptr || *text == L'\0') {
            return {};
        }
        const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        if (length <= 1) {
            return {};
        }
        // The returned length includes the terminator, which std::string stores on its own.
        std::string utf8(static_cast<usize>(length - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), length, nullptr, nullptr);
        return utf8;
    }
}

void conjure::core::jobs::detail::set_current_thread_name(std::string_view name) noexcept {
    const std::wstring wide = to_wide(name);
    static_cast<void>(SetThreadDescription(GetCurrentThread(), wide.c_str()));
}

std::string conjure::core::jobs::detail::current_thread_name() {
    PWSTR description = nullptr;
    if (FAILED(GetThreadDescription(GetCurrentThread(), &description)) || description == nullptr) {
        return {};
    }
    std::string name = to_utf8(description);
    LocalFree(description);
    return name;
}

/*===========================================================================*/
/*===========================================================================*/
// POSIX (Linux and macOS)

#else

namespace {
    // Linux limits names to 15 bytes; macOS allows 63. Both accept the shorter buffer, and a
    // single limit keeps the behaviour predictable across the two.
    constexpr usize NAME_BUFFER_SIZE = MAX_POSIX_THREAD_NAME_LENGTH + 1;
}

void conjure::core::jobs::detail::set_current_thread_name(std::string_view name) noexcept {
    std::array<char, NAME_BUFFER_SIZE> buffer = {};
    const usize length = std::min(name.size(), MAX_POSIX_THREAD_NAME_LENGTH);
    std::copy_n(name.data(), length, buffer.data());
    #if CONJURE_PLATFORM_MACOS
        static_cast<void>(pthread_setname_np(buffer.data()));
    #else
        static_cast<void>(pthread_setname_np(pthread_self(), buffer.data()));
    #endif
}

std::string conjure::core::jobs::detail::current_thread_name() {
    std::array<char, 64> buffer = {};
    if (pthread_getname_np(pthread_self(), buffer.data(), buffer.size()) != 0) {
        return {};
    }
    return std::string(buffer.data());
}

#endif
