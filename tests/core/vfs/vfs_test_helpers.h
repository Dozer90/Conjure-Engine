#pragma once

// Helpers shared by the vfs test files.


#include "conjure/testing/doctest.h"

#include "conjure/core/error.h"
#include "conjure/core/result.h"
#include "conjure/core/vfs/file_system.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace conjure::testing::vfs_helpers {

    inline std::vector<std::byte> to_bytes(std::string_view text) {
        const std::span<const std::byte> view = std::as_bytes(std::span<const char>(text.data(), text.size()));
        return std::vector<std::byte>(view.begin(), view.end());
    }

    inline std::string to_string(std::span<const std::byte> bytes) {
        return std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    }

    /// <summary>
    /// The generic code of a failed result, or NONE for a successful one (so a mismatch prints
    /// something readable instead of asserting inside get_error).
    /// </summary>
    template <typename ValueType>
    core::error_code code_of(const core::result<ValueType> &value) {
        if (value.has_value()) {
            return core::error_code::NONE;
        }
        return value.get_error().generic_code();
    }

    /// <summary>
    /// Collects every entry an enumerate call reports, in callback order.
    /// </summary>
    struct enumeration {
        std::vector<std::string> paths = {};
        std::vector<core::vfs::file_stat> stats = {};
    };

    inline core::result<enumeration> enumerate_all(
      core::vfs::file_system &fileSystem,
      std::string_view directory,
      core::vfs::enumerate_flags flags
    ) {
        enumeration collected = {};
        const core::result<void> outcome = fileSystem.enumerate(
          directory,
          flags,
          [&collected](std::string_view path, const core::vfs::file_stat &stat) {
              collected.paths.emplace_back(path);
              collected.stats.push_back(stat);
          }
        );
        if (!outcome.has_value()) {
            return outcome.get_error();
        }
        return collected;
    }
}
