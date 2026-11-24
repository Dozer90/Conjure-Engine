#include "conjure/engine/export.h"

#include "conjure/core/error.h"
#include "conjure/core/process.h"
#include "conjure/core/string_utils.h"

#include <array>
#include <charconv>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace conjure;
using namespace conjure::engine;

namespace {

    // engine/src/engine/CMakeLists.txt reads sdk.version of global.json at the repository root into the definition.
    constexpr std::string_view REQUIRED_VERSION = CONJURE_DOTNET_SDK_VERSION;

    // Microsoft's official download page of a .NET release; the release ("10.0") completes it.
    constexpr std::string_view DOWNLOAD_PAGE_PREFIX = "https://dotnet.microsoft.com/download/dotnet/";

    // The most of dotnet's own output a message quotes: its explanation of a missing SDK is a few lines long.
    constexpr usize QUOTED_OUTPUT_LIMIT = 1000;

    /// <summary>
    /// The three numbers a version starts with and whether a prerelease suffix ('-') follows them.
    /// </summary>
    struct sdk_version {
        std::array<u32, 3> numbers = {};
        bool prerelease = false;
    };

    /// <summary>
    /// Reads "<major>.<minor>.<patch>" at the start of 'text', optionally followed by a '-' (prerelease) or '+'
    /// (build metadata) suffix; false for anything else.
    /// </summary>
    [[nodiscard]] bool parse_version(std::string_view text, sdk_version &parsed) noexcept {
        const char *cursor = text.data();
        const char *const end = text.data() + text.size();
        for (usize index = 0; index < parsed.numbers.size(); ++index) {
            if (index > 0) {
                if (cursor == end || *cursor != '.') {
                    return false;
                }
                ++cursor;
            }
            // from_chars accepts neither a sign nor whitespace, so each number is digits only.
            const std::from_chars_result read = std::from_chars(cursor, end, parsed.numbers[index]);
            if (read.ec != std::errc{}) {
                return false;
            }
            cursor = read.ptr;
        }
        if (cursor == end) {
            return true;
        }
        if (*cursor == '-') {
            parsed.prerelease = true;
            return true;
        }
        return *cursor == '+';
    }

    /// <summary>
    /// The major and minor version of the required SDK ("10.0"), which names its release.
    /// </summary>
    [[nodiscard]] std::string_view required_release() noexcept {
        const usize firstDot = REQUIRED_VERSION.find('.');
        const usize secondDot = REQUIRED_VERSION.find('.', firstDot + 1);
        return REQUIRED_VERSION.substr(0, secondDot);
    }

    /// <summary>
    /// The end of dotnet's output without surrounding whitespace, at most QUOTED_OUTPUT_LIMIT bytes.
    /// </summary>
    [[nodiscard]] std::string quoted_output(std::string_view output) {
        std::string_view text = core::str::trim(output);
        if (text.size() > QUOTED_OUTPUT_LIMIT) {
            text = text.substr(text.size() - QUOTED_OUTPUT_LIMIT);
        }
        return std::string(text);
    }

    [[nodiscard]] core::error missing_sdk(std::string_view detail) {
        return core::error(core::error_code::DEPENDENCY_FAILURE, missing_dotnet_sdk_message(detail));
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public functions

std::string_view conjure::engine::required_dotnet_sdk_version() noexcept {
    return REQUIRED_VERSION;
}

std::string conjure::engine::dotnet_sdk_download_page() {
    return std::string(DOWNLOAD_PAGE_PREFIX) + std::string(required_release());
}

bool conjure::engine::is_usable_dotnet_sdk_version(std::string_view version) noexcept {
    sdk_version found = {};
    if (!parse_version(core::str::trim(version), found)) {
        return false;
    }
    sdk_version required = {};
    if (!parse_version(REQUIRED_VERSION, required)) {
        return false;
    }
    if (found.numbers != required.numbers) {
        return found.numbers > required.numbers;
    }
    return !found.prerelease;
}

std::string conjure::engine::missing_dotnet_sdk_message(std::string_view detail) {
    return std::format(
      "no usable .NET SDK: {}. Building the game's managed code needs the .NET SDK {} or later (the version the "
      "engine's global.json names); install it from Microsoft's download page {}",
      detail,
      REQUIRED_VERSION,
      dotnet_sdk_download_page()
    );
}

core::result<std::string> conjure::engine::find_dotnet_sdk(
  std::string_view dotnet,
  const std::filesystem::path &workingDirectory,
  f64 timeoutSeconds
) {
    core::process_desc desc = {};
    desc.executable = std::string(dotnet);
    desc.arguments = {"--version"};
    desc.workingDirectory = workingDirectory;
    desc.timeoutSeconds = timeoutSeconds;
    desc.mergeStderr = true;
    // The same quiet environment as every other dotnet command of the engine: no telemetry, no first-run banner.
    desc.environment = {{"DOTNET_CLI_TELEMETRY_OPTOUT", "1"}, {"DOTNET_NOLOGO", "1"}};
    const core::result<core::process_result> ran = core::process::run(desc);
    if (!ran.has_value()) {
        return missing_sdk(std::format("'{}' could not be started ({})", dotnet, ran.get_error().message()));
    }
    const core::process_result &result = ran.value();
    if (result.timedOut) {
        return missing_sdk(std::format("'{} --version' did not answer within {} s", dotnet, timeoutSeconds));
    }
    if (result.exitCode != 0) {
        return missing_sdk(std::format(
          "'{} --version' exited with {}: {}",
          dotnet,
          result.exitCode,
          quoted_output(result.standardOutput)
        ));
    }
    // dotnet prints the version alone; a first-run notice, should one appear despite DOTNET_NOLOGO, comes before it.
    const std::vector<std::string_view> lines = core::str::split_lines(core::str::trim(result.standardOutput));
    const std::string_view version = lines.empty() ? std::string_view() : core::str::trim(lines.back());
    if (!is_usable_dotnet_sdk_version(version)) {
        return missing_sdk(std::format("'{} --version' reports the SDK '{}'", dotnet, version));
    }
    return std::string(version);
}
