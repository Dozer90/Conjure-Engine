// Windows backend of editor_arguments.h: the wide command line, converted to UTF-8.

#include "editor_arguments.h"

#include "conjure/core/string_utils.h"

#include <Windows.h>
#include <shellapi.h>

#include <cwchar>
#include <string_view>

namespace {

    [[nodiscard]] std::string utf8_of(std::wstring_view wide) {
        // wchar_t and char16_t are both UTF-16 code units here; the copy keeps the conversion free of casts.
        const std::u16string units(wide.begin(), wide.end());
        return conjure::core::str::from_utf16(units);
    }
}

std::vector<std::string> conjure::editor_app::utf8_arguments(int argc, char **argv) {
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
