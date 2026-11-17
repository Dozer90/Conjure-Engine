#include "editor/core/user_paths.h"

#include "conjure/core/error.h"
#include "conjure/core/platform_detect.h"

#include <cstdlib>
#include <string>
#include <string_view>

namespace conjure::editor {

    namespace {
        /// <summary>
        /// The value of an environment variable, "" when it is not set.
        /// </summary>
        [[nodiscard]] std::string_view environment_value(const char *name) {
            const char *value = std::getenv(name);
            if (value == nullptr) {
                return {};
            }
            return std::string_view(value);
        }
    }

    core::result<std::filesystem::path> user_state_directory() {
        const bool macosLayout = CONJURE_PLATFORM_MACOS != 0;
        const std::string directory = posix_user_state_directory(
          environment_value("XDG_STATE_HOME"),
          environment_value("HOME"),
          macosLayout
        );
        if (directory.empty()) {
            return core::error(
              core::error_code::NOT_FOUND,
              macosLayout
                ? "the environment variable HOME names no absolute folder"
                : "neither XDG_STATE_HOME nor HOME names an absolute folder"
            );
        }
        return std::filesystem::path(directory);
    }
}
