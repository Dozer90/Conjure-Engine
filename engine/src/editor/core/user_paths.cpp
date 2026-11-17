#include "editor/core/user_paths.h"

namespace conjure::editor {

    namespace {
        /// <summary>
        /// 'folder' without its trailing separators when it is an absolute POSIX path, "" otherwise. The root
        /// itself becomes "", which the callers extend with a path that starts with a separator.
        /// </summary>
        [[nodiscard]] std::string_view absolute_base(std::string_view folder, bool &outAbsolute) {
            outAbsolute = !folder.empty() && folder.front() == '/';
            if (!outAbsolute) {
                return {};
            }
            while (!folder.empty() && folder.back() == '/') {
                folder.remove_suffix(1);
            }
            return folder;
        }
    }

    std::string posix_user_state_directory(std::string_view xdgStateHome, std::string_view home, bool macosLayout) {
        bool homeAbsolute = false;
        const std::string_view homeBase = absolute_base(home, homeAbsolute);
        if (macosLayout) {
            if (!homeAbsolute) {
                return "";
            }
            return std::string(homeBase) + "/Library/Application Support/Conjure";
        }
        bool stateAbsolute = false;
        const std::string_view stateBase = absolute_base(xdgStateHome, stateAbsolute);
        if (stateAbsolute) {
            return std::string(stateBase) + "/conjure";
        }
        if (!homeAbsolute) {
            return "";
        }
        return std::string(homeBase) + "/.local/state/conjure";
    }
}
