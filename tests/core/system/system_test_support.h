#pragma once

// Helpers shared by the core/system tests: running conjure_process_child, the helper program
// tests/core/system/CMakeLists.txt builds next to the test executable. test_process.cpp defines them.

#include "conjure/core/process.h"

#include <filesystem>
#include <string>
#include <vector>

namespace process_child_support {

    /// <summary>
    /// conjure_process_child, built next to the test executable.
    /// </summary>
    [[nodiscard]] const std::filesystem::path &child_path();

    /// <summary>
    /// A process_desc that runs conjure_process_child with 'arguments'.
    /// </summary>
    [[nodiscard]] conjure::core::process_desc child(std::vector<std::string> arguments);

    /// <summary>
    /// 'text' without the '\r' the Windows runtime adds to every line a child prints in text mode.
    /// </summary>
    [[nodiscard]] std::string without_carriage_returns(std::string text);
}
