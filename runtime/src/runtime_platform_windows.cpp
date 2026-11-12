// Windows backend of runtime_platform.h: the wide command line, GetModuleFileNameW, the standard-error and
// standard-output handles and MessageBoxW.

#include "runtime_platform.h"

#include "conjure/core/string_utils.h"

#include <Windows.h>
#include <shellapi.h>

#include <array>
#include <cwchar>
#include <system_error>

namespace {

    [[nodiscard]] std::string utf8_of(std::wstring_view wide) {
        // wchar_t and char16_t are both UTF-16 code units here; the copy keeps the conversion free of casts.
        const std::u16string units(wide.begin(), wide.end());
        return conjure::core::str::from_utf16(units);
    }

    [[nodiscard]] std::wstring wide_of(std::string_view utf8) {
        const std::u16string units = conjure::core::str::to_utf16(utf8);
        return std::wstring(units.begin(), units.end());
    }

    /// <summary>
    /// True when the standard handle ('STD_ERROR_HANDLE' or 'STD_OUTPUT_HANDLE') leads somewhere.
    /// </summary>
    [[nodiscard]] bool has_standard_handle(DWORD which) {
        const HANDLE handle = GetStdHandle(which);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        // A GUI-subsystem process started from a terminal inherits a console handle without a console behind it;
        // GetFileType reports it as unknown, exactly like no handle at all.
        return GetFileType(handle) != FILE_TYPE_UNKNOWN;
    }

    void show_dialog(std::string_view title, std::string_view text, UINT icon) {
        const std::wstring wideTitle = wide_of(title);
        const std::wstring wideText = wide_of(text);
        MessageBoxW(nullptr, wideText.c_str(), wideTitle.c_str(), MB_OK | icon | MB_SETFOREGROUND | MB_TASKMODAL);
    }
}

std::vector<std::string> conjure::runtime::utf8_arguments(int argc, char **argv) {
    std::vector<std::string> arguments = {};
    int count = 0;
    LPWSTR *wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (wide == nullptr) {
        // Out of memory or no command line at all: the narrow arguments are all there is.
        for (int index = 1; index < argc; ++index) {
            arguments.emplace_back(argv[index]);
        }
        return arguments;
    }
    arguments.reserve(count > 1 ? static_cast<size_t>(count) - 1 : 0);
    for (int index = 1; index < count; ++index) {
        arguments.push_back(utf8_of(std::wstring_view(wide[index], std::wcslen(wide[index]))));
    }
    LocalFree(wide);
    return arguments;
}

std::filesystem::path conjure::runtime::executable_directory() {
    std::array<wchar_t, MAX_PATH> buffer = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        std::error_code ignored = {};
        return std::filesystem::current_path(ignored);
    }
    return std::filesystem::path(buffer.data(), buffer.data() + length).parent_path();
}

bool conjure::runtime::has_standard_error() {
    return has_standard_handle(STD_ERROR_HANDLE);
}

bool conjure::runtime::has_standard_output() {
    return has_standard_handle(STD_OUTPUT_HANDLE);
}

void conjure::runtime::show_error_dialog(std::string_view title, std::string_view text) {
    show_dialog(title, text, MB_ICONERROR);
}

void conjure::runtime::show_info_dialog(std::string_view title, std::string_view text) {
    show_dialog(title, text, MB_ICONINFORMATION);
}
