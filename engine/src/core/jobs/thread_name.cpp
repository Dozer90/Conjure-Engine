#include "core/jobs/thread_name.h"

#include <algorithm>

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
    // UTF-16 units the first 'bytes' bytes of 'text' convert to. Invalid UTF-8 sequences count as U+FFFD
    // (flags 0), so 0 means an empty prefix or a failed call.
    int wide_length(std::string_view text, usize bytes) noexcept {
        if (bytes == 0) {
            return 0;
        }
        return MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(bytes), nullptr, 0);
    }

    // Longest prefix of 'text' that converts into at most MAX_WINDOWS_THREAD_NAME_LENGTH units; the
    // cut never splits a code point because continuation bytes (10xxxxxx) are stepped over.
    usize fitting_prefix(std::string_view text, int &length) noexcept {
        usize bytes = text.size();
        length = wide_length(text, bytes);
        while (length > static_cast<int>(MAX_WINDOWS_THREAD_NAME_LENGTH) && bytes > 0) {
            do {
                --bytes;
            } while (bytes > 0 && (static_cast<unsigned char>(text[bytes]) & 0xC0) == 0x80);
            length = wide_length(text, bytes);
        }
        return bytes;
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

prepared_thread_name conjure::core::jobs::detail::prepare_thread_name(std::string_view name) noexcept {
    prepared_thread_name prepared = {};
    if (name.empty()) {
        return prepared;
    }

    int length = 0;
    const usize bytes = fitting_prefix(name, length);
    if (length <= 0) {
        return prepared;
    }
    MultiByteToWideChar(CP_UTF8, 0, name.data(), static_cast<int>(bytes), prepared.characters.data(), length);
    return prepared;
}

void conjure::core::jobs::detail::set_current_thread_name(const prepared_thread_name &name) noexcept {
    static_cast<void>(SetThreadDescription(GetCurrentThread(), name.characters.data()));
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

prepared_thread_name conjure::core::jobs::detail::prepare_thread_name(std::string_view name) noexcept {
    // Linux limits names to 15 bytes; macOS allows 63. Both accept the shorter buffer, and a
    // single limit keeps the behaviour predictable across the two. The cut steps back over
    // continuation bytes (10xxxxxx), so it never splits a code point.
    prepared_thread_name prepared = {};
    usize length = std::min(name.size(), MAX_POSIX_THREAD_NAME_LENGTH);
    while (length > 0 && length < name.size() && (static_cast<unsigned char>(name[length]) & 0xC0) == 0x80) {
        --length;
    }
    std::copy_n(name.data(), length, prepared.characters.data());
    return prepared;
}

void conjure::core::jobs::detail::set_current_thread_name(const prepared_thread_name &name) noexcept {
    #if CONJURE_PLATFORM_MACOS
        static_cast<void>(pthread_setname_np(name.characters.data()));
    #else
        static_cast<void>(pthread_setname_np(pthread_self(), name.characters.data()));
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

/*===========================================================================*/
/*===========================================================================*/
// Shared

void conjure::core::jobs::detail::set_current_thread_name(std::string_view name) noexcept {
    set_current_thread_name(prepare_thread_name(name));
}
