#include "editor/core/user_paths.h"

#include "conjure/core/error.h"

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include <Windows.h>

#include <ShlObj.h>

#include <format>

namespace conjure::editor {

    core::result<std::filesystem::path> user_state_directory() {
        // The known folder, not the LOCALAPPDATA variable: it is there for a process started with a bare
        // environment as well. LocalAppData is the per-user folder that stays on this machine; what is kept here
        // is about paths of this machine.
        wchar_t *folder = nullptr;
        const HRESULT found = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &folder);
        if (FAILED(found) || folder == nullptr) {
            // The documentation asks for the free on failure as well; it accepts a null pointer.
            CoTaskMemFree(folder);
            return core::error(
              core::error_code::NOT_FOUND,
              std::format(
                "Windows names no LocalAppData folder for this user (error 0x{:08X})",
                static_cast<unsigned long>(found)
              )
            );
        }
        std::filesystem::path directory = std::filesystem::path(folder) / "Conjure";
        CoTaskMemFree(folder);
        return directory;
    }
}
